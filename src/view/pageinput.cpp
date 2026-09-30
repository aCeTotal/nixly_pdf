#include "pageview.h"

#include "module/moduleset.h"
#include "pdf/document.h"

#include <QKeyEvent>
#include <cmath>

namespace {

constexpr double kWheelStep = 140;
constexpr double kKeyStep = 80;
constexpr double kPageStep = 0.9;
constexpr double kZoomStep = 1.2;
constexpr double kNotch = 120.0;
constexpr double kNudge = 1;
constexpr double kNudgeFar = 10;
constexpr double kMarkReach = 4;

} // namespace

void PageView::wheelEvent(QWheelEvent *event)
{
    if (!doc)
        return;
    const QPoint angle = event->angleDelta();
    if (event->modifiers() & Qt::ControlModifier) {
        zoomTo(zoom.target() * std::pow(kZoomStep, angle.y() / kNotch), event->position());
        return;
    }
    const QPoint pixels = event->pixelDelta();
    if (pixels.isNull()) {
        scrollX.setTarget(scrollX.target() - angle.x() / kNotch * kWheelStep);
        scrollY.setTarget(scrollY.target() - angle.y() / kNotch * kWheelStep);
    } else {
        scrollX.shift(-pixels.x());
        scrollY.shift(-pixels.y());
    }
    clampScroll();
    overlay.wake();
    ticker.start();
}

void PageView::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    const QPointF pos = event->position();
    const QRectF grip = overlay.thumb(scrollState());
    if (!doc || event->button() != Qt::LeftButton)
        return;
    if (pos.x() > width() - ScrollOverlay::kGrip && !grip.isNull()) {
        gripOffset = grip.contains(pos) ? pos.y() - grip.top() : grip.height() / 2;
        mouseMoveEvent(event);
        return;
    }
    if (armed) {
        startSketch(pos);
        return;
    }
    if (ghost) {
        placeModule(pos);
        return;
    }
    if (interactive() && (pressMark(pos) || pressModule(pos)))
        return;
    select(0);
    pick(0);
    if (mode != Mode::Edit || !hovered)
        return;
    const Hover lift = *std::exchange(hovered, std::nullopt);
    if (lift.picture)
        emit pictureRequested(lift.index, *lift.picture);
    else
        emit liftRequested(lift.index, lift.passage);
}

void PageView::mouseMoveEvent(QMouseEvent *event)
{
    cursor = event->position();
    if (gripOffset) {
        const ScrollState state = scrollState();
        const double travel = height() - 8 - overlay.thumb(state).height();
        scrollY.jump((cursor->y() - *gripOffset - 4) / travel * (layout.height() - height()));
        clampScroll();
        overlay.wake();
        ticker.start();
        return;
    }
    if (sketch) {
        drawSketch(*cursor);
        return;
    }
    if (markDrag) {
        dragMark(*cursor);
        return;
    }
    if (drag) {
        dragModule(*cursor);
        return;
    }
    if (ghost || armed) {
        update();
        return;
    }
    const std::optional<Grip> grip = interactive() ? gripAt(*cursor) : std::nullopt;
    const bool handle = markHandleAt(*cursor).has_value();
    const std::optional<Spot> spot = spotAt(*cursor);
    const int page = spot ? doc->slot(spot->index).id : 0;
    const bool overMark = interactive() && spot && marks->hit(page, spot->point, kMarkReach / zoom.value());
    const bool overModule = interactive() && spot && (overMark || modules->hit(page, spot->point));
    if (mode == Mode::Edit && !grip && !handle && !overModule) {
        hoverText(*cursor);
        return;
    }
    hovered.reset();
    const Qt::CursorShape gripShape = grip == Grip::Width ? Qt::SizeHorCursor : Qt::SizeFDiagCursor;
    setCursor(grip || handle ? gripShape : overModule ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void PageView::mouseReleaseEvent(QMouseEvent *event)
{
    gripOffset.reset();
    if (sketch) {
        finishSketch(event->position());
        return;
    }
    if (markDrag) {
        markDrag.reset();
        showMarkBar();
        return;
    }
    if (!drag)
        return;
    drag.reset();
    showBar();
}

void PageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const std::optional<Spot> spot = interactive() ? spotAt(event->position()) : std::nullopt;
    const int page = spot ? doc->slot(spot->index).id : 0;
    if (const int mark = spot ? marks->hit(page, spot->point, kMarkReach / zoom.value()) : 0) {
        editCallout(mark);
        return;
    }
    const int id = spot ? modules->hit(page, spot->point) : 0;
    if (id)
        editText(id);
}

void PageView::leaveEvent(QEvent *)
{
    cursor.reset();
    hovered.reset();
    update();
}

void PageView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && (ghost || armed)) {
        cancelPlacement();
        return;
    }
    if (markKey(event) || moduleKey(event))
        return;
    if ((event->modifiers() & Qt::ControlModifier) && zoomKey(event->key()))
        return;
    const double step = scrollStep(event->key());
    if (step == 0) {
        QWidget::keyPressEvent(event);
        return;
    }
    scrollY.setTarget(scrollY.target() + step);
    clampScroll();
    overlay.wake();
    ticker.start();
}

