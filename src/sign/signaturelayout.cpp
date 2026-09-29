#include "signaturelayout.h"

#include "scriptfonts.h"

#include <QFile>
#include <QGlyphRun>
#include <QLocale>
#include <QPainter>
#include <algorithm>

namespace {

constexpr double kEm = 1000;
constexpr double kLabelRatio = 0.34;
constexpr double kLabelGap = 0.25;

} // namespace

SignatureLayout::SignatureLayout(const Signature &signature) : color(signature.color)
{
    const std::vector<ScriptFont> &fonts = scriptFonts();
    if (!fonts.empty() && !signature.name.isEmpty())
        add(fonts[size_t(std::clamp(signature.font, 0, int(fonts.size()) - 1))].path, signature.name, signature.size);
    const double label = signature.size * kLabelRatio;
    for (const QString &line : signature.note.split('\n', Qt::SkipEmptyParts))
        add(labelFont(), line, label);
    if (signature.dated)
        add(labelFont(), QLocale().toString(signature.date, QLocale::ShortFormat), label);

    for (Line &line : lines)
        line.baseline.ry() -= extent.height() / 2;
}

// Shapes and stacks one line.
void SignatureLayout::add(const QString &path, const QString &text, double size)
{
    const QRawFont design(path, kEm, QFont::PreferNoHinting);
    if (!design.isValid())
        return;
    const QList<quint32> glyphs = design.glyphIndexesForString(text);
    const QList<QPointF> kerned = design.advancesForGlyphIndexes(glyphs, QRawFont::KernedAdvances);
    const QList<QPointF> plain = design.advancesForGlyphIndexes(glyphs);
    const double scale = size / kEm;

    Line line{path, design, glyphs, {}, {}, {}};
    line.font.setPixelSize(size);
    double x = 0;
    for (qsizetype i = 0; i < glyphs.size(); ++i) {
        line.positions.append(QPointF(x * scale, 0));
        line.items.push_back({int(glyphs[i]), float(plain[i].x() - kerned[i].x())});
        x += kerned[i].x();
    }
    const double width = x * scale;
    const double gap = lines.empty() ? 0 : size * kLabelGap;
    const double top = extent.height() + gap;
    for (QPointF &p : line.positions)
        p.rx() -= width / 2;
    line.baseline = QPointF(-width / 2, top + design.ascent() * scale);
    extent = QSizeF(std::max(extent.width(), width), top + (design.ascent() + design.descent()) * scale);
    lines.push_back(std::move(line));
}

void SignatureLayout::paint(QPainter &painter) const
{
    painter.setPen(color);
    for (const Line &line : lines) {
        QGlyphRun run;
        run.setRawFont(line.font);
        run.setGlyphIndexes(line.glyphs);
        run.setPositions(line.positions);
        painter.drawGlyphRun(QPointF(0, line.baseline.y()), run);
    }
}

std::vector<StampLine> SignatureLayout::stampLines(QPointF centre) const
{
    std::vector<StampLine> stamped;
    for (const Line &line : lines) {
        stamped.push_back({QFile::encodeName(line.path), float(line.font.pixelSize()), centre + line.baseline,
                           color.rgb(), line.items});
    }
    return stamped;
}
