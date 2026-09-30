#include "pageview.h"

#include "app/theme.h"
#include "mark/callouteditor.h"
#include "mark/markbar.h"
#include "module/modulebar.h"
#include "module/moduleeditor.h"
#include "pagepainter.h"
#include "pdf/document.h"
#include "pdf/renderer.h"

#include <QPainter>

namespace {

constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 12;
constexpr double kFitLimit = 1.5;
constexpr QSizeF kEmptyCard(380, 180);

} // namespace

PageView::PageView(QWidget *parent)
    : QWidget(parent), ticker(this, [this](double seconds) { return frame(seconds); })
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    zoom.jump(1);
    buildModuleTools();
    buildMarkTools();
}

void PageView::setDocument(Document *document, Renderer *source, Layers layers)
{
    doc = document;
    renderer = source;
    modules = layers.modules;
    marks = layers.marks;
    picked = 0;
    armed.reset();
    sketch.reset();
    markDrag.reset();
    markBar->hide();
    callout->hide();
    runs.setDocument(doc);
    hovered.reset();
    drag.reset();
    ghost.reset();
    selected = 0;
    editor->hide();
    bar->hide();
    fitted = true;
    current = -1;
    scrollX.jump(0);
    scrollY.jump(0);
    settledScale = previousScale = 0;
    layout = PageLayout();
    if (renderer)
        connect(renderer, &Renderer::updated, this, qOverload<>(&QWidget::update));
    if (modules)
        connect(modules, &ModuleSet::changed, this, qOverload<>(&QWidget::update));
    if (marks)
        connect(marks, &MarkSet::changed, this, qOverload<>(&QWidget::update));
    relayout();
}

void PageView::setMode(Mode next)
{
    mode = next;
    ghost.reset();
    armed.reset();
    sketch.reset();
    markDrag.reset();
    hovered.reset();
    editor->clearFocus();
    callout->clearFocus();
    if (!interactive()) {
        select(0);
        pick(0);
    }
    unsetCursor();
    update();
}

void PageView::relayout()
{
    if (fitted)
        zoom.jump(fitZoom());
    rebuild(zoom.value(), QPointF(width() / 2.0, 0));
    clampScroll();
    scrolled();
    update();
}

void PageView::forgetText(int id)
{
    runs.forget(id);
}

void PageView::goToPage(int index)
{
    if (!doc || index < 0 || index >= layout.count())
        return;
    scrollY.setTarget(layout.rect(index).top() - PageLayout::kGap);
    clampScroll();
    overlay.wake();
    ticker.start();
}

void PageView::armPlacement(const Module &prototype)
{
    ghost = Ghost{prototype, typeset(prototype, modules->fonts())};
    armed.reset();
    select(0);
    setCursor(Qt::CrossCursor);
    setFocus();
    update();
}

bool PageView::frame(double seconds)
{
    if (zoom.moving()) {
        zoom.advance(seconds);
        rebuild(zoom.value(), zoomAnchor);
    }
    scrollX.advance(seconds);
    scrollY.advance(seconds);
    clampScroll();
    const bool fading = overlay.advance(seconds);
    scrolled();
    update();
    return zoom.moving() || scrollX.moving() || scrollY.moving() || fading;
}

void PageView::zoomTo(double target, QPointF anchor)
{
    zoom.setTarget(std::clamp(target, kMinZoom, kMaxZoom));
    zoomAnchor = anchor;
    fitted = false;
    ticker.start();
}

// Relayout keeping anchor still.
void PageView::rebuild(double scale, QPointF anchor)
{
    if (!doc)
        return;
    const QPointF content = anchor + QPointF(scrollX.value(), scrollY.value());
    const int index = layout.count() ? layout.pageAt(content.y()) : -1;
    const QRectF before = index >= 0 ? layout.rect(index) : QRectF();
    layout.build(*doc, scale, width());
    if (index < 0 || index >= layout.count())
        return;
    const QRectF after = layout.rect(index);
    const double fx = (content.x() - before.left()) / before.width();
    const double fy = (content.y() - before.top()) / before.height();
    scrollX.shift(after.left() + fx * after.width() - content.x());
    scrollY.shift(after.top() + fy * after.height() - content.y());
}

void PageView::clampScroll()
{
    scrollX.clamp(0, std::max(0.0, layout.width() - width()));
    scrollY.clamp(0, std::max(0.0, layout.height() - height()));
}

