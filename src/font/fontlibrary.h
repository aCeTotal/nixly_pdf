#pragma once

#include "pdf/context.h"
#include "systemfont.h"

#include <QHash>
#include <QRawFont>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

// Advance in em thousandths.
struct Glyph
{
    quint32 gid;
    int code;
    float advance;
};

// Face for painting and writing.
struct Face
{
    QRawFont raw;
    QString family;
    QString name;
    int object = 0;
    QByteArray path;
    int codeBytes = 2;
    bool bold = false;
    bool italic = false;
    bool serif = false;
    QHash<char32_t, Glyph> glyphs;
};

class FontLibrary
{
public:
    FontLibrary() = default;
    FontLibrary(const FontLibrary &) = delete;
    FontLibrary &operator=(const FontLibrary &) = delete;

    // Loads font and sibling subsets.
    int adopt(fz_context *ctx, pdf_page *page, fz_font *font);
    std::vector<Face *> pageFaces(int object);
    Face *fileFace(const FontQuery &query);
    Face *fileAt(const QByteArray &path);
    std::optional<Glyph> glyph(Face &face, char32_t c);

private:
    std::unordered_map<int, std::unique_ptr<Face>> pages;
    std::unordered_map<int, std::vector<int>> siblings;
    std::map<QByteArray, std::unique_ptr<Face>> files;
};
