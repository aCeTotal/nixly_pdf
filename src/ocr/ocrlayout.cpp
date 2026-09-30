#include "ocrlayout.h"

#include "edit/cells.h"
#include "rules.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kRiseToSize = 0.72;
constexpr double kColumnGap = 1.2;
constexpr double kIndent = 0.8;
constexpr double kMinLeading = 0.9;
constexpr double kMaxLeading = 1.8;
constexpr double kLeadingDrift = 0.25;
constexpr double kCoverMargin = 0.15;
constexpr size_t kInkShare = 33;
constexpr int kRing = 12;
constexpr double kInside = 1;

// Line words in one column.
struct Segment
{
    std::vector<const OcrWord *> words;
    QRectF box;
    int line;
    int cell;
    bool alone;
    double size;
};

double median(std::vector<double> values)
{
    std::nth_element(values.begin(), values.begin() + qsizetype(values.size() / 2), values.end());
    return values[values.size() / 2];
}

double sizeOf(const std::vector<const OcrWord *> &words)
{
    std::vector<double> rises;
    for (const OcrWord *word : words)
        rises.push_back(word->baseline.y() - word->box.top());
    return std::max(1.0, median(rises) / kRiseToSize);
}

std::vector<Segment> segmentLine(const std::vector<const OcrWord *> &line)
{
    const double size = sizeOf(line);
    std::vector<Segment> segments;
    for (const OcrWord *word : line) {
        const bool apart = !segments.empty() && word->box.left() - segments.back().box.right() > size * kColumnGap;
        if (segments.empty() || apart || word->cell != segments.back().cell)
            segments.push_back({{}, word->box, word->line, word->cell, false, 0});
        segments.back().words.push_back(word);
        segments.back().box |= word->box;
    }
    for (Segment &segment : segments) {
        segment.alone = segments.size() == 1;
        segment.size = sizeOf(segment.words);
    }
    return segments;
}

std::vector<Segment> segmentAll(const std::vector<OcrWord> &words)
{
    std::vector<Segment> segments;
    std::vector<const OcrWord *> line;
    for (size_t i = 0; i <= words.size(); ++i) {
        const bool flush = i == words.size() || (!line.empty() && words[i].line != line.front()->line);
        if (flush && !line.empty()) {
            for (Segment &segment : segmentLine(line))
                segments.push_back(std::move(segment));
            line.clear();
        }
        if (i < words.size())
            line.push_back(&words[i]);
    }
    return segments;
}

// Segment continues the block's paragraph.
bool joins(const std::vector<const Segment *> &block, const Segment &next)
{
    const Segment &last = *block.back();
    const double size = last.size;
    const double step = next.words.front()->baseline.y() - last.words.front()->baseline.y();
    const double leading = block.size() > 1 ? (last.words.front()->baseline.y() -
                                              block.front()->words.front()->baseline.y()) / double(block.size() - 1)
                                            : step;
    return last.alone && next.alone && next.line == last.line + 1 &&
           next.words.front()->paragraph == last.words.front()->paragraph &&
           std::fabs(next.box.left() - last.box.left()) < size * kIndent && step > size * kMinLeading &&
           step < size * kMaxLeading && std::fabs(step - leading) < leading * kLeadingDrift;
}

// Same table cell, or paragraph.
bool belongs(const std::vector<const Segment *> &block, const Segment &next)
{
    const int cell = block.front()->cell;
    return cell >= 0 || next.cell >= 0 ? cell == next.cell : joins(block, next);
}

// Line ends mid hyphenated word.
bool hyphenated(const QString &text)
{
    return text.size() > 1 && text.back() == u'-' && text[text.size() - 2].isLetterOrNumber();
}

int distance(const unsigned char *a, const unsigned char *b)
{
    return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
}

