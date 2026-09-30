#include "fixture.h"

#include "edit/runcache.h"
#include "module/pickup.h"
#include "ocr/ocr.h"

#include <QGuiApplication>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>

namespace {

// Mixed fonts, code box, table.
constexpr const char *kLayout =
    "BT /F1 11 Tf 72 700 Td (Alpha beta ) Tj /F2 9 Tf (gamma) Tj /F1 11 Tf ( delta) Tj 0 -14 Td (epsilon zeta) Tj ET\n"
    "0.9 g 60 560 300 80 re f 0 g\n"
    "BT /F2 10 Tf 72 620 Td (int main\\(\\)) Tj 0 -12 Td ({) Tj 0 -12 Td (    ) Tj 1 0 0 rg (return) Tj 0 g ( 0;) Tj\n"
    "0 -24 Td (}) Tj ET\n"
    "60 400 0.75 40 re f 100 400 0.75 40 re f 200 400 0.75 40 re f 60 440 140.75 0.75 re f 60 400 140.75 0.75 re f\n"
    "BT /F1 9 Tf 66 428 Td (Cell-) Tj 0 -12 Td (one) Tj ET BT /F1 9 Tf 106 428 Td (Two) Tj ET";

// Lifts passage under a point.
struct Picker
{
    Document &doc;
    FontLibrary &fonts;
    RunCache cache;

    Module at(QPointF point)
    {
        const Passage passage = cache.hit(0, point);
        if (passage.runs.empty())
            return {};
        const Module module = pickUp(doc, fonts, {0, passage}).module;
        cache.forget(doc.slot(0).id);
        return module;
    }
};

bool spanned(const Module &module, const QString &text)
{
    return module.spans.size() == 1 && module.text.mid(module.spans[0].start, module.spans[0].length) == text;
}

void checkLayout(const QString &path, FontLibrary &fonts)
{
    writePages(QFile::encodeName(path), {kLayout});
    QString error;
    std::unique_ptr<Document> doc = Document::open(path, &error);
    Picker picker{*doc, fonts, {}};
    picker.cache.setDocument(doc.get());
    const Module para = picker.at(QPointF(80, 138));
    check(para.text == "Alpha beta gamma delta epsilon zeta", "lifts a paragraph across fonts");
    check(spanned(para, "gamma") && para.spans[0].font != para.font, "inline code keeps its font");
    check(spanned(para, "gamma") && std::fabs(para.spans[0].scale - 9.0 / 11) < 0.01, "inline code keeps its size");
    const Module box = picker.at(QPointF(80, 219));
    check(box.text == "int main()\n{\n    return 0;\n\n}", "lifts a boxed block line by line");
    check(spanned(box, "return") && box.spans[0].color == Qt::red, "boxed block keeps its colours");
    check(picker.at(QPointF(70, 411)).text == "Cell-one", "lifts a table cell as one");
    check(picker.at(QPointF(110, 411)).text == "Two", "keeps table cells apart");

    ModuleSet modules(fonts);
    modules.add(para);
    modules.add(box);
    const QString saved = path + QStringLiteral(".saved.pdf");
    check(save(*doc, modules, saved), "saves mixed-font modules");
    std::unique_ptr<Document> reopened = Document::open(saved, &error);
    const std::vector<TextRun> runs = reopened ? pageText(*reopened, 0).runs : std::vector<TextRun>();
    const TextRun *gamma = findRun(runs, "gamma");
    check(gamma && gamma->font.contains("Courier"), "burns inline code in its own font");
    const TextRun *red = findRun(runs, "return");
    check(red && red->color == qRgb(255, 0, 0), "burns span colours");
}

// Two ruled cells, one wrapped.
constexpr const char *kTable =
    "0 g 60 500 1 120 re f 260 500 1 120 re f 460 500 1 120 re f 60 620 401 1 re f 60 500 401 1 re f\n"
    "BT /F1 18 Tf 80 590 Td (Alpha) Tj 0 -24 Td (beta) Tj ET BT /F1 18 Tf 280 590 Td (Gamma) Tj ET";

bool hasBlock(const OcrResult &ocr, const QString &text)
{
    return std::ranges::any_of(ocr.blocks, [&text](const OcrBlock &block) { return block.text == text; });
}

void checkScannedTable(const QTemporaryDir &dir)
{
    const QByteArray table = QFile::encodeName(dir.filePath("table.pdf"));
    const QByteArray scan = QFile::encodeName(dir.filePath("table-scan.pdf"));
    writePages(table, {kTable});
    writeScan(table, scan);
    QString error;
    std::unique_ptr<Document> doc = Document::open(QFile::decodeName(scan), &error);
    const OcrResult ocr = doc ? recognize(*doc, 0) : OcrResult();
    check(hasBlock(ocr, QStringLiteral("Alpha beta")), "recognises a scanned table cell as one block");
    check(hasBlock(ocr, QStringLiteral("Gamma")), "keeps scanned cells apart");
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    FontLibrary fonts;
    checkLayout(dir.filePath("layout.pdf"), fonts);
    checkScannedTable(dir);
    return verdict();
}
