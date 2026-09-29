#include "renderer.h"

#include "document.h"

#include <QHashFunctions>
#include <algorithm>
#include <climits>
#include <cmath>

namespace {

constexpr int kThumbLevel = INT_MIN;
constexpr double kScaleUnit = 1024.0;
constexpr size_t kTileQueueLimit = 96;
constexpr size_t kThumbQueueLimit = 512;
constexpr size_t kListLimit = 32;
constexpr qsizetype kTileBudget = qsizetype(384) << 20;
constexpr qsizetype kThumbBudget = qsizetype(192) << 20;

unsigned workerCount()
{
    const unsigned cores = std::thread::hardware_concurrency();
    return std::clamp(cores > 1 ? cores - 1 : 1u, 2u, 6u);
}

// Draws list region into memory.
void draw(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_irect area, unsigned char *samples)
{
    fz_pixmap *pix = fz_new_pixmap_with_bbox_and_data(ctx, fz_device_bgr(ctx), area, nullptr, 1, samples);
    fz_device *dev = nullptr;
    fz_var(dev);
    fz_try(ctx)
    {
        fz_clear_pixmap_with_value(ctx, pix, 0xff);
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        fz_run_display_list(ctx, list, dev, ctm, fz_rect_from_irect(area), nullptr);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx)
    {
        fz_drop_device(ctx, dev);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx)
        fz_rethrow(ctx);
}

fz_display_list *build(fz_context *ctx, fz_document *doc, int index, fz_rect *bounds)
{
    fz_page *page = fz_load_page(ctx, doc, index);
    fz_display_list *list = nullptr;
    fz_try(ctx)
    {
        *bounds = fz_bound_page(ctx, page);
        list = fz_new_display_list_from_page(ctx, page);
    }
    fz_always(ctx)
        fz_drop_page(ctx, page);
    fz_catch(ctx)
        fz_rethrow(ctx);
    return list;
}

} // namespace

size_t qHash(const TileKey &key, size_t seed)
{
    return qHashMulti(seed, key.page, key.level, key.column, key.row);
}

Renderer::Renderer(Document &doc, QObject *parent)
    : QObject(parent), doc(doc), tiles(kTileBudget), thumbs(kThumbBudget)
{
    const unsigned count = workerCount();
    for (unsigned i = 0; i < count; ++i)
        workers.emplace_back([this] { work(); });
}

Renderer::~Renderer()
{
    {
        std::lock_guard hold(queueGuard);
        stopping = true;
    }
    wake.notify_all();
    for (std::thread &worker : workers)
        worker.join();
    dropLists();
}

int Renderer::scaleKey(double scale)
{
    return int(std::lround(scale * kScaleUnit));
}

double Renderer::keyScale(int key)
{
    return key / kScaleUnit;
}

const QImage *Renderer::tile(const TileKey &key)
{
    if (const QImage *hit = tiles.find(key))
        return hit;
    enqueue(tileQueue, {key, epochs[key.page]});
    return nullptr;
}

const QImage *Renderer::thumbnail(int page)
{
    if (const QImage *hit = thumbs.find(page))
        return hit;
    enqueue(thumbQueue, {{page, kThumbLevel, thumbWidth, 0}, epochs[page]});
    return nullptr;
}

void Renderer::setThumbnailWidth(int pixels)
{
    if (pixels == thumbWidth)
        return;
    thumbWidth = pixels;
    thumbs.clear();
}

void Renderer::forget(int page)
{
    ++epochs[page];
    pending.removeIf([page](QHash<TileKey, unsigned>::iterator it) { return it.key().page == page; });
    tiles.removeIf([page](const TileKey &key) { return key.page == page; });
    thumbs.remove(page);
    std::lock_guard hold(doc.mutex());
    const auto stale = std::find_if(lists.begin(), lists.end(), [page](const auto &entry) { return entry.first == page; });
    if (stale == lists.end())
        return;
    fz_drop_display_list(doc.ctx(), stale->second.list);
    lists.erase(stale);
}

void Renderer::enqueue(std::deque<Job> &queue, const Job &job)
{
    if (pending.contains(job.key))
        return;
    pending.insert(job.key, job.epoch);
    const size_t limit = &queue == &tileQueue ? kTileQueueLimit : kThumbQueueLimit;
    {
        std::lock_guard hold(queueGuard);
        queue.push_front(job);
        if (queue.size() > limit) {
            pending.remove(queue.back().key);
            queue.pop_back();
        }
    }
    wake.notify_one();
}

