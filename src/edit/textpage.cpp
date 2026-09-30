#include "textpage.h"

#include "cells.h"

#include <algorithm>
#include <span>
#include <utility>

namespace {

constexpr double kInside = 1;
constexpr double kSideBySide = 0.5;
constexpr double kRoomy = 1.5;

// Backdrop holding a line.
struct Member
{
    size_t box;
    size_t line;
};

// Box with room around line.
bool holds(const QRectF &box, const QRectF &line)
{
    const bool inside = box.adjusted(-kInside, -kInside, kInside, kInside).contains(line);
    return inside && box.height() >= line.height() * kRoomy;
}

// Right edge before trailing spaces.
double inkRight(const TextRun &run)
{
    qsizetype kept = run.text.size();
    while (kept > 0 && run.text[kept - 1].isSpace())
        --kept;
    const bool mapped = run.text.size() == qsizetype(run.glyphs.size()) && kept < run.text.size();
    return mapped ? run.glyphs[size_t(kept)].x() : run.box.right();
}

// Groups runs into numbered lines.
std::vector<Line> linesOf(std::vector<TextRun> &runs)
{
    std::vector<Line> lines;
    int previous = -1;
    for (size_t i = 0; i < runs.size(); ++i) {
        TextRun &run = runs[i];
        if (lines.empty() || run.line != previous)
            lines.push_back({i, i, run.box});
        previous = std::exchange(run.line, int(lines.size() - 1));
        lines.back().last = i + 1;
        lines.back().box |= run.box;
        lines.back().box.setRight(inkRight(run));
    }
    return lines;
}

// Smallest backdrop around each line.
std::vector<Member> membersOf(const std::vector<Line> &lines, const std::vector<QRectF> &boxes)
{
    std::vector<Member> members;
    for (size_t i = 0; i < lines.size(); ++i) {
        const QRectF &box = lines[i].box;
        const auto hit = std::ranges::find_if(boxes, [&box](const QRectF &b) { return holds(b, box); });
        if (hit != boxes.end())
            members.push_back({size_t(hit - boxes.begin()), i});
    }
    return members;
}

bool sideBySide(const QRectF &a, const QRectF &b)
{
    const double overlap = std::min(a.bottom(), b.bottom()) - std::max(a.top(), b.top());
    return overlap > std::min(a.height(), b.height()) * kSideBySide;
}

// Backdrop around one column.
void addPanel(TextPage &page, const QRectF &box, std::span<const Member> members)
{
    for (size_t i = 1; i < members.size(); ++i) {
        if (sideBySide(page.lines[members[i - 1].line].box, page.lines[members[i].line].box))
            return;
    }
    for (const Member &member : members)
        page.lines[member.line].panel = int(page.panels.size());
    page.panels.push_back(box);
}

void findPanels(TextPage &page, std::vector<QRectF> boxes)
{
    std::ranges::sort(boxes, {}, [](const QRectF &r) { return r.width() * r.height(); });
    std::vector<Member> members = membersOf(page.lines, boxes);
    std::ranges::sort(members, {}, [&page](const Member &m) { return std::pair(m.box, page.lines[m.line].box.top()); });
    for (size_t first = 0, last = 0; first < members.size(); first = last) {
        last = first + 1;
        while (last < members.size() && members[last].box == members[first].box)
            ++last;
        addPanel(page, boxes[members[first].box], std::span(members).subspan(first, last - first));
    }
}

} // namespace

TextPage arrange(PageText text)
{
    TextPage page{std::move(text.runs), {}, {}, std::move(text.pictures)};
    page.lines = linesOf(page.runs);
    std::vector<QRectF> boxes = std::move(text.backdrops);
    std::vector<QRectF> texts;
    for (const Line &line : page.lines)
        texts.push_back(line.box);
    const std::vector<QRectF> cells = tableCells(texts, boxes);
    boxes.insert(boxes.end(), cells.begin(), cells.end());
    findPanels(page, std::move(boxes));
    return page;
}
