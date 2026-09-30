#include "markset.h"

#include "markshape.h"

#include <algorithm>

int MarkSet::add(Mark mark)
{
    mark.id = nextId++;
    marks.push_back(std::move(mark));
    emit changed(marks.back().page);
    return marks.back().id;
}

void MarkSet::update(const Mark &mark)
{
    const auto hit = std::ranges::find(marks, mark.id, &Mark::id);
    if (hit == marks.end())
        return;
    const int before = hit->page;
    *hit = mark;
    emit changed(mark.page);
    if (before != mark.page)
        emit changed(before);
}

void MarkSet::remove(int id)
{
    const auto hit = std::ranges::find(marks, id, &Mark::id);
    if (hit == marks.end())
        return;
    const int page = hit->page;
    marks.erase(hit);
    emit changed(page);
}

void MarkSet::dropPage(int page)
{
    std::erase_if(marks, [page](const Mark &mark) { return mark.page == page; });
}

const Mark *MarkSet::find(int id) const
{
    const auto hit = std::ranges::find(marks, id, &Mark::id);
    return hit == marks.end() ? nullptr : &*hit;
}

std::vector<int> MarkSet::onPage(int page) const
{
    std::vector<int> ids;
    for (const Mark &mark : marks) {
        if (mark.page == page)
            ids.push_back(mark.id);
    }
    return ids;
}

int MarkSet::hit(int page, QPointF point, double reach) const
{
    for (auto it = marks.rbegin(); it != marks.rend(); ++it) {
        if (it->page == page && hits(*it, point, reach))
            return it->id;
    }
    return 0;
}
