#include "content.h"

#include <QByteArray>
#include <cmath>

namespace {

constexpr float kOriginTolerance = 0.35f;

struct Rewrite
{
    const std::vector<fz_point> *removed;
    const std::vector<TextSpan> *spans;
    std::vector<QByteArray> names;
};

int dropGlyph(fz_context *, void *opaque, int *, int, fz_matrix trm, fz_matrix ctm, fz_rect, int, float, float)
{
    const auto *rewrite = static_cast<const Rewrite *>(opaque);
    const fz_matrix m = fz_concat(trm, ctm);
    for (const fz_point &p : *rewrite->removed) {
        if (std::fabs(p.x - m.e) < kOriginTolerance && std::fabs(p.y - m.f) < kOriginTolerance)
            return 1;
    }
    return 0;
}

void appendItems(fz_context *ctx, fz_buffer *out, const TextSpan &span)
{
    const char *format = span.codeBytes == 1 ? "%02x" : "%04x";
    bool open = false;
    fz_append_byte(ctx, out, '[');
    for (const TjItem &item : span.items) {
        if (item.code >= 0 && !open)
            fz_append_byte(ctx, out, '<');
        if (item.code >= 0) {
            fz_append_printf(ctx, out, format, item.code);
            open = true;
        }
        if (item.shift == 0)
            continue;
        if (open)
            fz_append_byte(ctx, out, '>');
        open = false;
        fz_append_printf(ctx, out, " %g ", item.shift);
    }
    if (open)
        fz_append_byte(ctx, out, '>');
    fz_append_string(ctx, out, "] TJ\n");
}

void appendSpans(fz_context *ctx, fz_buffer *out, void *opaque)
{
    const auto *rewrite = static_cast<const Rewrite *>(opaque);
    for (size_t i = 0; i < rewrite->spans->size(); ++i) {
        const TextSpan &span = (*rewrite->spans)[i];
        const fz_matrix &m = span.matrix;
        fz_append_printf(ctx, out, "\nq %g %g %g rg BT /%s 1 Tf %g %g %g %g %g %g Tm ",
                         qRed(span.color) / 255.0f, qGreen(span.color) / 255.0f, qBlue(span.color) / 255.0f,
                         rewrite->names[i].constData(), m.a, m.b, m.c, m.d, m.e, m.f);
        appendItems(ctx, out, span);
        fz_append_string(ctx, out, "ET Q\n");
    }
}

QByteArray freeName(fz_context *ctx, pdf_obj *fonts, int *serial)
{
    QByteArray name;
    do
        name = "NxF" + QByteArray::number(++*serial);
    while (pdf_dict_gets(ctx, fonts, name.constData()));
    return name;
}

} // namespace

QString rewritePage(fz_context *ctx, pdf_document *doc, pdf_page *page, const std::vector<fz_point> &removed,
                    const std::vector<TextSpan> &spans)
{
    Rewrite rewrite{&removed, &spans, {}};
    pdf_obj *oldFonts = pdf_dict_get(ctx, pdf_page_resources(ctx, page), PDF_NAME(Font));
    int serial = 0;
    for (size_t i = 0; i < spans.size(); ++i)
        rewrite.names.push_back(freeName(ctx, oldFonts, &serial));

    pdf_sanitize_filter_options sanitize{};
    sanitize.opaque = &rewrite;
    sanitize.text_filter = removed.empty() ? nullptr : dropGlyph;
    pdf_filter_factory filters[] = {{pdf_new_sanitize_filter, &sanitize}, {nullptr, nullptr}};
    pdf_filter_options options{};
    options.recurse = 1;
    options.instance_forms = 1;
    options.opaque = &rewrite;
    options.complete = appendSpans;
    options.filters = filters;
    return attempt(ctx, [&] {
        pdf_filter_page_contents(ctx, doc, page, &options);
        pdf_obj *resources = pdf_page_resources(ctx, page);
        pdf_obj *fonts = pdf_dict_get(ctx, resources, PDF_NAME(Font));
        if (!fonts)
            fonts = pdf_dict_put_dict(ctx, resources, PDF_NAME(Font), int(spans.size()));
        for (size_t i = 0; i < spans.size(); ++i)
            pdf_dict_puts(ctx, fonts, rewrite.names[i].constData(), spans[i].font);
    });
}
