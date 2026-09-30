#include "pageview.h"

#include "app/theme.h"
#include "mark/callouteditor.h"
#include "mark/markbar.h"
#include "mark/markpaint.h"
#include "mark/markshape.h"
#include "pdf/document.h"

#include <QKeyEvent>
#include <QPainter>
#include <algorithm>

namespace {

constexpr double kHandle = 5;
constexpr double kHandleReach = 9;
constexpr double kReach = 4;
constexpr double kTap = 4;
constexpr double kFrameMargin = 3;
constexpr QSizeF kShape(120, 80);
constexpr QSizeF kCallout(170, 56);
constexpr QPointF kCalloutOffset(90, -70);
constexpr QPointF kArrowSpan(70, -50);
constexpr double kImageShare = 0.5;
constexpr double kPixelsPerPoint = 96.0 / 72.0;

// Geometry spanned by a drag.
Mark spanned(Mark mark, QPointF from, QPointF to)
{
    const double aspect = mark.image.isNull() ? 1 : double(mark.image.width()) / mark.image.height();
    switch (mark.kind) {
    case MarkKind::Arrow:
        mark.tail = from;
        mark.tip = to;
        return mark;
    case MarkKind::Callout:
        mark.tip = from;
        mark.box = QRectF(QPointF(), kCallout);
        mark.box.moveCenter(to);
        return mark;
    case MarkKind::Image:
        mark.box = QRectF(from, QSizeF(to.x() - from.x(), (to.x() - from.x()) / aspect)).normalized();
        return mark;
    case MarkKind::Rectangle:
    case MarkKind::Ellipse:
    case MarkKind::Cloud:
        mark.box = QRectF(from, to).normalized();
        return mark;
    }
    return mark;
}

// Natural size within half page.
QSizeF fitted(const QImage &image, double pageWidth)
{
    const QSizeF natural = QSizeF(image.size()) / kPixelsPerPoint;
    return natural * std::min(1.0, pageWidth * kImageShare / natural.width());
}

// Default geometry for a click.
Mark placed(Mark mark, QPointF at, double pageWidth)
{
    switch (mark.kind) {
    case MarkKind::Arrow:
        return spanned(mark, at, at + kArrowSpan);
    case MarkKind::Callout:
        return spanned(mark, at, at + kCalloutOffset);
    case MarkKind::Image:
        mark.box = QRectF(QPointF(), fitted(mark.image, pageWidth));
        mark.box.moveCenter(at);
        return mark;
    case MarkKind::Rectangle:
    case MarkKind::Ellipse:
    case MarkKind::Cloud:
        mark.box = QRectF(QPointF(), kShape);
        mark.box.moveCenter(at);
        return mark;
    }
    return mark;
}

} // namespace

void PageView::armMark(const Mark &prototype)
{
    armed = prototype;
    ghost.reset();
    select(0);
    pick(0);
    setCursor(Qt::CrossCursor);
    setFocus();
}

void PageView::paintMarks(QPainter &painter, int index)
{
    if (!marks)
        return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setTransform(pageToView(index));
    for (int id : marks->onPage(doc->slot(index).id)) {
        Mark shown = *marks->find(id);
        if (callout->isVisible() && callout->mark() == id)
            shown.text.clear();
        paintMark(painter, shown);
    }
    if (sketch && sketch->index == index)
        paintMark(painter, sketch->mark);
    painter.restore();
}

void PageView::paintMarkSelection(QPainter &painter)
{
    const Mark *mark = picked && interactive() ? marks->find(picked) : nullptr;
    const int index = mark ? doc->indexOf(mark->page) : -1;
    if (index < 0 || callout->isVisible())
        return;
    const QTransform toView = pageToView(index);
    const QRectF area = extentOf(*mark).adjusted(-kFrameMargin, -kFrameMargin, kFrameMargin, kFrameMargin);
    painter.setPen(QPen(theme::accent, 1.5, Qt::DashLine));
    painter.setBrush(theme::faded(theme::accent, 12));
    painter.drawRect(toView.mapRect(area));
    painter.setPen(QPen(theme::accent, 2));
    painter.setBrush(Qt::white);
    for (const QPointF &handle : handlesOf(*mark))
        painter.drawEllipse(toView.map(handle), kHandle, kHandle);
}

