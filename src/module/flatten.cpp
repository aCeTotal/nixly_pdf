#include "flatten.h"

#include "modulepaint.h"
#include "pdf/document.h"

#include <map>

namespace {

struct Ink
{
    fz_context *ctx;
    pdf_document *copy;
    ModuleSet &modules;
    fz_matrix toUser = fz_identity;
    PageEdit edit;
    std::map<QByteArray, pdf_obj *> embedded;
    std::vector<pdf_obj *> held;
};

fz_matrix toFz(const QTransform &t)
{
    return {float(t.m11()), float(t.m12()), float(t.m21()), float(t.m22()), float(t.dx()), float(t.dy())};
}

fz_quad quadOf(const QRectF &r, fz_matrix toUser)
{
    const fz_rect rect{float(r.left()), float(r.top()), float(r.right()), float(r.bottom())};
    return fz_transform_quad(fz_quad_from_rect(rect), toUser);
}

pdf_obj *fontFor(Ink &ink, const Face &face, QString *failure)
{
    if (face.object) {
        pdf_obj *ref = pdf_new_indirect(ink.ctx, ink.copy, face.object, 0);
        ink.held.push_back(ref);
        return ref;
    }
    const auto hit = ink.embedded.find(face.path);
    if (hit != ink.embedded.end())
        return hit->second;
    pdf_obj *added = nullptr;
    *failure = attempt(ink.ctx, [&] {
        fz_font *font = fz_new_font_from_file(ink.ctx, nullptr, face.path.constData(), 0, 0);
        fz_try(ink.ctx)
            added = pdf_add_cid_font(ink.ctx, ink.copy, font);
        fz_always(ink.ctx)
            fz_drop_font(ink.ctx, font);
        fz_catch(ink.ctx)
            fz_rethrow(ink.ctx);
    });
    ink.embedded.emplace(face.path, added);
    return added;
}

QString addModule(Ink &ink, const Module &module, const Layout &layout)
{
    if (!module.cover.isEmpty())
        ink.edit.covers.push_back({quadOf(module.cover, ink.toUser), module.coverColor.rgb()});
    const QTransform place = moduleTransform(module);
    for (const GlyphRun &run : layout.runs) {
        QString failure;
        pdf_obj *font = fontFor(ink, *run.face, &failure);
        if (!failure.isEmpty())
            return failure;
        const QTransform glyph(run.size, 0, 0, -run.size, run.origin.x(), run.origin.y());
        const fz_matrix matrix = fz_concat(toFz(glyph * place), ink.toUser);
        ink.edit.spans.push_back(
            {font, run.face->object ? run.face->codeBytes : 2, matrix, inkOf(module, run.style).rgb(), run.items});
    }
    return {};
}

QString flattenPage(Ink &ink, int index, const std::vector<int> &ids)
{
    pdf_page *page = nullptr;
    fz_matrix ctm = fz_identity;
    QString failure = attempt(ink.ctx, [&] {
        page = pdf_load_page(ink.ctx, ink.copy, index);
        pdf_page_transform(ink.ctx, page, nullptr, &ctm);
    });
    ink.toUser = fz_invert_matrix(ctm);
    ink.edit = {};
    for (size_t i = 0; failure.isEmpty() && i < ids.size(); ++i)
        failure = addModule(ink, *ink.modules.find(ids[i]), ink.modules.layout(ids[i]));
    if (failure.isEmpty())
        failure = rewritePage(ink.ctx, page, ink.edit);
    pdf_drop_page(ink.ctx, page);
    return failure;
}

} // namespace

QString flatten(fz_context *ctx, pdf_document *copy, const Flattening &source)
{
    Ink ink{ctx, copy, source.modules, fz_identity, {}, {}, {}};
    QString failure;
    for (int index = 0; failure.isEmpty() && index < source.doc.count(); ++index) {
        const std::vector<int> ids = source.modules.onPage(source.doc.slot(index).id);
        if (!ids.empty())
            failure = flattenPage(ink, index, ids);
    }
    for (pdf_obj *ref : ink.held)
        pdf_drop_obj(ctx, ref);
    for (const auto &entry : ink.embedded)
        pdf_drop_obj(ctx, entry.second);
    return failure;
}
