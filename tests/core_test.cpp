#include "edit/runcache.h"
#include "mark/annotate.h"
#include "mark/pickmarks.h"
#include "module/flatten.h"
#include "module/pickup.h"
#include "pdf/document.h"
#include "ocr/ocr.h"
#include "pdf/pagetree.h"
#include "sign/signature.h"

#include <QGuiApplication>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    failures += ok ? 0 : 1;
}

// Mixed fonts, code box, table.
constexpr const char *kLayout =
    "BT /F1 11 Tf 72 700 Td (Alpha beta ) Tj /F2 9 Tf (gamma) Tj /F1 11 Tf ( delta) Tj 0 -14 Td (epsilon zeta) Tj ET\n"
    "0.9 g 60 560 300 80 re f 0 g\n"
    "BT /F2 10 Tf 72 620 Td (int main\\(\\)) Tj 0 -12 Td ({) Tj 0 -12 Td (    ) Tj 1 0 0 rg (return) Tj 0 g ( 0;) Tj\n"
    "0 -24 Td (}) Tj ET\n"
    "60 400 0.75 40 re f 100 400 0.75 40 re f 200 400 0.75 40 re f 60 440 140.75 0.75 re f 60 400 140.75 0.75 re f\n"
    "BT /F1 9 Tf 66 428 Td (Cell-) Tj 0 -12 Td (one) Tj ET BT /F1 9 Tf 106 428 Td (Two) Tj ET";

// Helvetica F1, Courier F2 pages.
void writePages(const QByteArray &path, const std::vector<QByteArray> &contents)
{
    fz_context *ctx = mainContext();
    pdf_document *doc = pdf_create_document(ctx);
    fz_font *sans = fz_new_base14_font(ctx, "Helvetica");
    fz_font *mono = fz_new_base14_font(ctx, "Courier");
    pdf_obj *f1 = pdf_add_simple_font(ctx, doc, sans, PDF_SIMPLE_ENCODING_LATIN);
    pdf_obj *f2 = pdf_add_simple_font(ctx, doc, mono, PDF_SIMPLE_ENCODING_LATIN);
    for (const QByteArray &content : contents) {
        pdf_obj *resources = pdf_new_dict(ctx, doc, 1);
        pdf_dict_putp(ctx, resources, "Font/F1", f1);
        pdf_dict_putp(ctx, resources, "Font/F2", f2);
        const auto *bytes = reinterpret_cast<const unsigned char *>(content.constData());
        fz_buffer *buffer = fz_new_buffer_from_copied_data(ctx, bytes, size_t(content.size()));
        pdf_obj *page = pdf_add_page(ctx, doc, fz_make_rect(0, 0, 595, 842), 0, resources, buffer);
        pdf_insert_page(ctx, doc, -1, page);
        pdf_drop_obj(ctx, page);
        fz_drop_buffer(ctx, buffer);
        pdf_drop_obj(ctx, resources);
    }
    pdf_save_document(ctx, doc, path.constData(), &pdf_default_write_options);
    pdf_drop_obj(ctx, f2);
    pdf_drop_obj(ctx, f1);
    fz_drop_font(ctx, mono);
    fz_drop_font(ctx, sans);
    pdf_drop_document(ctx, doc);
}

void writeSample(const QByteArray &path)
{
    std::vector<QByteArray> pages;
    for (const char *text : {"Hello World", "Second page", "Third page"})
        pages.push_back(QByteArray("BT /F1 24 Tf 72 700 Td (") + text + ") Tj ET");
    writePages(path, pages);
}

// Page one as a scan.
void writeScan(const QByteArray &source, const QByteArray &path)
{
    fz_context *ctx = mainContext();
    fz_document *text = fz_open_document(ctx, source.constData());
    fz_pixmap *pix = fz_new_pixmap_from_page_number(ctx, text, 0, fz_scale(2, 2), fz_device_rgb(ctx), 0);
    fz_image *image = fz_new_image_from_pixmap(ctx, pix, nullptr);
    pdf_document *doc = pdf_create_document(ctx);
    pdf_obj *resources = pdf_new_dict(ctx, doc, 1);
    pdf_obj *added = pdf_add_image(ctx, doc, image);
    pdf_dict_putp(ctx, resources, "XObject/Im0", added);
    fz_buffer *contents = fz_new_buffer(ctx, 64);
    fz_append_string(ctx, contents, "q 595 0 0 842 0 0 cm /Im0 Do Q");
    pdf_obj *page = pdf_add_page(ctx, doc, fz_make_rect(0, 0, 595, 842), 0, resources, contents);
    pdf_insert_page(ctx, doc, -1, page);
    pdf_save_document(ctx, doc, path.constData(), &pdf_default_write_options);
    pdf_drop_obj(ctx, page);
    fz_drop_buffer(ctx, contents);
    pdf_drop_obj(ctx, added);
    pdf_drop_obj(ctx, resources);
    pdf_drop_document(ctx, doc);
    fz_drop_image(ctx, image);
    fz_drop_pixmap(ctx, pix);
    fz_drop_document(ctx, text);
}

const TextRun *findRun(const std::vector<TextRun> &runs, const QString &text)
{
    for (const TextRun &run : runs) {
        if (run.text.contains(text))
            return &run;
    }
    return nullptr;
}