std::optional<int> PageView::markHandleAt(QPointF pos) const
{
    const Mark *mark = picked && interactive() ? marks->find(picked) : nullptr;
    const int index = mark ? doc->indexOf(mark->page) : -1;
    if (index < 0)
        return std::nullopt;
    const std::vector<QPointF> handles = handlesOf(*mark);
    const QTransform toView = pageToView(index);
    for (size_t i = 0; i < handles.size(); ++i) {
        if (QLineF(pos, toView.map(handles[i])).length() < kHandleReach)
            return int(i);
    }
    return std::nullopt;
}

bool PageView::pressMark(QPointF pos)
{
    if (const std::optional<int> handle = markHandleAt(pos)) {
        markDrag = MarkDrag{picked, *handle, pos, *marks->find(picked)};
        return true;
    }
    const std::optional<Spot> spot = spotAt(pos);
    const int id = spot ? marks->hit(doc->slot(spot->index).id, spot->point, kReach / zoom.value()) : 0;
    if (!id)
        return false;
    pick(id);
    markDrag = MarkDrag{id, kBody, pos, *marks->find(id)};
    return true;
}

void PageView::dragMark(QPointF pos)
{
    marks->update(dragged(markDrag->start, markDrag->handle, (pos - markDrag->grab) / zoom.value()));
    syncMarks();
}

void PageView::startSketch(QPointF pos)
{
    const std::optional<Spot> spot = spotAt(pos);
    if (!spot)
        return;
    Mark mark = *armed;
    mark.page = doc->slot(spot->index).id;
    sketch = Sketch{spot->index, spot->point, pos, mark};
    drawSketch(pos);
}

void PageView::drawSketch(QPointF pos)
{
    sketch->mark = spanned(sketch->mark, sketch->from, pageToView(sketch->index).inverted().map(pos));
    update();
}

void PageView::finishSketch(QPointF pos)
{
    const Sketch done = *std::exchange(sketch, std::nullopt);
    armed.reset();
    unsetCursor();
    const bool tapped = QLineF(pos, done.press).length() < kTap;
    const double pageWidth = doc->slot(done.index).size.width();
    const int id = marks->add(tapped ? placed(done.mark, done.from, pageWidth) : done.mark);
    pick(id);
    emit toolFinished();
    if (done.mark.kind == MarkKind::Callout)
        editCallout(id);
}

void PageView::pick(int id)
{
    picked = id;
    if (id && selected)
        select(0);
    showMarkBar();
    update();
}

void PageView::editCallout(int id)
{
    const Mark *mark = marks->find(id);
    if (!mark || mark->kind != MarkKind::Callout)
        return;
    pick(id);
    markBar->hide();
    callout->open(*mark, zoom.value());
    syncMarks();
    update();
}

void PageView::showMarkBar()
{
    const Mark *mark = picked && interactive() && callout->isHidden() ? marks->find(picked) : nullptr;
    if (!mark) {
        markBar->hide();
        return;
    }
    markBar->present(*mark);
    syncMarks();
}

void PageView::syncMarks()
{
    const Mark *mark = picked ? marks->find(picked) : nullptr;
    const int index = mark ? doc->indexOf(mark->page) : -1;
    if (index < 0)
        return;
    const QTransform toView = pageToView(index);
    if (callout->isVisible())
        callout->setGeometry(toView.mapRect(mark->box).toAlignedRect());
    markBar->move(barPoint(toView.mapRect(extentOf(*mark)), markBar));
}

void PageView::buildMarkTools()
{
    markBar = new MarkBar(this);
    callout = new CalloutEditor(this);
    connect(markBar, &MarkBar::changed, this, [this](const Mark &mark) {
        marks->update(mark);
        syncMarks();
    });
    connect(markBar, &MarkBar::removeRequested, this, [this](int id) {
        marks->remove(id);
        pick(0);
    });
    connect(callout, &CalloutEditor::committed, this, [this](const Mark &mark) {
        marks->update(mark);
        setFocus();
        showMarkBar();
    });
}

// Delete, deselect or nudge.
bool PageView::markKey(QKeyEvent *event)
{
    const Mark *mark = picked && interactive() ? marks->find(picked) : nullptr;
    if (!mark)
        return false;
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        marks->remove(picked);
        pick(0);
        return true;
    case Qt::Key_Escape:
        pick(0);
        return true;
    default:
        break;
    }
    const std::optional<QPointF> shift = nudgeOf(event);
    if (!shift)
        return false;
    marks->update(dragged(*mark, kBody, *shift));
    syncMarks();
    return true;
}
