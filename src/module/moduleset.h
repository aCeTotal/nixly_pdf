#pragma once

#include "typeset.h"

#include <QObject>

// Document modules, cached layouts.
class ModuleSet : public QObject
{
    Q_OBJECT

public:
    explicit ModuleSet(FontLibrary &fonts, QObject *parent = nullptr);

    int add(Module module);
    void update(const Module &module);
    void remove(int id);
    void dropPage(int page);

    const Module *find(int id) const;
    const Layout &layout(int id) const;
    std::vector<int> onPage(int page) const;
    int hit(int page, QPointF point) const;
    bool empty() const { return entries.empty(); }
    FontLibrary &fonts() { return library; }

signals:
    void changed(int page);

private:
    struct Entry
    {
        Module module;
        Layout layout;
    };

    const Entry *entry(int id) const;
    void pushBelow(const Entry &moved, const QRectF &was);

    FontLibrary &library;
    std::vector<Entry> entries;
    int nextId = 1;
};
