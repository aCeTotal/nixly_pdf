#pragma once

#include "passage.h"

#include <optional>
#include <unordered_map>

// Lazily arranged text per page.
class RunCache
{
public:
    void setDocument(Document *doc);
    void forget(int id);

    // Paragraph or panel under point.
    Passage hit(int index, QPointF point);
    // Topmost picture under point.
    std::optional<Picture> pictureAt(int index, QPointF point);

private:
    const TextPage &page(int index);

    Document *doc = nullptr;
    std::unordered_map<int, TextPage> pages;
};
