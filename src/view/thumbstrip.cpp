#include "thumbstrip.h"

#include "pdf/document.h"
#include "pdf/renderer.h"

#include <QKeyEvent>
#include <algorithm>

namespace {

constexpr double kLabel = 24;
constexpr double kGap = 14;
constexpr double kMargin = 16;
constexpr double kLiftThreshold = 6;
constexpr double kEdge = 48;
constexpr double kEdgeSpeed = 900;
constexpr double kWheelStep = 110;
constexpr double kNotch = 120.0;

} // namespace

ThumbStrip::ThumbStrip(QWidget *parent)
    : QWidget(parent), ticker(this, [this](double seconds) { return frame(seconds); })
{
    setFixedWidth(kWidth);
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void ThumbStrip::setDocument(Document *document, Renderer *source)
{
    doc = document;
    renderer = source;
    current = -1;
    drag.reset();
    scroll.jump(0);
    if (renderer)
        connect(renderer, &Renderer::updated, this, qOverload<>(&QWidget::update));
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

void ThumbStrip::scrub(double y)
{
    if (tops.empty())
        return;
    const int index = slotAt(y + scroll.value());
    if (index != current)
        emit pageChosen(index);
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
    bool moving = scroll.moving() || edge != 0;
    for (SmoothValue &shift : shifts) {
        shift.advance(seconds);
        moving = moving || shift.moving();
    }
    if (edge != 0 && mode != Mode::Edit)
        scrub(pointer);
    if (edge != 0 && drag->lifted)
        arrange(dropSlot());
    update();
    return moving;
}

void ThumbStrip::wheelEvent(QWheelEvent *event)
{
    const QPoint pixels = event->pixelDelta();
    if (pixels.isNull())
        scroll.setTarget(scroll.target() - event->angleDelta().y() / kNotch * kWheelStep);
    else
        scroll.shift(-pixels.y());
    scroll.clamp(0, std::max(0.0, extent - height()));
    ticker.start();
}

void ThumbStrip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || tops.empty())
        return;
    pointer = event->position().y();
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
    if (mode != Mode::Edit) {
        scrub(pointer);
        return;
    }
    const double moved = std::abs(pointer + scroll.value() - slotTop(drag->from) - drag->grab);
    if (!drag->lifted && moved < kLiftThreshold)
        return;
    drag->lifted = true;
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
    const int target = drag->lifted ? dropSlot() : from;
    drag.reset();
    for (SmoothValue &shift : shifts)
        shift.jump(0);
    if (target != from)
        emit pageMoved(from, target);
    update();
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
