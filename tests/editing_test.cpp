#include "fixture.h"

#include "module/caret.h"
#include "module/textedit.h"

#include <QGuiApplication>
#include <cmath>

namespace {

void checkPush(FontLibrary &fonts)
{
    ModuleSet modules(fonts);
    Module top;
    top.anchor = QPointF(72, 100);
    top.text = QStringLiteral("Grows");
    top.font.family = QStringLiteral("Liberation Sans");
    Module under = top;
    under.anchor = QPointF(80, 160);
    Module aside = top;
    aside.anchor = QPointF(400, 160);
    const int grown = modules.add(top);
    const int below = modules.add(under);
    const int beside = modules.add(aside);
    top.id = grown;
    top.size *= 3;
    modules.update(top);
    check(modules.find(below)->anchor.y() > 160, "growing text pushes the module below");
    check(modules.find(beside)->anchor.y() == 160, "leaves other columns alone");
    top.size /= 3;
    modules.update(top);
    check(std::fabs(modules.find(below)->anchor.y() - 160) < 0.01, "shrinking pulls it back up");
}

void checkTextEdits()
{
    Module module;
    module.text = QStringLiteral("alpha beta gamma");
    module.spans = {{6, 4, {0, QStringLiteral("Courier"), false, false}, 0.8, Qt::red}};
    insertText(module, 8, QStringLiteral("XY"));
    check(module.text == "alpha beXYta gamma" && module.spans[0].length == 6, "typing inside a span keeps its style");
    insertText(module, 0, QStringLiteral(">> "));
    check(module.spans[0].start == 9, "typing before a span moves it");
    insertText(module, 15, QStringLiteral("!"));
    check(module.spans[0].length == 7, "typing at a span's end continues it");
    removeText(module, 9, 16);
    check(module.spans.empty() && module.text == ">> alpha  gamma", "deleting a span's text drops it");
}

void checkCaret(FontLibrary &fonts)
{
    Module module;
    module.text = QStringLiteral("one two three four five six");
    module.font.family = QStringLiteral("Liberation Sans");
    module.width = 60;
    const Layout layout = typeset(module, fonts);
    const bool wrapped = layout.baselines.size() > 1;
    check(wrapped && layout.stops.size() == size_t(module.text.size()) + 1, "records a caret stop per position");
    const qsizetype end = lineEnd(layout, 0);
    const bool split = layout.stops[size_t(end)].line == 0 && layout.stops[size_t(end) + 1].line == 1;
    check(wrapped && split, "line end sits before the wrap");
    const QPointF start(layout.stops[size_t(end) + 1].x + 1, layout.baselines[1]);
    check(positionAt(layout, start) == end + 1, "finds the position under a point");
    check(lineStart(layout, positionOnLine(layout, 1, 1000)) == end + 1, "moves along a wrapped line");
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    FontLibrary fonts;
    checkPush(fonts);
    checkTextEdits();
    checkCaret(fonts);
    return verdict();
}
