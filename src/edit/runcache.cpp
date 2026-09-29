#include "runcache.h"

#include "pdf/document.h"

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

const TextRun *RunCache::hit(int index, QPointF point)
{
    const int id = doc->slot(index).id;
    auto cached = pages.find(id);
    if (cached == pages.end())
        cached = pages.emplace(id, pageRuns(*doc, index)).first;
    for (const TextRun &run : cached->second) {
        if (run.box.adjusted(-kReach, -kReach, kReach, kReach).contains(point))
            return &run;
    }
    return nullptr;
}
