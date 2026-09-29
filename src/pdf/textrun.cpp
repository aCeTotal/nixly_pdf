#include "textrun.h"

#include "document.h"
#include "fontname.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float kSizeTolerance = 0.05f;
constexpr int kVisible = FZ_STEXT_FILLED | FZ_STEXT_STROKED;

bool sameStyle(fz_context *ctx, const fz_stext_char *a, const fz_stext_char *b)
{
    const bool sameFace = a->font == b->font ||
                          std::strcmp(baseFontName(fz_font_name(ctx, a->font)), baseFontName(fz_font_name(ctx, b->font))) == 0;
    return sameFace && a->argb == b->argb && std::fabs(a->size - b->size) < kSizeTolerance;
}

LiveRun startRun(fz_context *ctx, const fz_stext_line *line, const fz_stext_char *ch)
{
    TextRun run{{},
                {},
                {line->dir.x, line->dir.y},
                ch->size,
                QRgb(ch->argb),
                QString::fromUtf8(fz_font_name(ctx, ch->font)),
                bool(fz_font_is_bold(ctx, ch->font)),
                bool(fz_font_is_italic(ctx, ch->font)),
                bool(fz_font_is_serif(ctx, ch->font)),
                {}};
    return {std::move(run), ch->font};
}

void addChar(TextRun &run, const fz_stext_char *ch)
{
    const fz_rect r = fz_rect_from_quad(ch->quad);
    run.box |= QRectF(QPointF(r.x0, r.y0), QPointF(r.x1, r.y1));
    run.text += QString::fromUcs4(reinterpret_cast<const char32_t *>(&ch->c), 1);
    if (!(ch->flags & FZ_STEXT_SYNTHETIC))
        run.glyphs.emplace_back(ch->origin.x, ch->origin.y);
}

void collectLine(fz_context *ctx, const fz_stext_line *line, std::vector<LiveRun> &runs)
{
    const fz_stext_char *styled = nullptr;
    for (const fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
        if (!(ch->flags & kVisible) && !(ch->flags & FZ_STEXT_SYNTHETIC))
            continue;
        if (!styled || !sameStyle(ctx, styled, ch)) {
            runs.push_back(startRun(ctx, line, ch));
            styled = ch;
        }
        addChar(runs.back().run, ch);
    }
}

} // namespace

fz_stext_page *extractText(fz_context *ctx, fz_page *page)
{
    const fz_stext_options options{};
    return fz_new_stext_page_from_page(ctx, page, &options);
}

std::vector<LiveRun> collectRuns(fz_context *ctx, fz_stext_page *text)
{
    std::vector<LiveRun> runs;
    for (fz_stext_block *block = text->first_block; block; block = block->next) {
        if (block->type != FZ_STEXT_BLOCK_TEXT)
            continue;
        for (fz_stext_line *line = block->u.t.first_line; line; line = line->next)
            collectLine(ctx, line, runs);
    }
    std::erase_if(runs, [](const LiveRun &live) { return live.run.glyphs.empty(); });
    return runs;
}

std::vector<TextRun> pageRuns(Document &doc, int index)
{
    std::vector<TextRun> runs;
    std::lock_guard hold(doc.mutex());
    fz_context *ctx = doc.ctx();
    fz_page *page = nullptr;
    fz_stext_page *text = nullptr;
    fz_var(page);
    fz_var(text);
    fz_try(ctx)
    {
        page = fz_load_page(ctx, &doc.pdf()->super, index);
        text = extractText(ctx, page);
    }
    fz_catch(ctx)
    {
        fz_report_error(ctx);
        fz_drop_page(ctx, page);
        return runs;
    }
    for (LiveRun &live : collectRuns(ctx, text))
        runs.push_back(std::move(live.run));
    fz_drop_stext_page(ctx, text);
    fz_drop_page(ctx, page);
    return runs;
}
