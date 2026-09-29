#include "pdf/document.h"
#include "pdf/pagetree.h"
#include "pdf/stamp.h"
#include "pdf/textedit.h"
#include "sign/signaturelayout.h"

#include <QGuiApplication>
#include <QTemporaryDir>
#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    failures += ok ? 0 : 1;
}

void addPage(fz_context *ctx, pdf_document *doc, pdf_obj *font, const char *text)
{
    pdf_obj *resources = pdf_new_dict(ctx, doc, 1);
    pdf_dict_putp(ctx, resources, "Font/F1", font);
    fz_buffer *contents = fz_new_buffer(ctx, 64);
    fz_append_printf(ctx, contents, "BT /F1 24 Tf 72 700 Td (%s) Tj ET", text);
    pdf_obj *page = pdf_add_page(ctx, doc, fz_make_rect(0, 0, 595, 842), 0, resources, contents);
    pdf_insert_page(ctx, doc, -1, page);
    pdf_drop_obj(ctx, page);
    fz_drop_buffer(ctx, contents);
    pdf_drop_obj(ctx, resources);
}

void writeSample(const QByteArray &path)
{
    fz_context *ctx = mainContext();
    pdf_document *doc = pdf_create_document(ctx);
    fz_font *helvetica = fz_new_base14_font(ctx, "Helvetica");
    pdf_obj *font = pdf_add_simple_font(ctx, doc, helvetica, PDF_SIMPLE_ENCODING_LATIN);
    addPage(ctx, doc, font, "Hello World");
    addPage(ctx, doc, font, "Second page");
    addPage(ctx, doc, font, "Third page");
    pdf_save_document(ctx, doc, path.constData(), &pdf_default_write_options);
    pdf_drop_obj(ctx, font);
    fz_drop_font(ctx, helvetica);
    pdf_drop_document(ctx, doc);
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
    return findRun(pageRuns(doc, index), text) != nullptr;
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

    std::vector<TextRun> runs = pageRuns(*doc, 0);
    const TextRun *hello = findRun(runs, "Hello World");
    check(hello, "finds the text run");

    const EditOutcome same = replaceRun(*doc, 0, *hello, "Hello Nixly");
    check(same.error.isEmpty() && same.substitute.isEmpty(), "rewrites text in the page font");
    check(pageHas(*doc, 0, "Hello Nixly") && !pageHas(*doc, 0, "Hello World"), "old text replaced");

    runs = pageRuns(*doc, 1);
    const EditOutcome foreign = replaceRun(*doc, 1, *findRun(runs, "Second"), QString::fromUtf8("Привет"));
    check(foreign.error.isEmpty() && !foreign.substitute.isEmpty(), "falls back for missing glyphs");
    check(pageHas(*doc, 1, QString::fromUtf8("Привет")), "fallback text is extractable");

    Signature signature;
    signature.name = "Ada Lovelace";
    const SignatureLayout layout(signature);
    check(!layout.size().isEmpty(), "lays out a signature");
    check(placeStamp(*doc, 2, layout.stampLines(QPointF(300, 400))).isEmpty(), "stamps the signature");
    check(pageHas(*doc, 2, "Ada"), "signature is on the page");

    const int moved = doc->slot(0).id;
    check(movePage(*doc, 0, 2, &error) && doc->slot(2).id == moved, "moves a page");
    check(insertBlankPage(*doc, 0, &error) && doc->count() == 4, "adds a blank page");
    check(deletePage(*doc, 0, &error) && doc->count() == 3, "deletes a page");

    const QString saved = dir.filePath("saved.pdf");
    check(doc->save(saved, &error), "saves");
    runs = pageRuns(*doc, 1);
    const TextRun *third = findRun(runs, "Third");
    check(third && replaceRun(*doc, 1, *third, "Third, edited after saving").error.isEmpty(), "edits after saving");
    check(doc->save(saved, &error), "saves again over itself");
    doc.reset();
    std::unique_ptr<Document> reopened = Document::open(saved, &error);
    check(reopened && reopened->count() == 3, "reopens the saved file");
    check(pageHas(*reopened, 2, "Hello Nixly"), "edits survive saving");
    check(pageHas(*reopened, 1, "Ada"), "signature survives saving");
    check(pageHas(*reopened, 1, "edited after saving"), "second save kept the edit");
    return failures == 0 ? 0 : 1;
}
