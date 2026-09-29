#pragma once

#include "context.h"

#include <QSizeF>
#include <QString>
#include <memory>
#include <mutex>
#include <vector>

struct PageSlot
{
    int id;
    QSizeF size;
};

class Document
{
public:
    static std::unique_ptr<Document> open(const QString &path, QString *error);
    ~Document();

    bool locked() const;
    bool unlock(const QString &password);
    bool save(const QString &path, QString *error);
    void markFontEmbedded();

    // Locked edit, then page refresh.
    template <typename Change>
    bool modify(QString *error, Change change)
    {
        std::lock_guard hold(guard);
        *error = change(ctx(), handle);
        if (!error->isEmpty())
            return false;
        reload();
        return true;
    }

    int count() const { return int(pages.size()); }
    const PageSlot &slot(int index) const { return pages[size_t(index)]; }
    int indexOf(int id) const;

    fz_context *ctx() const { return mainContext(); }
    pdf_document *pdf() const { return handle; }
    std::mutex &mutex() { return guard; }
    const QString &path() const { return filePath; }

private:
    Document(pdf_document *handle, const QString &path);
    void reload();

    pdf_document *handle;
    QString filePath;
    std::vector<PageSlot> pages;
    std::mutex guard;
    bool encrypted;
    bool fontsEmbedded = false;
};
