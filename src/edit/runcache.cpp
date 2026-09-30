#include "runcache.h"

#include "pdf/document.h"

#include <algorithm>

namespace {

constexpr double kReach = 1.5;

} // namespace

void RunCache::setDocument(Document *document)
{
    doc = document;
    pages.clear();
}

void RunCache::forget(int id)
{
    pages.erase(id);
}

const TextPage &RunCache::page(int index)
{
    const int id = doc->slot(index).id;
    auto cached = pages.find(id);
    if (cached == pages.end())
        cached = pages.emplace(id, arrange(pageText(*doc, index))).first;
    return cached->second;
}

Passage RunCache::hit(int index, QPointF point)
{
    const TextPage &text = page(index);
    const auto found = std::ranges::find_if(text.runs, [point](const TextRun &run) {
        return run.box.adjusted(-kReach, -kReach, kReach, kReach).contains(point);
    });
    if (found == text.runs.end())
        return {};
    return passageAt(text, size_t(found->line));
}