bool Renderer::nextJob(Job *job)
{
    std::unique_lock hold(queueGuard);
    wake.wait(hold, [this] { return stopping || !tileQueue.empty() || !thumbQueue.empty(); });
    if (stopping)
        return false;
    std::deque<Job> &queue = tileQueue.empty() ? thumbQueue : tileQueue;
    *job = queue.front();
    queue.pop_front();
    return true;
}

void Renderer::work()
{
    Job job;
    while (nextJob(&job))
        render(job);
}

Renderer::List Renderer::acquire(int page)
{
    fz_context *ctx = threadContext();
    std::lock_guard hold(doc.mutex());
    const auto hit = std::find_if(lists.begin(), lists.end(), [page](const auto &entry) { return entry.first == page; });
    if (hit != lists.end()) {
        std::rotate(lists.begin(), hit, hit + 1);
        return {fz_keep_display_list(ctx, lists.front().second.list), lists.front().second.x0,
                lists.front().second.y0, lists.front().second.x1, lists.front().second.y1};
    }
    const int index = doc.indexOf(page);
    if (index < 0)
        return {};
    fz_rect bounds{};
    fz_display_list *list = nullptr;
    fz_var(list);
    fz_try(ctx)
        list = build(ctx, &doc.pdf()->super, index, &bounds);
    fz_catch(ctx)
    {
        fz_report_error(ctx);
        return {};
    }
    lists.insert(lists.begin(), {page, {list, bounds.x0, bounds.y0, bounds.x1, bounds.y1}});
    if (lists.size() > kListLimit) {
        fz_drop_display_list(ctx, lists.back().second.list);
        lists.pop_back();
    }
    return {fz_keep_display_list(ctx, list), bounds.x0, bounds.y0, bounds.x1, bounds.y1};
}

void Renderer::render(const Job &job)
{
    const List source = acquire(job.key.page);
    if (!source.list) {
        QMetaObject::invokeMethod(this, [this, job] { deliver(job, QImage()); });
        return;
    }
    fz_context *ctx = threadContext();
    const float width = source.x1 - source.x0;
    const float height = source.y1 - source.y0;
    const bool thumb = job.key.level == kThumbLevel;
    const float scale = thumb ? float(job.key.column) / width : float(keyScale(job.key.level));
    const int fullWidth = int(std::ceil(width * scale));
    const int fullHeight = int(std::ceil(height * scale));
    const int x0 = thumb ? 0 : job.key.column * kTileSize;
    const int y0 = thumb ? 0 : job.key.row * kTileSize;
    const int x1 = thumb ? fullWidth : std::min(fullWidth, x0 + kTileSize);
    const int y1 = thumb ? fullHeight : std::min(fullHeight, y0 + kTileSize);
    QImage image;
    if (x1 > x0 && y1 > y0)
        image = QImage(x1 - x0, y1 - y0, QImage::Format_ARGB32_Premultiplied);
    const fz_matrix ctm = fz_pre_translate(fz_scale(scale, scale), -source.x0, -source.y0);
    fz_try(ctx)
    {
        if (!image.isNull())
            draw(ctx, source.list, ctm, {x0, y0, x1, y1}, image.bits());
    }
    fz_always(ctx)
        fz_drop_display_list(ctx, source.list);
    fz_catch(ctx)
    {
        fz_report_error(ctx);
        image = QImage();
    }
    QMetaObject::invokeMethod(this, [this, job, image = std::move(image)]() mutable { deliver(job, std::move(image)); });
}

void Renderer::deliver(const Job &job, QImage image)
{
    const auto waiting = pending.constFind(job.key);
    if (waiting == pending.constEnd() || *waiting != job.epoch)
        return;
    pending.erase(waiting);
    if (job.epoch != epochs[job.key.page] || image.isNull())
        return;
    if (job.key.level == kThumbLevel) {
        if (job.key.column == thumbWidth)
            thumbs.insert(job.key.page, std::move(image));
    } else {
        tiles.insert(job.key, std::move(image));
    }
    emit updated();
}

void Renderer::dropLists()
{
    for (const auto &entry : lists)
        fz_drop_display_list(doc.ctx(), entry.second.list);
    lists.clear();
}
