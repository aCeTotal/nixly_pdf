#include "modulepaint.h"

#include <QGlyphRun>
#include <QPainter>

QTransform moduleTransform(const Module &module)
{
    QTransform transform;
    transform.translate(module.anchor.x(), module.anchor.y());
    transform.rotateRadians(module.angle);
    transform.scale(module.stretch, 1);
    return transform;
}

QColor inkOf(const Module &module, int style)
{
    const QColor own = style ? module.spans[size_t(style - 1)].color : QColor();
    return own.isValid() ? own : module.color;
}

void paintModule(QPainter &painter, const Module &module, const Layout &layout)
{
    if (!module.cover.isEmpty())
        painter.fillRect(module.cover, module.coverColor);
    painter.save();
    painter.setTransform(moduleTransform(module), true);
    for (const GlyphRun &run : layout.runs) {
        QGlyphRun glyphs;
        glyphs.setRawFont(run.sized);
        glyphs.setGlyphIndexes(run.gids);
        glyphs.setPositions(run.positions);
        painter.setPen(inkOf(module, run.style));
        painter.drawGlyphRun(run.origin, glyphs);
    }
    painter.restore();
}
