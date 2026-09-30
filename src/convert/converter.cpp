#include "converter.h"

#include "formats.h"
#include "redraw.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace {

constexpr const char *kOffice = "soffice";
constexpr const char *kCsvFilter = "--infilter=CSV:44/59/9,34,76,1";
constexpr int kOfficeTimeoutMs = 180000;

QString officeProfile()
{
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return QUrl::fromLocalFile(QDir(cache).filePath(QStringLiteral("libreoffice"))).toString();
}

} // namespace

Converter::Converter(QObject *parent) : QObject(parent)
{
    patience.setSingleShot(true);
    patience.setInterval(kOfficeTimeoutMs);
    connect(&patience, &QTimer::timeout, &office, &QProcess::kill);
    connect(&office, &QProcess::finished, this, [this] { officeDone(); });
    connect(&office, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(tr("LibreOffice is needed for this file type"));
    });
}

Converter::~Converter()
{
    office.kill();
    office.waitForFinished();
    if (drawing.valid())
        drawing.wait();
}

void Converter::start(const QStringList &files)
{
    running = true;
    sources = files;
    waiting = files;
    made.clear();
    scratch = std::make_unique<QTemporaryDir>();
    next();
}

void Converter::next()
{
    if (waiting.isEmpty()) {
        finish({});
        return;
    }
    const QString file = waiting.takeFirst();
    const QString folder = scratch->filePath(QString::number(made.size()));
    QDir().mkpath(folder);
    const QString target = QDir(folder).filePath(QFileInfo(file).completeBaseName() + QStringLiteral(".pdf"));
    switch (sourceOf(file)) {
    case Source::Pdf:
        made << file;
        next();
        return;
    case Source::Drawn:
        startDrawn(file, target);
        return;
    case Source::Office:
        expected = target;
        startOffice(file, folder);
        return;
    case Source::Unknown:
        finish(tr("%1 is not a file type Nixly PDF can open").arg(QFileInfo(file).fileName()));
        return;
    }
}

void Converter::startDrawn(const QString &file, const QString &target)
{
    drawing = std::async(std::launch::async, [this, file, target] {
        const QString failure = redrawAsPdf(file, target);
        QMetaObject::invokeMethod(this, [this, failure, target] {
            drawing.wait();
            if (!failure.isEmpty()) {
                finish(failure);
                return;
            }
            made << target;
            next();
        });
    });
}

void Converter::startOffice(const QString &file, const QString &folder)
{
    QStringList arguments{QStringLiteral("--headless"),   QStringLiteral("--norestore"),
                          QStringLiteral("--nolockcheck"), QStringLiteral("-env:UserInstallation=") + officeProfile(),
                          QStringLiteral("--convert-to"),  QStringLiteral("pdf"),
                          QStringLiteral("--outdir"),      folder};
    if (QFileInfo(file).suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0)
        arguments << QString::fromLatin1(kCsvFilter);
    arguments << file;
    patience.start();
    office.start(QString::fromLatin1(kOffice), arguments);
}

void Converter::officeDone()
{
    patience.stop();
    if (!QFileInfo::exists(expected)) {
        finish(tr("LibreOffice could not convert %1").arg(QFileInfo(expected).completeBaseName()));
        return;
    }
    made << expected;
    next();
}

void Converter::finish(const QString &error)
{
    patience.stop();
    running = false;
    waiting.clear();
    emit finished(sources, error.isEmpty() ? made : QStringList(), error);
}
