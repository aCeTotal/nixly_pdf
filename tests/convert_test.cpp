#include "fixture.h"

#include "convert/converter.h"
#include "convert/formats.h"
#include "convert/redraw.h"

#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QTimer>

namespace {

constexpr int kOfficeWaitMs = 240000;

// Quantities, prices and formulas.
constexpr const char *kSheet = "Produkt;Antall;Pris;Sum\n"
                               "Parkering;3;240;=B2*C2\n"
                               "Gebyr;1;10;=B3*C3\n"
                               "Fleks;1;40;=B4*C4\n"
                               "Totalt;;;=SUM(D2:D4)\n";

void checkFormats()
{
    const bool known = sourceOf(QStringLiteral("a.XLSX")) == Source::Office && sourceOf("b.Pdf") == Source::Pdf;
    check(known && sourceOf("c.png") == Source::Drawn && sourceOf("d.heic") == Source::Unknown, "sorts files by type");
}

void checkPicture(const QTemporaryDir &dir)
{
    const QString png = dir.filePath("red.png");
    QImage red(200, 100, QImage::Format_RGB32);
    red.fill(Qt::red);
    red.save(png);
    const QString pdf = dir.filePath("red.pdf");
    check(redrawAsPdf(png, pdf).isEmpty(), "turns an image into a PDF");
    QString error;
    std::unique_ptr<Document> doc = Document::open(pdf, &error);
    const std::vector<Picture> pictures = doc ? pageText(*doc, 0).pictures : std::vector<Picture>();
    check(pictures.size() == 1, "finds the picture on the page");
    const Taken taken = pictures.empty() ? Taken() : takePicture(*doc, 0, pictures.front());
    const bool pixels = !taken.image.isNull() && taken.image.pixelColor(10, 10) == QColor(Qt::red);
    check(taken.error.isEmpty() && pixels, "lifts the picture with its pixels");
    check(doc && pageText(*doc, 0).pictures.empty(), "lifted picture leaves the page");
}

void checkOffice(const QTemporaryDir &dir)
{
    const QString csv = dir.filePath("sheet.csv");
    QFile file(csv);
    const bool written = file.open(QIODevice::WriteOnly) && file.write(kSheet) > 0;
    file.close();
    check(written, "writes a sample spreadsheet");
    Converter converter;
    QStringList made;
    QEventLoop wait;
    QObject::connect(&converter, &Converter::finished, &wait,
                     [&](const QStringList &, const QStringList &pdfs, const QString &) {
                         made = pdfs;
                         wait.quit();
                     });
    QTimer::singleShot(kOfficeWaitMs, &wait, &QEventLoop::quit);
    converter.start({csv});
    wait.exec();
    QString error;
    std::unique_ptr<Document> doc = made.isEmpty() ? nullptr : Document::open(made.front(), &error);
    check(doc && pageHas(*doc, 0, QStringLiteral("Totalt")), "converts a spreadsheet");
    check(doc && pageHas(*doc, 0, QStringLiteral("770")), "keeps spreadsheet formulas computed");
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    checkFormats();
    checkPicture(dir);
    checkOffice(dir);
    return verdict();
}
