#include "passage.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <span>

namespace {

constexpr double kMinGap = 0.8;
constexpr double kMaxGap = 2.2;
constexpr double kGapDrift = 0.15;
constexpr double kIndent = 0.6;
constexpr float kSizeTolerance = 0.05f;

// Wrap edge and line pitch.
struct Frame
{
    double right;
    double pitch;
};

double baseline(const TextPage &page, size_t line)
{
    return page.runs[page.lines[line].first].glyphs.front().y();
}

// Run carrying most glyphs.
const TextRun &bodyOf(const TextPage &page, const Line &line)
{
    const std::span runs(page.runs.begin() + qsizetype(line.first), line.last - line.first);
    return *std::ranges::max_element(runs, {}, [](const TextRun &run) { return run.glyphs.size(); });
}

bool sameStyle(const TextRun &a, const TextRun &b)
{
    return a.font == b.font && a.color == b.color && std::fabs(a.size - b.size) < kSizeTolerance &&
           a.block == b.block && a.direction == QPointF(1, 0) && b.direction == QPointF(1, 0);
}

// Next line continues the paragraph.
bool continues(const TextPage &page, size_t upper, size_t lower, double gap)
{
    const Line &top = page.lines[upper];
    const Line &bottom = page.lines[lower];
    const TextRun &a = bodyOf(page, top);
    const double step = baseline(page, lower) - baseline(page, upper);
    const bool spaced = step > a.size * kMinGap && step < a.size * kMaxGap &&
                        (gap <= 0 || std::fabs(step - gap) < gap * kGapDrift);
    const bool aligned = std::fabs(top.box.left() - bottom.box.left()) < a.size * kIndent;
    return top.panel < 0 && bottom.panel < 0 && sameStyle(a, bodyOf(page, bottom)) && spaced && aligned;
}

std::vector<size_t> paragraphLines(const TextPage &page, size_t line)
{
    size_t first = line;
    size_t last = line;
    double gap = 0;
    while (first > 0 && continues(page, first - 1, first, gap)) {
        gap = baseline(page, first) - baseline(page, first - 1);
        --first;
    }
    while (last + 1 < page.lines.size() && continues(page, last, last + 1, gap)) {
        gap = baseline(page, last + 1) - baseline(page, last);
        ++last;
    }
    std::vector<size_t> lines(last - first + 1);
    std::iota(lines.begin(), lines.end(), first);
    return lines;
}

std::vector<size_t> panelLines(const TextPage &page, int panel)
{
    std::vector<size_t> lines;
    for (size_t i = 0; i < page.lines.size(); ++i) {
        if (page.lines[i].panel == panel)
            lines.push_back(i);
    }
    std::ranges::sort(lines, {}, [&page](size_t line) { return baseline(page, line); });
    return lines;
}

double pitchOf(const TextPage &page, const std::vector<size_t> &lines)
{
    if (lines.size() < 2)
        return 0;
    std::vector<double> steps;
    for (size_t i = 1; i < lines.size(); ++i)
        steps.push_back(baseline(page, lines[i]) - baseline(page, lines[i - 1]));
    std::ranges::nth_element(steps, steps.begin() + qsizetype(steps.size() / 2));
    return steps[steps.size() / 2];
}

// Width of the leading word.
double firstWord(const TextRun &run)
{
    const qsizetype space = run.text.indexOf(' ');
    const bool measured = space > 0 && size_t(space) < run.glyphs.size();
    return (measured ? run.glyphs[size_t(space)].x() : run.box.right()) - run.box.left();
}

QString trimmedEnd(QString text)
{
    while (!text.isEmpty() && text.back().isSpace())
        text.chop(1);
    return text;
}

QString trimmedStart(const QString &text)
{
    const auto solid = std::ranges::find_if(text, [](QChar c) { return !c.isSpace(); });
    return text.sliced(solid - text.begin());
}

// Space, none after hyphen.
QString wrapJoint(const TextPage &page, size_t upper)
{
    const QString text = trimmedEnd(page.runs[page.lines[upper].last - 1].text);
    const bool hyphen = text.size() > 1 && text.back() == u'-' && text[text.size() - 2].isLetterOrNumber();
    return hyphen ? QString() : QStringLiteral(" ");
}

// Soft wrap, break or blanks.
QString jointIn(const TextPage &page, size_t upper, size_t lower, const Frame &frame)
{
    const double step = baseline(page, lower) - baseline(page, upper);
    const long blank = frame.pitch > 0 ? std::lround(step / frame.pitch) - 1 : 0;
    if (blank > 0)
        return QString(blank + 1, '\n');
    const TextRun &next = page.runs[page.lines[lower].first];
    const bool wraps = !next.text.startsWith(' ') && page.lines[upper].box.right() + firstWord(next) > frame.right;
    return wraps ? wrapJoint(page, upper) : QStringLiteral("\n");
}

// Appends line; panels keep indent.
void addLine(Passage &passage, const TextPage &page, size_t index, const QString &joint)
{
    const Line &line = page.lines[index];
    for (size_t i = line.first; i < line.last; ++i) {
        TextRun run = page.runs[i];
        if (i == line.first && line.panel < 0)
            run.text = trimmedStart(run.text);
        if (i + 1 == line.last)
            run.text = trimmedEnd(run.text);
        passage.joints.push_back(i == line.first ? joint : QString());
        passage.runs.push_back(std::move(run));
    }
    passage.outlines.push_back(line.box);
}

Passage flowed(const TextPage &page, const std::vector<size_t> &lines)
{
    Passage passage;
    for (size_t i = 0; i < lines.size(); ++i)
        addLine(passage, page, lines[i], i ? wrapJoint(page, lines[i - 1]) : QString());
    passage.leading = pitchOf(page, lines);
    return passage;
}

Passage boxed(const TextPage &page, const std::vector<size_t> &lines, const QRectF &panel)
{
    const size_t leftmost = std::ranges::min(lines, {}, [&page](size_t line) { return page.lines[line].box.left(); });
    const double inset = page.lines[leftmost].box.left() - panel.left();
    const Frame frame{panel.right() - inset, pitchOf(page, lines)};
    Passage passage;
    for (size_t i = 0; i < lines.size(); ++i)
        addLine(passage, page, lines[i], i ? jointIn(page, lines[i - 1], lines[i], frame) : QString());
    passage.leading = frame.pitch;
    return passage;
}

} // namespace

Passage passageAt(const TextPage &page, size_t line)
{
    const int panel = page.lines[line].panel;
    if (panel < 0)
        return flowed(page, paragraphLines(page, line));
    return boxed(page, panelLines(page, panel), page.panels[size_t(panel)]);
}
