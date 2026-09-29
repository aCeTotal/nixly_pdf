#include "pageview.h"

#include "edit/runeditor.h"

#include <QKeyEvent>
#include <cmath>

namespace {

constexpr double kWheelStep = 140;
constexpr double kKeyStep = 80;
constexpr double kPageStep = 0.9;
constexpr double kZoomStep = 1.2;
constexpr double kNotch = 120.0;

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
    const std::optional<Spot> spot = spotAt(pos);
    if (stamp && spot) {
        stamp.reset();
        unsetCursor();
        update();
        emit stampPlaced(spot->index, spot->point);
        return;
    }
    if (mode == Mode::Edit && hovered)
        beginEdit();
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
    if (stamp) {
        update();
        return;
    }
    if (mode == Mode::Edit && !editing)
        hoverText(*cursor);
}

void PageView::mouseReleaseEvent(QMouseEvent *)
{
    gripOffset.reset();
}

void PageView::leaveEvent(QEvent *)
{
    cursor.reset();
    hovered.reset();
    update();
}

void PageView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelStamp();
        return;
    }
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

void PageView::cancelStamp()
{
    if (!stamp)
        return;
    stamp.reset();
    unsetCursor();
    update();
    emit stampCancelled();
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
    const TextRun *run = spot ? runs.hit(spot->index, spot->point) : nullptr;
    const bool same = run && hovered && hovered->index == spot->index && hovered->run.glyphs == run->glyphs;
    if (same || (!run && !hovered))
        return;
    hovered = run ? std::optional<Editing>(Editing{spot->index, *run}) : std::nullopt;
    setCursor(run ? Qt::IBeamCursor : Qt::ArrowCursor);
    update();
}

void PageView::beginEdit()
{
    editing = std::exchange(hovered, std::nullopt);
    editor->open(editing->run);
    placeEditor();
    update();
}

void PageView::placeEditor()
{
    if (editing)
        editor->place(toView(editing->index, editing->run.box), editing->run.size * zoom.value());
}