// Paper around, ink furthest.
std::pair<QRgb, QRgb> colours(const OcrImage &image, const QRect &box)
{
    const QRect area(0, 0, image.width, image.height);
    const QRect ring = box.adjusted(-kRing, -kRing, kRing, kRing).intersected(area);
    auto at = [&](int x, int y) { return image.samples + y * image.stride + x * 3; };
    auto luminance = [](const unsigned char *p) { return p[0] * 299 + p[1] * 587 + p[2] * 114; };
    std::vector<const unsigned char *> border;
    for (int x = ring.left(); x <= ring.right(); ++x) {
        border.push_back(at(x, ring.top()));
        border.push_back(at(x, ring.bottom()));
    }
    std::nth_element(border.begin(), border.begin() + qsizetype(border.size() / 2), border.end(),
                     [&](const unsigned char *a, const unsigned char *b) { return luminance(a) < luminance(b); });
    const unsigned char *paper = border[border.size() / 2];
    std::vector<const unsigned char *> inside;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        for (int x = box.left(); x <= box.right(); ++x)
            inside.push_back(at(x, y));
    }
    const size_t far = std::max<size_t>(1, inside.size() / kInkShare);
    std::partial_sort(inside.begin(), inside.begin() + qsizetype(far), inside.end(),
                      [&](const unsigned char *a, const unsigned char *b) {
                          return distance(a, paper) > distance(b, paper);
                      });
    const unsigned char *ink = inside[far / 2];
    return {qRgb(ink[0], ink[1], ink[2]), qRgb(paper[0], paper[1], paper[2])};
}

OcrBlock blockOf(const std::vector<const Segment *> &block, const OcrImage &image)
{
    QString text;
    QRectF box;
    std::vector<const OcrWord *> words;
    for (const Segment *segment : block) {
        for (const OcrWord *word : segment->words) {
            const bool joined = text.isEmpty() || (word == segment->words.front() && hyphenated(text));
            text += (joined ? QString() : QStringLiteral(" ")) + word->text;
        }
        words.insert(words.end(), segment->words.begin(), segment->words.end());
        box |= segment->box;
    }
    const double size = sizeOf(words);
    const OcrWord &first = *block.front()->words.front();
    const OcrWord &last = *block.back()->words.front();
    const double leading = block.size() > 1 ? (last.baseline.y() - first.baseline.y()) / double(block.size() - 1) : 0;
    const QRect pixels = box.toAlignedRect().intersected(QRect(0, 0, image.width, image.height));
    const auto [ink, paper] = colours(image, pixels);
    const double m = size * image.scale * kCoverMargin;
    return {text,
            image.toPoints(box).adjusted(-m, -m, m, m),
            first.baseline * image.scale + image.origin,
            first.angle,
            leading * image.scale,
            size * image.scale,
            ink,
            paper};
}

// Table cell of each word.
void placeInCells(std::vector<OcrWord> &words, const OcrImage &image)
{
    std::vector<QRectF> boxes;
    for (const OcrWord &word : words)
        boxes.push_back(image.toPoints(word.box));
    const std::vector<QRectF> cells = tableCells(boxes, rulesIn(image));
    for (size_t i = 0; i < words.size(); ++i) {
        const QRectF box = boxes[i].adjusted(kInside, kInside, -kInside, -kInside);
        const auto hit = std::ranges::find_if(cells, [&box](const QRectF &cell) { return cell.contains(box); });
        words[i].cell = hit == cells.end() ? -1 : int(hit - cells.begin());
    }
}

} // namespace

std::vector<OcrBlock> layoutBlocks(std::vector<OcrWord> words, const OcrImage &image)
{
    placeInCells(words, image);
    const std::vector<Segment> segments = segmentAll(words);
    std::vector<std::vector<const Segment *>> blocks;
    for (const Segment &segment : segments) {
        const auto open = std::ranges::find_if(blocks, [&](const auto &block) { return belongs(block, segment); });
        if (open != blocks.end())
            open->push_back(&segment);
        else
            blocks.push_back({&segment});
    }
    std::vector<OcrBlock> result;
    for (const auto &block : blocks)
        result.push_back(blockOf(block, image));
    return result;
}