void PageView::scrolled()
{
    syncEditors();
    if (!layout.count())
        return;
    const int page = layout.pageAt(scrollY.value() + height() / 2.0);
    if (page == current)
        return;
    current = page;
    emit currentPageChanged(page);
}

double PageView::fitZoom() const
{
    double widest = 0;
    for (int i = 0; doc && i < doc->count(); ++i)
        widest = std::max(widest, doc->slot(i).size.width());
    if (widest <= 0)
        return 1;
    return std::min(kFitLimit, (width() - 2 * PageLayout::kMargin) / widest);
}

ScrollState PageView::scrollState() const
{
    return {QRectF(rect()), scrollY.value(), layout.height()};
}

std::optional<PageView::Spot> PageView::spotAt(QPointF pos) const
{
    if (!layout.count())
        return std::nullopt;
    const QPointF content = pos + QPointF(scrollX.value(), scrollY.value());
    const int index = layout.pageAt(content.y());
    const QRectF &page = layout.rect(index);
    if (!page.contains(content))
        return std::nullopt;
    return Spot{index, (content - page.topLeft()) / zoom.value()};
}

QRectF PageView::toView(int index, const QRectF &box) const
{
    const double z = zoom.value();
    const QPointF origin = layout.rect(index).topLeft() - QPointF(scrollX.value(), scrollY.value());
    return QRectF(origin + box.topLeft() * z, box.size() * z);
}

void PageView::resizeEvent(QResizeEvent *)
{
    relayout();
}

void PageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), theme::backdrop);
    if (!doc || !layout.count()) {
        paintEmpty(painter);
        return;
    }
    paintPages(painter);
    paintOverlays(painter);
}

void PageView::paintPages(QPainter &painter)
{
    const double dpr = devicePixelRatioF();
    const double z = zoom.value();
    const int exact = Renderer::scaleKey(z * dpr);
    if (!zoom.moving() && exact != settledScale) {
        previousScale = settledScale;
        settledScale = exact;
    }
    const QPointF offset(scrollX.value(), scrollY.value());
    const QRectF view(rect());
    const QRectF ahead = view.adjusted(0, -view.height() / 2, 0, view.height() / 2);
    auto frameOf = [&](int i, const QRectF &clip) {
        return PageFrame{doc->slot(i).id, layout.rect(i).translated(-offset), clip, z, dpr, settledScale,
                         previousScale};
    };
    auto visit = [&](const QRectF &area, auto draw) {
        for (int i = layout.pageAt(offset.y() + area.top()); i < layout.count(); ++i) {
            if (layout.rect(i).top() - offset.y() > area.bottom())
                break;
            draw(i, frameOf(i, area));
        }
    };
    if (!zoom.moving())
        visit(ahead, [&](int, const PageFrame &page) { prefetchPage(*renderer, page); });
    visit(view, [&](int i, const PageFrame &page) {
        paintPage(painter, *renderer, page);
        paintModules(painter, i);
        paintMarks(painter, i);
    });
}

void PageView::paintOverlays(QPainter &painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    for (size_t i = 0; hovered && i < hovered->passage.outlines.size(); ++i) {
        painter.setPen(QPen(theme::accent, 1.5));
        painter.setBrush(theme::faded(theme::accent, 28));
        painter.drawRoundedRect(toView(hovered->index, hovered->passage.outlines[i]).adjusted(-3, -2, 3, 2), 4, 4);
    }
    paintSelection(painter);
    paintMarkSelection(painter);
    paintGhost(painter);
    overlay.paint(painter, scrollState(), QStringLiteral("%1 / %2").arg(current + 1).arg(layout.count()));
}

void PageView::paintEmpty(QPainter &painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    QRectF card(QPointF(), kEmptyCard);
    card.moveCenter(QRectF(rect()).center());
    painter.setPen(QPen(theme::faded(theme::accent, 120), 1.5, Qt::DashLine));
    painter.setBrush(theme::faded(theme::surface, 200));
    painter.drawRoundedRect(card, 18, 18);
    QFont font = painter.font();
    font.setPixelSize(18);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(theme::text);
    painter.drawText(card.adjusted(0, 0, 0, -30), Qt::AlignCenter, tr("Drop a PDF here"));
    font.setPixelSize(13);
    font.setWeight(QFont::Normal);
    painter.setFont(font);
    painter.setPen(theme::muted);
    painter.drawText(card.adjusted(0, 40, 0, 0), Qt::AlignCenter, tr("or choose File ▸ Open  (Ctrl+O)"));
}
