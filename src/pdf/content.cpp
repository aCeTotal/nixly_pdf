#include "content.h"

#include <QByteArray>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float kOriginTolerance = 0.35f;
constexpr const char *kInline = "<inline>";

struct Rewrite
{
    const PageEdit *edit;
    std::vector<QByteArray> names;
};

int dropGlyph(fz_context *, void *opaque, int *, int, fz_matrix trm, fz_matrix ctm, fz_rect, int, float, float)
{
    const auto *rewrite = static_cast<const Rewrite *>(opaque);
    const fz_matrix m = fz_concat(trm, ctm);
    for (const fz_point &p : rewrite->edit->removed) {
        if (std::fabs(p.x - m.e) < kOriginTolerance && std::fabs(p.y - m.f) < kOriginTolerance)
            return 1;
    }
    return 0;
}

// Drops lifted images only.
fz_image *dropImage(fz_context *ctx, void *opaque, fz_matrix ctm, const char *name, fz_image *image, fz_rect)
{
    const auto *rewrite = static_cast<const Rewrite *>(opaque);
    const auto lifted = [ctm](fz_matrix m) { return samePlacement(m, ctm); };
    if (std::ranges::any_of(rewrite->edit->images, lifted))
        return nullptr;
    return std::strcmp(name, kInline) == 0 ? fz_keep_image(ctx, image) : image;
}

void appendColor(fz_context *ctx, fz_buffer *out, QRgb color)
{
    fz_append_printf(ctx, out, "%g %g %g rg ", qRed(color) / 255.0f, qGreen(color) / 255.0f, qBlue(color) / 255.0f);
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

void appendInk(fz_context *ctx, fz_buffer *out, void *opaque)
{
    const auto *rewrite = static_cast<const Rewrite *>(opaque);
    for (const Cover &cover : rewrite->edit->covers) {
        const fz_quad &q = cover.quad;
        fz_append_string(ctx, out, "\nq ");
        appendColor(ctx, out, cover.color);
        fz_append_printf(ctx, out, "%g %g m %g %g l %g %g l %g %g l f Q\n", q.ul.x, q.ul.y, q.ur.x, q.ur.y, q.lr.x,
                         q.lr.y, q.ll.x, q.ll.y);
    }
    const std::vector<TextSpan> &spans = rewrite->edit->spans;
    for (size_t i = 0; i < spans.size(); ++i) {
        const fz_matrix &m = spans[i].matrix;
        fz_append_string(ctx, out, "\nq ");
        appendColor(ctx, out, spans[i].color);
        fz_append_printf(ctx, out, "BT /%s 1 Tf %g %g %g %g %g %g Tm ", rewrite->names[i].constData(), m.a, m.b, m.c,
                         m.d, m.e, m.f);
        appendItems(ctx, out, spans[i]);
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

QString rewritePage(fz_context *ctx, pdf_page *page, const PageEdit &edit)
{
    Rewrite rewrite{&edit, {}};
    pdf_obj *oldFonts = pdf_dict_get(ctx, pdf_page_resources(ctx, page), PDF_NAME(Font));
    int serial = 0;
    for (size_t i = 0; i < edit.spans.size(); ++i)
        rewrite.names.push_back(freeName(ctx, oldFonts, &serial));

    pdf_sanitize_filter_options sanitize{};
    sanitize.opaque = &rewrite;
    sanitize.text_filter = edit.removed.empty() ? nullptr : dropGlyph;
    sanitize.image_filter = edit.images.empty() ? nullptr : dropImage;
    pdf_filter_factory filters[] = {{pdf_new_sanitize_filter, &sanitize}, {nullptr, nullptr}};
    pdf_filter_options options{};
    options.recurse = 1;
    options.instance_forms = 1;
    options.opaque = &rewrite;
    options.complete = appendInk;
    options.filters = filters;
    return attempt(ctx, [&] {
        pdf_filter_page_contents(ctx, page->doc, page, &options);
        pdf_obj *resources = pdf_page_resources(ctx, page);
        pdf_obj *fonts = pdf_dict_get(ctx, resources, PDF_NAME(Font));
        if (!fonts)
            fonts = pdf_dict_put_dict(ctx, resources, PDF_NAME(Font), int(edit.spans.size()));
        for (size_t i = 0; i < edit.spans.size(); ++i)
            pdf_dict_puts(ctx, fonts, rewrite.names[i].constData(), edit.spans[i].font);
    });
}

bool samePlacement(fz_matrix a, fz_matrix b)
{
    const float gaps[] = {a.a - b.a, a.b - b.b, a.c - b.c, a.d - b.d, a.e - b.e, a.f - b.f};
    return std::ranges::all_of(gaps, [](float gap) { return std::fabs(gap) < kOriginTolerance; });
}
