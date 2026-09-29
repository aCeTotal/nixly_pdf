#pragma once

#include "imagecache.h"

#include <QObject>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class Document;
struct fz_display_list;

struct TileKey
{
    int page;
    int level;
    int column;
    int row;

    bool operator==(const TileKey &) const = default;
};

size_t qHash(const TileKey &key, size_t seed = 0);

class Renderer : public QObject
{
    Q_OBJECT

public:
    static constexpr int kTileSize = 512;

    explicit Renderer(Document &doc, QObject *parent = nullptr);
    ~Renderer() override;

    // Cached image, else queued.
    const QImage *tile(const TileKey &key);
    const QImage *thumbnail(int page);
    const QImage *cachedTile(const TileKey &key) { return tiles.find(key); }

    void setThumbnailWidth(int pixels);
    void forget(int page);

    // Fixed-point render scale.
    static int scaleKey(double scale);
    static double keyScale(int key);

signals:
    void updated();

private:
    struct Job
    {
        TileKey key;
        unsigned epoch;
    };
    struct List
    {
        fz_display_list *list;
        float x0, y0, x1, y1;
    };

    void enqueue(std::deque<Job> &queue, const Job &job);
    bool nextJob(Job *job);
    void work();
    void render(const Job &job);
    List acquire(int page);
    void deliver(const Job &job, QImage image);
    void dropLists();

    Document &doc;
    ImageCache<TileKey> tiles;
    ImageCache<int> thumbs;
    QHash<TileKey, unsigned> pending;
    std::unordered_map<int, unsigned> epochs;
    int thumbWidth = 240;

    std::mutex queueGuard;
    std::condition_variable wake;
    std::deque<Job> tileQueue;
    std::deque<Job> thumbQueue;
    bool stopping = false;

    std::vector<std::pair<int, List>> lists;
    std::vector<std::thread> workers;
};
