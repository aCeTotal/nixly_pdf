#include "moduleset.h"

#include "modulepaint.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kReach = 3;
constexpr double kTouch = 1;
constexpr double kStill = 0.01;

// Changes needing a new layout.
bool reshaped(const Module &a, const Module &b)
{
    return a.text != b.text || a.spans != b.spans || a.size != b.size || a.width != b.width ||
           a.leading != b.leading || a.font != b.font;
}

// Page box of module.
QRectF footprint(const Module &module, const Layout &layout)
{
    return moduleTransform(module).mapRect(layout.bounds);
}

} // namespace

ModuleSet::ModuleSet(FontLibrary &fonts, QObject *parent) : QObject(parent), library(fonts) {}

int ModuleSet::add(Module module)
{
    module.id = nextId++;
    Layout layout = typeset(module, library);
    entries.push_back({module, std::move(layout)});
    emit changed(module.page);
    return module.id;
}

void ModuleSet::update(const Module &module)
{
    const auto hit = std::ranges::find_if(entries, [&](const Entry &e) { return e.module.id == module.id; });
    if (hit == entries.end())
        return;
    const int before = hit->module.page;
    const QRectF was = footprint(hit->module, hit->layout);
    const bool relaid = reshaped(hit->module, module);
    if (relaid)
        hit->layout = typeset(module, library);
    hit->module = module;
    if (relaid && before == module.page)
        pushBelow(*hit, was);
    emit changed(module.page);
    if (before != module.page)
        emit changed(before);
}

// Column below follows resize.
void ModuleSet::pushBelow(const Entry &moved, const QRectF &was)
{
    const double delta = footprint(moved.module, moved.layout).bottom() - was.bottom();
    if (std::fabs(delta) < kStill)
        return;
    for (Entry &other : entries) {
        const QRectF box = footprint(other.module, other.layout);
        const bool column = box.left() < was.right() && box.right() > was.left();
        const bool under = other.module.page == moved.module.page && box.top() >= was.bottom() - kTouch;
        if (&other != &moved && column && under)
            other.module.anchor.ry() += delta;
    }
}

void ModuleSet::remove(int id)
{
    const auto hit = std::ranges::find_if(entries, [id](const Entry &e) { return e.module.id == id; });
    if (hit == entries.end())
        return;
    const int page = hit->module.page;
    entries.erase(hit);
    emit changed(page);
}

void ModuleSet::dropPage(int page)
{
    std::erase_if(entries, [page](const Entry &e) { return e.module.page == page; });
}

const ModuleSet::Entry *ModuleSet::entry(int id) const
{
    const auto hit = std::ranges::find_if(entries, [id](const Entry &e) { return e.module.id == id; });
    return hit == entries.end() ? nullptr : &*hit;
}

const Module *ModuleSet::find(int id) const
{
    const Entry *found = entry(id);
    return found ? &found->module : nullptr;
}

const Layout &ModuleSet::layout(int id) const
{
    return entry(id)->layout;
}

std::vector<int> ModuleSet::onPage(int page) const
{
    std::vector<int> ids;
    for (const Entry &e : entries) {
        if (e.module.page == page)
            ids.push_back(e.module.id);
    }
    return ids;
}

int ModuleSet::hit(int page, QPointF point) const
{
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (it->module.page != page)
            continue;
        const QPointF local = moduleTransform(it->module).inverted().map(point);
        if (it->layout.bounds.adjusted(-kReach, -kReach, kReach, kReach).contains(local))
            return it->module.id;
    }
    return 0;
}
