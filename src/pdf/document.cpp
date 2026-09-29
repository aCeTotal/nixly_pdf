#include "document.h"

#include <QFile>
#include <filesystem>

namespace {

pdf_write_options saveOptions()
{
    pdf_write_options opts = pdf_default_write_options;
    opts.do_garbage = 1;
    opts.do_compress = 1;
    opts.do_compress_images = 1;
    opts.do_compress_fonts = 1;
    opts.do_use_objstms = 1;
    return opts;
}

// Saves with fonts subset.
void saveSubset(fz_context *ctx, pdf_document *doc, const char *path, const pdf_write_options *opts)
{
    fz_buffer *buffer = fz_new_buffer(ctx, 1 << 20);
    fz_output *out = nullptr;
    fz_stream *stream = nullptr;
    pdf_document *copy = nullptr;
    fz_var(out);
    fz_var(stream);
    fz_var(copy);
    fz_try(ctx)
    {
        out = fz_new_output_with_buffer(ctx, buffer);
        pdf_write_document(ctx, doc, out, opts);
        fz_close_output(ctx, out);
        stream = fz_open_buffer(ctx, buffer);
        copy = pdf_open_document_with_stream(ctx, stream);
        pdf_subset_fonts(ctx, copy, 0, nullptr);
        pdf_save_document(ctx, copy, path, opts);
    }
    fz_always(ctx)
    {
        pdf_drop_document(ctx, copy);
        fz_drop_stream(ctx, stream);
        fz_drop_output(ctx, out);
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx)
        fz_rethrow(ctx);
}

void write(fz_context *ctx, pdf_document *doc, const char *path, bool subset)
{
    const pdf_write_options opts = saveOptions();
    if (subset)
        saveSubset(ctx, doc, path, &opts);
    else
        pdf_save_document(ctx, doc, path, &opts);
}

} // namespace

std::unique_ptr<Document> Document::open(const QString &path, QString *error)
{
    fz_context *ctx = mainContext();
    const QByteArray file = QFile::encodeName(path);
    pdf_document *handle = nullptr;
    fz_try(ctx)
        handle = pdf_open_document(ctx, file.constData());
    fz_catch(ctx)
    {
        *error = takeError(ctx);
        return nullptr;
    }
    return std::unique_ptr<Document>(new Document(handle, path));
}

Document::Document(pdf_document *handle, const QString &path)
    : handle(handle), filePath(path), encrypted(pdf_needs_password(mainContext(), handle))
{
    if (!encrypted)
        reload();
}

Document::~Document()
{
    pdf_drop_document(ctx(), handle);
}

bool Document::locked() const
{
    return encrypted && pages.empty();
}

bool Document::unlock(const QString &password)
{
    if (!pdf_authenticate_password(ctx(), handle, password.toUtf8().constData()))
        return false;
    reload();
    return true;
}

void Document::reload()
{
    fz_context *c = ctx();
    const int total = pdf_count_pages(c, handle);
    pages.clear();
    pages.reserve(size_t(total));
    for (int i = 0; i < total; ++i) {
        pdf_obj *page = pdf_lookup_page_obj(c, handle, i);
        fz_rect box;
        fz_matrix ctm;
        pdf_page_obj_transform(c, page, &box, &ctm);
        pages.push_back({pdf_to_num(c, page), QSizeF(box.x1 - box.x0, box.y1 - box.y0)});
    }
}

void Document::markFontEmbedded()
{
    fontsEmbedded = true;
}

int Document::indexOf(int id) const
{
    for (size_t i = 0; i < pages.size(); ++i) {
        if (pages[i].id == id)
            return int(i);
    }
    return -1;
}

bool Document::save(const QString &path, QString *error)
{
    const std::filesystem::path target = QFile::encodeName(path).toStdString();
    std::filesystem::path partial = target;
    partial += ".part";
    const bool subset = fontsEmbedded && !encrypted;
    std::error_code failure;
    std::lock_guard hold(guard);
    fz_context *c = ctx();
    *error = attempt(c, [&] { write(c, handle, partial.c_str(), subset); });
    if (!error->isEmpty()) {
        std::filesystem::remove(partial, failure);
        return false;
    }
    std::filesystem::rename(partial, target, failure);
    if (failure) {
        *error = QString::fromStdString(failure.message());
        std::filesystem::remove(partial, failure);
        return false;
    }
    filePath = path;
    reload();
    return true;
}
