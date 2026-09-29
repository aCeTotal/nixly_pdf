#include "fontname.h"

#include <QRegularExpression>
#include <cctype>

namespace {

constexpr int kTagLength = 6;

const QRegularExpression kWeight("(Bold|Halvfet|Fet|Black|Heavy|Semi ?bold|Demi)", QRegularExpression::CaseInsensitiveOption);

} // namespace

const char *baseFontName(const char *name)
{
    for (int i = 0; i < kTagLength; ++i) {
        if (!std::isupper(static_cast<unsigned char>(name[i])))
            return name;
    }
    return name[kTagLength] == '+' ? name + kTagLength + 1 : name;
}

QString familyOf(const QString &name)
{
    static const QRegularExpression style("[-,].*$");
    static const QRegularExpression vendor("(PSMT|PS|MT)$");
    static const QRegularExpression camel("(?<=[a-z])(?=[A-Z])");
    static const QRegularExpression words(
        "\\s+(Bold|Halvfet|Fet|Normal|Regular|Italic|Kursiv|Oblique|Light|Medium|Semi ?bold|Black|Book|Demi|Heavy)\\b.*$",
        QRegularExpression::CaseInsensitiveOption);
    QString family = QString::fromUtf8(baseFontName(name.toUtf8().constData()));
    family.remove(style).remove(vendor).replace(camel, " ");
    return family.remove(words).trimmed();
}

bool namesBold(const QString &name)
{
    return kWeight.match(QString::fromUtf8(baseFontName(name.toUtf8().constData()))).hasMatch();
}
