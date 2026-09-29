#pragma once

#include <QHash>
#include <QImage>
#include <list>
#include <utility>

// LRU images within byte budget.
template <typename Key>
class ImageCache
{
public:
    explicit ImageCache(qsizetype budget) : budget(budget) {}

    const QImage *find(const Key &key)
    {
        const auto hit = index.constFind(key);
        if (hit == index.constEnd())
            return nullptr;
        order.splice(order.begin(), order, *hit);
        return &order.front().second;
    }

    void insert(const Key &key, QImage image)
    {
        remove(key);
        bytes += image.sizeInBytes();
        order.emplace_front(key, std::move(image));
        index.insert(key, order.begin());
        while (bytes > budget && order.size() > 1)
            remove(order.back().first);
    }

    void remove(const Key &key)
    {
        const auto hit = index.constFind(key);
        if (hit == index.constEnd())
            return;
        bytes -= (*hit)->second.sizeInBytes();
        order.erase(*hit);
        index.erase(hit);
    }

    template <typename Predicate>
    void removeIf(Predicate predicate)
    {
        for (auto it = order.begin(); it != order.end();) {
            if (!predicate(it->first)) {
                ++it;
                continue;
            }
            bytes -= it->second.sizeInBytes();
            index.remove(it->first);
            it = order.erase(it);
        }
    }

    void clear()
    {
        order.clear();
        index.clear();
        bytes = 0;
    }

private:
    using Entry = std::pair<Key, QImage>;
    std::list<Entry> order;
    QHash<Key, typename std::list<Entry>::iterator> index;
    qsizetype bytes = 0;
    qsizetype budget;
};
