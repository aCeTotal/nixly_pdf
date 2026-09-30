#include "fixture.h"

#include "module/flatten.h"

#include <QFile>
#include <cstdio>

namespace {

int failures = 0;

} // namespace

void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    failures += ok ? 0 : 1;
}

int verdict()
{
    return failures == 0 ? 0 : 1;
}

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
