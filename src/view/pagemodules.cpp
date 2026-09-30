#include "pageview.h"

#include "app/theme.h"
#include "module/modulebar.h"
#include "module/moduleeditor.h"
#include "module/modulepaint.h"
#include "pdf/document.h"
#include "font/fontname.h"

#include <QPainter>
#include <cmath>

namespace {

constexpr double kHandle = 5;
constexpr double kHandleReach = 9;
constexpr double kFrameMargin = 3;
constexpr double kMinSize = 2;
constexpr double kMinStretch = 0.2;
constexpr double kBarGap = 10;
constexpr double kBarInset = 8;

QPolygonF frameOf(const Module &module, const Layout &layout, const QTransform &toView)
{
    const QRectF box = layout.bounds.adjusted(-kFrameMargin, -kFrameMargin, kFrameMargin, kFrameMargin);
    return (moduleTransform(module) * toView).map(QPolygonF(box));
}

// Far corner follows target.
Module resized(const Module &start, const QRectF &bounds, QPointF target)
{
    const QPointF margin(kFrameMargin, kFrameMargin);
    const QPointF corner = moduleTransform(start).map(bounds.topLeft() - margin);
    QTransform unturn;
    unturn.rotateRadians(-start.angle);
    const QPointF reach = unturn.map(target - corner);
    const double scale = std::max(kMinSize / start.size, (reach.y() - 2 * kFrameMargin) / bounds.height());
    Module module = start;
    module.stretch = std::max(kMinStretch, reach.x() / (scale * bounds.width() + 2 * kFrameMargin));
    module.size *= scale;
    module.width *= scale;
    module.leading *= scale;
    module.anchor = QPointF();
    module.anchor = corner - moduleTransform(module).map(bounds.topLeft() * scale - margin);
    return module;
}

} // namespace

bool PageView::interactive() const
{
    return modules && (mode == Mode::Edit || mode == Mode::Sign);
}

QTransform PageView::pageToView(int index) const
{
    const QRectF &page = layout.rect(index);
    QTransform transform;
    transform.translate(page.left() - scrollX.value(), page.top() - scrollY.value());
    transform.scale(zoom.value(), zoom.value());
    return transform;
}

void PageView::paintModules(QPainter &painter, int index)
{
    if (!modules)
        return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setTransform(pageToView(index));
    for (int id : modules->onPage(doc->slot(index).id))
        paintModule(painter, *modules->find(id), modules->layout(id));
    painter.restore();
}

void PageView::paintSelection(QPainter &painter)
{
    const Module *module = selected && interactive() ? modules->find(selected) : nullptr;
    const int index = module ? doc->indexOf(module->page) : -1;
    if (index < 0 || editor->isVisible())
        return;
    const QPolygonF frame = frameOf(*module, modules->layout(selected), pageToView(index));
    painter.setPen(QPen(theme::accent, 1.5, Qt::DashLine));
    painter.setBrush(theme::faded(theme::accent, 16));
    painter.drawPolygon(frame);
    painter.setPen(QPen(theme::accent, 2));
    painter.setBrush(Qt::white);
    painter.drawEllipse(frame[2], kHandle, kHandle);
    if (module->kind == ModuleKind::Text)
        painter.drawEllipse((frame[1] + frame[2]) / 2, kHandle, kHandle);
}

void PageView::paintGhost(QPainter &painter)
{
    const std::optional<Spot> spot = cursor ? spotAt(*cursor) : std::nullopt;
    if (!ghost || !spot)
        return;
    painter.save();
    painter.translate(*cursor);
    painter.scale(zoom.value(), zoom.value());
    painter.translate(-ghost->layout.bounds.center());
    painter.setOpacity(0.85);
    painter.setPen(QPen(theme::accent, 1.0 / zoom.value(), Qt::DashLine));
    painter.setBrush(theme::faded(theme::accent, 18));
    painter.drawRect(ghost->layout.bounds);
    Module shown = ghost->module;
    shown.anchor = QPointF();
    paintModule(painter, shown, ghost->layout);
    painter.restore();
}

std::optional<PageView::Grip> PageView::gripAt(QPointF pos) const
{
    const Module *module = selected && interactive() ? modules->find(selected) : nullptr;
    const int index = module ? doc->indexOf(module->page) : -1;
    if (index < 0)
        return std::nullopt;
    const QPolygonF frame = frameOf(*module, modules->layout(selected), pageToView(index));
    if (QLineF(pos, frame[2]).length() < kHandleReach)
        return Grip::Size;
    if (module->kind == ModuleKind::Text && QLineF(pos, (frame[1] + frame[2]) / 2).length() < kHandleReach)
        return Grip::Width;
    return std::nullopt;
}

