#include "formats.h"

#include <QFileInfo>
#include <QStringList>
#include <algorithm>

namespace {

struct Format
{
    const char *suffix;
    Source source;
};

constexpr Format kFormats[] = {
    {"pdf", Source::Pdf},       {"png", Source::Drawn},     {"jpg", Source::Drawn},     {"jpeg", Source::Drawn},
    {"jpe", Source::Drawn},     {"jfif", Source::Drawn},    {"gif", Source::Drawn},     {"bmp", Source::Drawn},
    {"tif", Source::Drawn},     {"tiff", Source::Drawn},    {"pnm", Source::Drawn},     {"pbm", Source::Drawn},
    {"pgm", Source::Drawn},     {"ppm", Source::Drawn},     {"pam", Source::Drawn},     {"jp2", Source::Drawn},
    {"jpx", Source::Drawn},     {"j2k", Source::Drawn},     {"psd", Source::Drawn},     {"svg", Source::Drawn},
    {"xps", Source::Drawn},     {"oxps", Source::Drawn},    {"epub", Source::Drawn},    {"cbz", Source::Drawn},
    {"fb2", Source::Drawn},     {"mobi", Source::Drawn},    {"txt", Source::Drawn},     {"html", Source::Drawn},
    {"htm", Source::Drawn},     {"xhtml", Source::Drawn},   {"doc", Source::Office},    {"docx", Source::Office},
    {"docm", Source::Office},   {"dot", Source::Office},    {"dotx", Source::Office},   {"odt", Source::Office},
    {"ott", Source::Office},    {"rtf", Source::Office},    {"wpd", Source::Office},    {"pages", Source::Office},
    {"xls", Source::Office},    {"xlsx", Source::Office},   {"xlsm", Source::Office},   {"xlsb", Source::Office},
    {"ods", Source::Office},    {"ots", Source::Office},    {"csv", Source::Office},    {"tsv", Source::Office},
    {"numbers", Source::Office}, {"ppt", Source::Office},   {"pptx", Source::Office},   {"pps", Source::Office},
    {"ppsx", Source::Office},   {"odp", Source::Office},    {"otp", Source::Office},    {"key", Source::Office},
    {"odg", Source::Office},    {"vsd", Source::Office},    {"vsdx", Source::Office},   {"pub", Source::Office},
    {"emf", Source::Office},    {"wmf", Source::Office},    {"webp", Source::Office},   {"md", Source::Office},
};

} // namespace

Source sourceOf(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    const auto hit = std::ranges::find_if(kFormats, [&suffix](const Format &f) { return suffix == f.suffix; });
    return hit == std::end(kFormats) ? Source::Unknown : hit->source;
}

QString openFilter()
{
    QStringList patterns;
    for (const Format &format : kFormats)
        patterns << QStringLiteral("*.") + QString::fromLatin1(format.suffix);
    return QStringLiteral("Documents and images (%1);;PDF documents (*.pdf)").arg(patterns.join(' '));
}
