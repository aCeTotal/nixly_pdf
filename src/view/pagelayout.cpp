#include "pagelayout.h"

#include "pdf/document.h"

#include <algorithm>

void PageLayout::build(const Document &doc, double zoom, double viewportWidth)
{
    double widest = 0;
    for (int i = 0; i < doc.count(); ++i)
        widest = std::max(widest, doc.slot(i).size.width() * zoom);
    const double width = std::max(viewportWidth, widest + 2 * kMargin);

    rects.resize(size_t(doc.count()));
    double y = kMargin;
    for (int i = 0; i < doc.count(); ++i) {
        const QSizeF size = doc.slot(i).size * zoom;
        rects[size_t(i)] = QRectF((width - size.width()) / 2, y, size.width(), size.height());
        y += size.height() + kGap;
    }
    extent = QSizeF(width, y - kGap + kMargin);
}

int PageLayout::pageAt(double y) const
{
    const auto after = std::upper_bound(rects.begin(), rects.end(), y,
                                        [](double value, const QRectF &r) { return value < r.top(); });
    return std::max(0, int(after - rects.begin()) - 1);
}
