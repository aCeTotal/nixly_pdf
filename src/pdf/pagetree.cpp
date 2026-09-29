#include "pagetree.h"

#include "document.h"

#include <QFile>

namespace {

constexpr fz_rect kA4{0, 0, 595.276f, 841.89f};

void move(fz_context *ctx, pdf_document *doc, int from, int to)
{
    pdf_obj *page = pdf_keep_obj(ctx, pdf_lookup_page_obj(ctx, doc, from));
    fz_try(ctx)
    {
        pdf_flatten_inheritable_page_items(ctx, page);
        pdf_delete_page(ctx, doc, from);
        pdf_insert_page(ctx, doc, to, page);
    }
    fz_always(ctx)
        pdf_drop_obj(ctx, page);
    fz_catch(ctx)
        fz_rethrow(ctx);
}

void insertBlank(fz_context *ctx, pdf_document *doc, int at, fz_rect box)
{
    pdf_obj *resources = pdf_new_dict(ctx, doc, 1);
    fz_buffer *contents = nullptr;
    pdf_obj *page = nullptr;
    fz_var(contents);
    fz_var(page);
    fz_try(ctx)
    {
        contents = fz_new_buffer(ctx, 1);
        page = pdf_add_page(ctx, doc, box, 0, resources, contents);
        pdf_insert_page(ctx, doc, at, page);
    }
    fz_always(ctx)
    {
        pdf_drop_obj(ctx, page);
        fz_drop_buffer(ctx, contents);
        pdf_drop_obj(ctx, resources);
    }
    fz_catch(ctx)
        fz_rethrow(ctx);
}

void graftAll(fz_context *ctx, pdf_document *doc, int at, const char *path)
{
    pdf_document *source = pdf_open_document(ctx, path);
    pdf_graft_map *map = nullptr;
    fz_var(map);
    fz_try(ctx)
    {
        if (pdf_needs_password(ctx, source))
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "The document is password protected");
        map = pdf_new_graft_map(ctx, doc);
        const int total = pdf_count_pages(ctx, source);
        for (int i = 0; i < total; ++i)
            pdf_graft_mapped_page(ctx, map, at + i, source, i);
    }
    fz_always(ctx)
    {
        pdf_drop_graft_map(ctx, map);
        pdf_drop_document(ctx, source);
    }
    fz_catch(ctx)
        fz_rethrow(ctx);
}

fz_rect neighbourBox(const Document &doc, int at)
{
    if (doc.count() == 0)
        return kA4;
    const QSizeF size = doc.slot(qBound(0, at - 1, doc.count() - 1)).size;
    return {0, 0, float(size.width()), float(size.height())};
}

} // namespace

bool movePage(Document &doc, int from, int to, QString *error)
{
    return doc.modify(error, [=](fz_context *ctx, pdf_document *pdf) {
        return attempt(ctx, [=] { move(ctx, pdf, from, to); });
    });
}

bool insertBlankPage(Document &doc, int at, QString *error)
{
    const fz_rect box = neighbourBox(doc, at);
    return doc.modify(error, [=](fz_context *ctx, pdf_document *pdf) {
        return attempt(ctx, [=] { insertBlank(ctx, pdf, at, box); });
    });
}

bool insertPdf(Document &doc, int at, const QString &path, QString *error)
{
    const QByteArray file = QFile::encodeName(path);
    const char *name = file.constData();
    return doc.modify(error, [=](fz_context *ctx, pdf_document *pdf) {
        return attempt(ctx, [=] { graftAll(ctx, pdf, at, name); });
    });
}

bool deletePage(Document &doc, int index, QString *error)
{
    return doc.modify(error, [=](fz_context *ctx, pdf_document *pdf) {
        return attempt(ctx, [=] { pdf_delete_page(ctx, pdf, index); });
    });
}
