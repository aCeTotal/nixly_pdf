#pragma once

#include "pdf/textrun.h"

#include <unordered_map>

// Lazily extracted runs per page.
class RunCache
{
public:
    void setDocument(Document *doc);
    void forget(int id);
    const TextRun *hit(int index, QPointF point);

private:
    Document *doc = nullptr;
    std::unordered_map<int, std::vector<TextRun>> pages;
};
