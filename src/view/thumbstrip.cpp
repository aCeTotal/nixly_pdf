#include "thumbstrip.h"

#include "pdf/document.h"
#include "pdf/renderer.h"
#include "mark/markset.h"
#include "module/moduleset.h"

#include <QKeyEvent>
#include <algorithm>

namespace {

constexpr double kLabel = 24;
constexpr double kGap = 14;
constexpr double kMargin = 16;
constexpr double kLiftThreshold = 6;
constexpr double kEdge = 48;
constexpr double kEdgeSpeed = 900;
constexpr double kNotch = 120.0;
constexpr double kPagePixels = 60;

} // namespace

ThumbStrip::ThumbStrip(QWidget *parent)
    : QWidget(parent), ticker(this, [this](double seconds) { return frame(seconds); })
{
    setFixedWidth(kWidth);
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void ThumbStrip::setDocument(Document *document, Renderer *source, Layers shown)
{
    doc = document;
    renderer = source;
    layers = shown;
    current = -1;
    drag.reset();
    scroll.jump(0);
    if (renderer)
        connect(renderer, &Renderer::updated, this, qOverload<>(&QWidget::update));
    if (layers.modules)
        connect(layers.modules, &ModuleSet::changed, this, qOverload<>(&QWidget::update));
    if (layers.marks)
        connect(layers.marks, &MarkSet::changed, this, qOverload<>(&QWidget::update));
    relayout();
}

void ThumbStrip::setMode(Mode next)
{
    mode = next;
    drag.reset();
    update();
}

void ThumbStrip::relayout()
{
    const int count = doc ? doc->count() : 0;
    tops.resize(size_t(count));
    shifts.assign(size_t(count), SmoothValue());
    landed = -1;
    double y = kMargin;
    for (int i = 0; i < count; ++i) {
        tops[size_t(i)] = y;
        const QSizeF size = doc->slot(i).size;
        y += kThumb * size.height() / size.width() + kLabel + kGap;
    }
    extent = y + kMargin - kGap;
    scroll.clamp(0, std::max(0.0, extent - height()));
    update();
}

void ThumbStrip::setCurrent(int index)
{
    if (index == current)
        return;
    current = index;
    aim = index == aim ? -1 : aim;
    follow(index);
    update();
}

double ThumbStrip::slotTop(int index) const
{
    return tops[size_t(index)];
}

int ThumbStrip::slotAt(double y) const
{
    const auto after = std::upper_bound(tops.begin(), tops.end(), y);
    return std::max(0, int(after - tops.begin()) - 1);
}

// Landing slot for lifted page.
int ThumbStrip::dropSlot() const
{
    const double y = pointer + scroll.value();
    int slot = 0;
    for (int i = 0; i < int(tops.size()); ++i) {
        const double next = i + 1 < int(tops.size()) ? tops[size_t(i) + 1] : extent;
        if (i != drag->from && (tops[size_t(i)] + next) / 2 < y)
            ++slot;
    }
    return slot;
}

void ThumbStrip::follow(int index)
{
    if (index < 0 || index >= int(tops.size()))
        return;
    const double top = slotTop(index) - kMargin;
    const double bottom = (index + 1 < int(tops.size()) ? slotTop(index + 1) : extent) + kMargin - kGap;
    if (top < scroll.target())
        scroll.setTarget(top);
    else if (bottom > scroll.target() + height())
        scroll.setTarget(bottom - height());
    scroll.clamp(0, std::max(0.0, extent - height()));
    ticker.start();
}

bool ThumbStrip::frame(double seconds)
{
    double edge = 0;
    if (drag && pointer < kEdge)
        edge = -(kEdge - pointer) / kEdge;
    else if (drag && pointer > height() - kEdge)
        edge = (pointer - height() + kEdge) / kEdge;
    if (edge != 0)
        scroll.shift(edge * kEdgeSpeed * seconds);
    scroll.advance(seconds);
    scroll.clamp(0, std::max(0.0, extent - height()));
    raise.advance(seconds);
    landed = raise.moving() ? landed : -1;
    bool moving = scroll.moving() || raise.moving() || edge != 0;
    for (SmoothValue &shift : shifts) {
        shift.advance(seconds);
        moving = moving || shift.moving();
    }
    if (edge != 0 && drag->lifted)
        arrange(dropSlot());
    update();
    return moving;
}

// Steps through pages.
void ThumbStrip::wheelEvent(QWheelEvent *event)
{
    const QPoint pixels = event->pixelDelta();
    wheel += pixels.isNull() ? event->angleDelta().y() / kNotch : pixels.y() / kPagePixels;
    const int steps = int(wheel);
    wheel -= steps;
    if (steps == 0 || tops.empty())
        return;
    aim = std::clamp((aim < 0 ? current : aim) - steps, 0, int(tops.size()) - 1);
    emit pageChosen(aim);
}

void ThumbStrip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || tops.empty())
        return;
    pointer = event->position().y();
    aim = -1;
    const int index = slotAt(pointer + scroll.value());
    drag = Drag{index, pointer + scroll.value() - slotTop(index), false};
    emit pageChosen(index);
}

void ThumbStrip::mouseMoveEvent(QMouseEvent *event)
{
    if (!drag)
        return;
    pointer = event->position().y();
    ticker.start();
    const double moved = std::abs(pointer + scroll.value() - slotTop(drag->from) - drag->grab);
    if (!drag->lifted && moved < kLiftThreshold)
        return;
    drag->lifted = true;
    raise.setTarget(1);
    arrange(dropSlot());
}

// Opens gap at landing slot.
void ThumbStrip::arrange(int to)
{
    const int from = drag->from;
    const double next = from + 1 < int(tops.size()) ? slotTop(from + 1) : extent - kMargin + kGap;
    const double span = next - slotTop(from);
    for (int i = 0; i < int(shifts.size()); ++i) {
        const bool up = from < i && i <= to;
        const bool down = to <= i && i < from;
        shifts[size_t(i)].setTarget(up ? -span : down ? span : 0);
    }
}

void ThumbStrip::mouseReleaseEvent(QMouseEvent *)
{
    if (!drag)
        return;
    const int from = drag->from;
    const int id = doc->slot(from).id;
    const bool lifted = drag->lifted;
    const int target = lifted ? dropSlot() : from;
    const double top = pointer - drag->grab + scroll.value();
    drag.reset();
    if (target != from)
        emit pageMoved(from, target);
    if (lifted)
        land(doc->indexOf(id), top);
    update();
}

// Glides a dropped page home.
void ThumbStrip::land(int index, double top)
{
    for (SmoothValue &shift : shifts)
        shift.setTarget(0);
    shifts[size_t(index)].jump(top - slotTop(index));
    shifts[size_t(index)].setTarget(0);
    landed = index;
    raise.setTarget(0);
    ticker.start();
}

void ThumbStrip::leaveEvent(QEvent *)
{
    wheel = 0;
    aim = -1;
}

void ThumbStrip::keyPressEvent(QKeyEvent *event)
{
    const int count = int(tops.size());
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        if (mode == Mode::Edit && current >= 0)
            emit deleteRequested(current);
        return;
    case Qt::Key_Down:
        if (current + 1 < count)
            emit pageChosen(current + 1);
        return;
    case Qt::Key_Up:
        if (current > 0)
            emit pageChosen(current - 1);
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

void ThumbStrip::resizeEvent(QResizeEvent *)
{
    scroll.clamp(0, std::max(0.0, extent - height()));
}
