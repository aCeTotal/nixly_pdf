#pragma once

#include <QRectF>
#include <vector>

class Document;

// Vertical page stack, view pixels.
class PageLayout
{
public:
    static constexpr double kGap = 18;
    static constexpr double kMargin = 28;

    void build(const Document &doc, double zoom, double viewportWidth);

    int count() const { return int(rects.size()); }
    const QRectF &rect(int index) const { return rects[size_t(index)]; }
    double width() const { return extent.width(); }
    double height() const { return extent.height(); }

    // Page slot containing a height.
    int pageAt(double y) const;

private:
    std::vector<QRectF> rects;
    QSizeF extent;
};
