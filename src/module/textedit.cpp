#include "textedit.h"

#include <algorithm>

namespace {

bool holds(const Span &span, qsizetype at)
{
    return span.start <= at && at < span.start + span.length;
}

// Span lending style to insertions.
qsizetype ownerOf(const std::vector<Span> &spans, qsizetype at)
{
    const qsizetype probe = at > 0 ? at - 1 : at;
    const auto hit = std::ranges::find_if(spans, [probe](const Span &span) { return holds(span, probe); });
    return hit == spans.end() ? -1 : hit - spans.begin();
}

} // namespace

void insertText(Module &module, qsizetype at, const QString &text)
{
    const qsizetype owner = ownerOf(module.spans, at);
    module.text.insert(at, text);
    const int count = int(text.size());
    for (qsizetype i = 0; i < qsizetype(module.spans.size()); ++i) {
        Span &span = module.spans[size_t(i)];
        if (i == owner)
            span.length += count;
        else if (span.start >= at)
            span.start += count;
    }
}

void removeText(Module &module, qsizetype from, qsizetype to)
{
    module.text.remove(from, to - from);
    for (Span &span : module.spans) {
        const qsizetype end = span.start + span.length;
        const qsizetype before = std::max<qsizetype>(0, std::min<qsizetype>(to, span.start) - from);
        const qsizetype inside = std::max<qsizetype>(0, std::min(to, end) - std::max<qsizetype>(from, span.start));
        span.start -= int(before);
        span.length -= int(inside);
    }
    std::erase_if(module.spans, [](const Span &span) { return span.length <= 0; });
}
