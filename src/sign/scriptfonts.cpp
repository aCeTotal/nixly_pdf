#include "scriptfonts.h"

#include "font/systemfont.h"

#include <QFile>
#include <QFontDatabase>

const std::vector<ScriptFont> &scriptFonts()
{
    static const std::vector<ScriptFont> fonts = [] {
        const QString dir = QStringLiteral(SIGNATURE_FONT_DIR "/");
        const char *files[] = {
            "MrsSaintDelafield-Regular.ttf",
            "MrDeHaviland-Regular.ttf",
            "Cedarville-Cursive.ttf",
            "LaBelleAurore.ttf",
            "DawningofaNewDay.ttf",
            "Zeyada.ttf",
            "NothingYouCouldDo.ttf",
            "Kristi-Regular.ttf",
            "GreatVibes-Regular.ttf",
            "AlexBrush-Regular.ttf",
            "Allura-Regular.ttf",
            "Sacramento-Regular.ttf",
            "Parisienne-Regular.ttf",
            "PinyonScript-Regular.ttf",
            "MrDafoe-Regular.ttf",
            "HerrVonMuellerhoff-Regular.ttf",
            "MonsieurLaDoulaise-Regular.ttf",
            "HomemadeApple-Regular.ttf",
        };
        std::vector<ScriptFont> found;
        for (const char *file : files) {
            const int id = QFontDatabase::addApplicationFont(dir + file);
            if (id >= 0)
                found.push_back({dir + file, QFontDatabase::applicationFontFamilies(id).value(0)});
        }
        return found;
    }();
    return fonts;
}

const QString &labelFont()
{
    const QString sans = QStringLiteral("sans-serif");
    static const QString path = QFile::decodeName(matchFont({sans, sans, false, false, {}}).path);
    return path;
}
