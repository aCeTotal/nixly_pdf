#include "fixture.h"

#include "module/pickup.h"
#include "ocr/ocr.h"
#include "pdf/pagetree.h"
#include "sign/signature.h"

#include <QGuiApplication>
#include <QTemporaryDir>
#include <set>

namespace {

void checkEncryption(Document &doc, ModuleSet &modules, const QString &path)
{
    const QString passphrase = QStringLiteral("correct horse battery");
    doc.protect(passphrase);
    check(save(doc, modules, path), "saves encrypted");
    QString error;
    std::unique_ptr<Document> locked = Document::open(path, &error);
    check(locked && locked->locked(), "encrypted file asks for the passphrase");
    check(locked && !locked->unlock(QStringLiteral("wrong horse battery")), "rejects a wrong passphrase");
    check(locked && locked->unlock(passphrase) && locked->count() == doc.count(), "opens with the passphrase");
    pdf_crypt *crypt = locked ? locked->pdf()->crypt : nullptr;
    fz_context *ctx = mainContext();
    const bool strong = crypt && pdf_crypt_revision(ctx, crypt) == 6 && pdf_crypt_length(ctx, crypt) == 256;
    check(strong, "encrypts with AES-256");
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    const QString source = dir.filePath("sample.pdf");
    writeSample(QFile::encodeName(source));

    QString error;
    std::unique_ptr<Document> doc = Document::open(source, &error);
    check(doc && doc->count() == 3, "opens a three page document");
    FontLibrary fonts;
    ModuleSet modules(fonts);

    std::vector<TextRun> runs = pageText(*doc, 0).runs;
    const TextRun *hello = findRun(runs, "Hello World");
    check(hello, "finds the text run");
    Lifted lifted = pickUp(*doc, fonts, {0, {{*hello}, {QString()}, {hello->box}, 0}});
    check(lifted.error.isEmpty() && lifted.module.text == "Hello World", "lifts text into a module");
    check(lifted.module.font.pageFont != 0, "module keeps the page font");
    check(!pageHas(*doc, 0, "Hello"), "lifted text leaves the page");
    Module hi = lifted.module;
    hi.text = "Hello Nixly";
    hi.anchor.rx() += 50;
    modules.add(hi);

    Module foreign;
    foreign.page = doc->slot(1).id;
    foreign.anchor = QPointF(72, 300);
    foreign.text = QString::fromUtf8("Привет Blåbær");
    foreign.font.family = "Liberation Sans";
    foreign.cover = QRectF(70, 120, 140, 30);
    foreign.coverColor = Qt::red;
    check(!typeset(foreign, fonts).runs.empty(), "lays out text in a fallback font");
    modules.add(foreign);

    Module wrapped = foreign;
    wrapped.text = "one two three four five six seven eight";
    wrapped.width = 60;
    wrapped.anchor = QPointF(72, 500);
    std::set<double> baselines;
    for (const GlyphRun &run : typeset(wrapped, fonts).runs)
        baselines.insert(run.origin.y());
    check(baselines.size() > 1, "wraps long text");
    modules.add(wrapped);

    Module signature = signatureModule({"Ada Lovelace", 0, Qt::darkBlue, 30});
    signature.page = doc->slot(2).id;
    signature.anchor = QPointF(100, 400);
    check(!typeset(signature, fonts).runs.empty(), "lays out a signature");
    modules.add(signature);

    const int moved = doc->slot(0).id;
    check(movePage(*doc, 0, 2, &error) && doc->slot(2).id == moved, "moves a page");
    check(insertBlankPage(*doc, 0, &error) && doc->count() == 4, "adds a blank page");
    check(deletePage(*doc, 0, &error) && doc->count() == 3, "deletes a page");

    const QString saved = dir.filePath("saved.pdf");
    check(save(*doc, modules, saved), "saves with modules burnt in");
    check(!pageHas(*doc, 2, "Nixly"), "live document stays editable");
    check(save(*doc, modules, saved), "saves again over itself");

    std::unique_ptr<Document> reopened = Document::open(saved, &error);
    check(reopened && reopened->count() == 3, "reopens the saved file");
    const std::vector<TextRun> first = pageText(*reopened, 2).runs;
    const TextRun *nixly = findRun(first, "Hello Nixly");
    check(nixly, "edited text is on the page");
    check(nixly && nixly->box.left() > hello->box.left() + 40, "moved text keeps its new place");
    check(pageHas(*reopened, 0, QString::fromUtf8("Привет")), "fallback text is on the page");
    check(pageHas(*reopened, 1, "Ada"), "signature is on the page");
    check(pixel(saved, 0, QPoint(100, 135)) == qRgb(255, 0, 0), "cover patch is burnt in");
    checkEncryption(*doc, modules, dir.filePath("locked.pdf"));

    const QString scan = dir.filePath("scan.pdf");
    writeScan(QFile::encodeName(source), QFile::encodeName(scan));
    std::unique_ptr<Document> scanned = Document::open(scan, &error);
    check(scanned && pageText(*scanned, 0).runs.empty(), "scan has no text layer");
    const OcrResult ocr = recognize(*scanned, 0);
    const bool read = !ocr.blocks.empty() && ocr.blocks.front().text.contains("Hello");
    check(ocr.error.isEmpty() && read, "recognises text in a scan");
    check(read && ocr.blocks.front().size > 18 && ocr.blocks.front().size < 30, "estimates the font size");
    return verdict();
}