bool pageHas(Document &doc, int index, const QString &text)
{
    return findRun(pageText(doc, index).runs, text) != nullptr;
}

QRgb pixel(const QString &path, int page, QPoint at)
{
    fz_context *ctx = mainContext();
    fz_document *doc = fz_open_document(ctx, QFile::encodeName(path).constData());
    fz_pixmap *pix = fz_new_pixmap_from_page_number(ctx, doc, page, fz_identity, fz_device_rgb(ctx), 0);
    const unsigned char *p = fz_pixmap_samples(ctx, pix) + at.y() * fz_pixmap_stride(ctx, pix) + at.x() * 3;
    const QRgb colour = qRgb(p[0], p[1], p[2]);
    fz_drop_pixmap(ctx, pix);
    fz_drop_document(ctx, doc);
    return colour;
}

bool save(Document &doc, ModuleSet &modules, const QString &path)
{
    QString error;
    return doc.save(path, [&](fz_context *ctx, pdf_document *copy) { return flatten(ctx, copy, {doc, modules}); },
                    &error);
}

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

bool near(QPointF a, QPointF b)
{
    return QLineF(a, b).length() < 0.5;
}

bool sameBox(const QRectF &a, const QRectF &b)
{
    return near(a.topLeft(), b.topLeft()) && near(a.bottomRight(), b.bottomRight());
}

const Mark *findMark(const std::vector<Mark> &marks, MarkKind kind)
{
    const auto hit = std::ranges::find(marks, kind, &Mark::kind);
    return hit == marks.end() ? nullptr : &*hit;
}

std::vector<Mark> sampleMarks(int page)
{
    Mark box;
    box.page = page;
    box.box = QRectF(60, 60, 120, 80);
    Mark ring = box;
    ring.kind = MarkKind::Ellipse;
    ring.box.translate(0, 120);
    Mark cloud = box;
    cloud.kind = MarkKind::Cloud;
    cloud.box.translate(200, 0);
    cloud.ink = Qt::blue;
    Mark arrow = box;
    arrow.kind = MarkKind::Arrow;
    arrow.tail = QPointF(300, 300);
    arrow.tip = QPointF(400, 250);
    Mark callout = box;
    callout.kind = MarkKind::Callout;
    callout.box = QRectF(250, 400, 180, 60);
    callout.tip = QPointF(120, 520);
    callout.text = QStringLiteral("Look here");
    callout.size = 14;
    callout.fill = Qt::white;
    Mark picture = box;
    picture.kind = MarkKind::Image;
    picture.box = QRectF(60, 600, 100, 50);
    picture.image = QImage(40, 20, QImage::Format_RGB32);
    picture.image.fill(Qt::green);
    return {box, ring, cloud, arrow, callout, picture};
}

void checkMarks(const QString &source, const QString &path, FontLibrary &fonts)
{
    QString error;
    std::unique_ptr<Document> doc = Document::open(source, &error);
    ModuleSet modules(fonts);
    MarkSet marks;
    for (const Mark &mark : sampleMarks(doc->slot(0).id))
        marks.add(mark);
    const bool saved = doc->save(path, [&](fz_context *ctx, pdf_document *copy) {
        return annotate(ctx, copy, {*doc, marks});
    }, &error);
    check(saved, "saves marks as annotations");
    std::unique_ptr<Document> reopened = Document::open(path, &error);
    const std::vector<Mark> read = reopened ? pickMarks(*reopened) : std::vector<Mark>();
    check(read.size() == 6, "reads every mark back");
    const std::vector<Mark> wrote = sampleMarks(0);
    const Mark *box = findMark(read, MarkKind::Rectangle);
    check(box && sameBox(box->box, wrote[0].box) && box->ink == wrote[0].ink, "rectangle keeps place and colour");
    const Mark *ring = findMark(read, MarkKind::Ellipse);
    check(ring && sameBox(ring->box, wrote[1].box), "ellipse keeps its place");
    const Mark *cloud = findMark(read, MarkKind::Cloud);
    check(cloud && sameBox(cloud->box, wrote[2].box) && cloud->ink == Qt::blue, "cloud keeps place and colour");
    const Mark *arrow = findMark(read, MarkKind::Arrow);
    check(arrow && near(arrow->tail, wrote[3].tail) && near(arrow->tip, wrote[3].tip), "arrow keeps its direction");
    const Mark *callout = findMark(read, MarkKind::Callout);
    const bool bubble = callout && sameBox(callout->box, wrote[4].box) && near(callout->tip, wrote[4].tip);
    check(bubble && callout->text == "Look here" && callout->size == 14, "callout keeps box, tip and text");
    const Mark *picture = findMark(read, MarkKind::Image);
    const bool green = picture && qGreen(picture->image.pixel(picture->image.rect().center())) > 200;
    check(picture && sameBox(picture->box, wrote[5].box) && green, "image keeps place and pixels");
    check(reopened && pickMarks(*reopened).empty(), "lifted annotations leave the page");
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
    checkLayout(dir.filePath("layout.pdf"), fonts);
    checkPush(fonts);
    checkScannedTable(dir);
    checkMarks(source, dir.filePath("marked.pdf"), fonts);
    return failures == 0 ? 0 : 1;
}
