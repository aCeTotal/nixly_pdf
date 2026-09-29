#include "app/theme.h"
#include "app/window.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("nixly-pdf");
    app.setApplicationDisplayName("Nixly PDF");
    app.setApplicationVersion("0.1");
    app.setDesktopFileName("nixly-pdf");
    theme::apply(app);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "PDF document to open.");
    parser.process(app);

    Window window(parser.positionalArguments().value(0));
    window.show();
    return app.exec();
}