bool PageView::pressModule(QPointF pos)
{
    if (const std::optional<Grip> grip = gripAt(pos)) {
        drag = Drag{selected, *grip, pos, *modules->find(selected), modules->layout(selected).bounds};
        return true;
    }
    const std::optional<Spot> spot = spotAt(pos);
    const int id = spot ? modules->hit(doc->slot(spot->index).id, spot->point) : 0;
    if (!id)
        return false;
    select(id);
    drag = Drag{id, Grip::Move, pos, *modules->find(id), modules->layout(id).bounds};
    return true;
}

void PageView::dragModule(QPointF pos)
{
    Module module = drag->start;
    const int start = doc->indexOf(module.page);
    const QPointF delta = (pos - drag->grab) / zoom.value();
    const QRectF &bounds = drag->bounds;
    if (drag->grip == Grip::Move) {
        const QPointF anchor = pageToView(start).map(module.anchor) + (pos - drag->grab);
        const std::optional<Spot> spot = spotAt(pos);
        const int target = spot && module.cover.isEmpty() ? spot->index : start;
        module.page = doc->slot(target).id;
        module.anchor = pageToView(target).inverted().map(anchor);
    }
    if (drag->grip == Grip::Width) {
        const double base = module.width > 0 ? module.width : bounds.width();
        module.width = std::max(module.size, base + delta.x() / module.stretch);
    }
    if (drag->grip == Grip::Size) {
        const QPointF corner = bounds.bottomRight() + QPointF(kFrameMargin, kFrameMargin);
        module = resized(module, bounds, moduleTransform(module).map(corner) + delta);
    }
    modules->update(module);
    syncEditors();
}

bool PageView::placeModule(QPointF pos)
{
    const std::optional<Spot> spot = spotAt(pos);
    if (!spot)
        return false;
    Module module = ghost->module;
    module.page = doc->slot(spot->index).id;
    module.anchor = spot->point - ghost->layout.bounds.center();
    ghost.reset();
    unsetCursor();
    const int id = modules->add(module);
    select(id);
    emit toolFinished();
    if (module.kind == ModuleKind::Text && module.text.isEmpty())
        editText(id);
    return true;
}

void PageView::select(int id)
{
    selected = id;
    if (id && picked)
        pick(0);
    showBar();
    update();
}

void PageView::editText(int id)
{
    const Module *module = modules->find(id);
    if (!module || module->kind == ModuleKind::Date)
        return;
    selected = id;
    bar->hide();
    const int index = doc->indexOf(module->page);
    const QTransform toView = moduleTransform(*module) * pageToView(index);
    const std::optional<QPointF> at = cursor ? std::optional(toView.inverted().map(*cursor)) : std::nullopt;
    editor->open(*module, modules->fonts(), at);
    syncEditors();
    update();
}

void PageView::showBar()
{
    const Module *module = selected && interactive() && !editor->isVisible() ? modules->find(selected) : nullptr;
    if (!module) {
        bar->hide();
        return;
    }
    const std::vector<Face *> faces = modules->fonts().pageFaces(module->originalFont);
    bar->present(*module, faces.empty() ? QString() : familyOf(faces.front()->name));
    syncEditors();
}

void PageView::syncEditors()
{
    syncMarks();
    const Module *module = selected ? modules->find(selected) : nullptr;
    const int index = module ? doc->indexOf(module->page) : -1;
    if (index < 0)
        return;
    const QRectF box = frameOf(*module, modules->layout(selected), pageToView(index)).boundingRect();
    if (editor->isVisible())
        editor->place(moduleTransform(*module) * pageToView(index));
    bar->move(barPoint(box, bar));
}

// Above the area, else below.
QPoint PageView::barPoint(const QRectF &area, const QWidget *panel) const
{
    const double half = panel->width() / 2.0;
    const double x = std::clamp(area.center().x() - half, kBarInset, width() - panel->width() - kBarInset);
    const double above = area.top() - panel->height() - kBarGap;
    return QPoint(int(x), int(above < kBarInset ? area.bottom() + kBarGap : above));
}

void PageView::buildModuleTools()
{
    editor = new ModuleEditor(this);
    bar = new ModuleBar(this);
    connect(editor, &ModuleEditor::edited, this, [this](const Module &module) {
        modules->update(module);
        syncEditors();
    });
    connect(editor, &ModuleEditor::committed, this, [this](const Module &module) {
        modules->update(module);
        setFocus();
        showBar();
    });
    connect(editor, &ModuleEditor::cancelled, this, [this](const Module &original) {
        modules->update(original);
        setFocus();
        showBar();
    });
    connect(bar, &ModuleBar::changed, this, [this](const Module &module) {
        modules->update(module);
        syncEditors();
    });
    connect(bar, &ModuleBar::removeRequested, this, [this](int id) {
        modules->remove(id);
        select(0);
    });
}
