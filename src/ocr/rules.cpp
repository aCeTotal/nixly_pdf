#include "rules.h"

#include <algorithm>

namespace {

constexpr int kChannels = 3;
constexpr int kSample = 7;
constexpr int kContrast = 28;
constexpr double kRuleLength = 24;

// Pixels along row or column.
struct Strip
{
    const unsigned char *start;
    int count;
    int step;
};

int luminance(const unsigned char *p)
{
    return (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
}

// Median brightness of sparse grid.
int paperOf(const OcrImage &image)
{
    std::vector<int> samples;
    for (int y = 0; y < image.height; y += kSample) {
        for (int x = 0; x < image.width; x += kSample)
            samples.push_back(luminance(image.samples + y * image.stride + x * kChannels));
    }
    std::ranges::nth_element(samples, samples.begin() + qsizetype(samples.size() / 2));
    return samples[samples.size() / 2];
}

// Dark stretches of enough length.
std::vector<std::pair<int, int>> darkRuns(const Strip &strip, int ink, int length)
{
    std::vector<std::pair<int, int>> runs;
    int begin = -1;
    for (int i = 0; i <= strip.count; ++i) {
        const bool dark = i < strip.count && luminance(strip.start + qsizetype(i) * strip.step) <= ink;
        if (dark && begin < 0)
            begin = i;
        if (!dark && begin >= 0 && i - begin >= length)
            runs.emplace_back(begin, i);
        if (!dark)
            begin = -1;
    }
    return runs;
}

} // namespace

std::vector<QRectF> rulesIn(const OcrImage &image)
{
    const int ink = paperOf(image) - kContrast;
    const int length = int(kRuleLength / image.scale);
    std::vector<QRectF> rules;
    for (int y = 0; y < image.height; ++y) {
        const Strip row{image.samples + qsizetype(y) * image.stride, image.width, kChannels};
        for (const auto &[from, to] : darkRuns(row, ink, length))
            rules.push_back(image.toPoints(QRectF(from, y, to - from, 1)));
    }
    for (int x = 0; x < image.width; ++x) {
        const Strip column{image.samples + qsizetype(x) * kChannels, image.height, image.stride};
        for (const auto &[from, to] : darkRuns(column, ink, length))
            rules.push_back(image.toPoints(QRectF(x, from, 1, to - from)));
    }
    return rules;
}
