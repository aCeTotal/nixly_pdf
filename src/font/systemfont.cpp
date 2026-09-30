#include "systemfont.h"

#include <fontconfig/fontconfig.h>
#include <mutex>

FontFile matchFont(const FontQuery &query)
{
    static std::once_flag bundled;
    std::call_once(bundled, [] {
        FcConfigAppFontAddDir(nullptr, reinterpret_cast<const FcChar8 *>(FALLBACK_FONT_DIR));
        FcConfigAppFontAddDir(nullptr, reinterpret_cast<const FcChar8 *>(SIGNATURE_FONT_DIR));
    });
    const QByteArray family = query.family.toUtf8();
    FcPattern *pattern = FcPatternCreate();
    const QByteArray generic = query.generic.toUtf8();
    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(family.constData()));
    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(generic.constData()));
    FcPatternAddInteger(pattern, FC_WEIGHT, query.bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
    FcPatternAddInteger(pattern, FC_SLANT, query.italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcCharSet *chars = FcCharSetCreate();
    for (char32_t c : query.text.toStdU32String())
        FcCharSetAddChar(chars, c);
    FcPatternAddCharSet(pattern, FC_CHARSET, chars);
    FcCharSetDestroy(chars);
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult outcome;
    FcPattern *match = FcFontMatch(nullptr, pattern, &outcome);
    FcPatternDestroy(pattern);

    FontFile found{{}, 0, {}};
    if (!match)
        return found;
    FcChar8 *file = nullptr;
    FcChar8 *matched = nullptr;
    FcPatternGetString(match, FC_FILE, 0, &file);
    FcPatternGetString(match, FC_FAMILY, 0, &matched);
    FcPatternGetInteger(match, FC_INDEX, 0, &found.index);
    found.path = QByteArray(reinterpret_cast<const char *>(file));
    found.family = QString::fromUtf8(reinterpret_cast<const char *>(matched));
    FcPatternDestroy(match);
    return found;
}