void PageView::cancelPlacement()
{
    ghost.reset();
    armed.reset();
    unsetCursor();
    update();
    emit toolFinished();
    emit placementCancelled();
}

// Delete, deselect or nudge.
bool PageView::moduleKey(QKeyEvent *event)
{
    const Module *module = selected && interactive() ? modules->find(selected) : nullptr;
    if (!module)
        return false;
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        modules->remove(selected);
        select(0);
        return true;
    case Qt::Key_Escape:
        select(0);
        return true;
    default:
        break;
    }
    const std::optional<QPointF> shift = nudgeOf(event);
    if (!shift)
        return false;
    Module moved = *module;
    moved.anchor += *shift;
    modules->update(moved);
    syncEditors();
    return true;
}

// Arrow step, far with Shift.
std::optional<QPointF> PageView::nudgeOf(const QKeyEvent *event)
{
    const double step = event->modifiers() & Qt::ShiftModifier ? kNudgeFar : kNudge;
    switch (event->key()) {
    case Qt::Key_Left:
        return QPointF(-step, 0);
    case Qt::Key_Right:
        return QPointF(step, 0);
    case Qt::Key_Up:
        return QPointF(0, -step);
    case Qt::Key_Down:
        return QPointF(0, step);
    default:
        return std::nullopt;
    }
}

bool PageView::zoomKey(int key)
{
    const QPointF middle(width() / 2.0, height() / 2.0);
    switch (key) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomTo(zoom.target() * kZoomStep, middle);
        return true;
    case Qt::Key_Minus:
        zoomTo(zoom.target() / kZoomStep, middle);
        return true;
    case Qt::Key_0:
        zoomTo(fitZoom(), middle);
        fitted = true;
        return true;
    default:
        return false;
    }
}

double PageView::scrollStep(int key) const
{
    switch (key) {
    case Qt::Key_Home:
        return -scrollY.target();
    case Qt::Key_End:
        return layout.height();
    case Qt::Key_PageDown:
    case Qt::Key_Space:
        return height() * kPageStep;
    case Qt::Key_PageUp:
        return -height() * kPageStep;
    case Qt::Key_Down:
        return kKeyStep;
    case Qt::Key_Up:
        return -kKeyStep;
    default:
        return 0;
    }
}

void PageView::hoverText(QPointF pos)
{
    const std::optional<Spot> spot = spotAt(pos);
    Passage found = spot ? runs.hit(spot->index, spot->point) : Passage();
    const bool text = !found.runs.empty();
    const std::optional<Picture> picture = spot && !text ? runs.pictureAt(spot->index, spot->point) : std::nullopt;
    if (picture)
        found.outlines = {picture->box};
    const bool none = !text && !picture;
    const bool same = hovered && !none && hovered->index == spot->index && hovered->passage.outlines == found.outlines;
    if (same || (none && !hovered))
        return;
    hovered = none ? std::nullopt : std::optional<Hover>(Hover{spot->index, std::move(found), picture});
    const Qt::CursorShape shape = hovered && hovered->picture ? Qt::PointingHandCursor : Qt::IBeamCursor;
    setCursor(hovered ? shape : Qt::ArrowCursor);
    update();
}

