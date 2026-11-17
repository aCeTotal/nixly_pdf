#include "theme.h"

#include <QApplication>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QAbstractButton>
#include <QLabel>
#include <QPalette>
#include <utility>

namespace theme {

namespace {

constexpr double kShadowBlur = 24;
constexpr double kShadowDrop = 6;
constexpr int kShadowAlpha = 110;
constexpr int kBackdropDarken = 130;
constexpr int kRingAlpha = 70;

// Built-in look without dark palette.
constexpr QColor kAccent(122, 162, 247);
constexpr QColor kText(240, 240, 242);
constexpr QColor kMuted(139, 143, 154);
constexpr QColor kSurface(26, 27, 38);
constexpr QColor kRaised(31, 32, 48);
constexpr QColor kBase(22, 23, 32);
constexpr QColor kSelection(60, 73, 110);

const std::pair<QPalette::ColorRole, QColor> kFallback[] = {
    {QPalette::Window, kSurface},
    {QPalette::Base, kBase},
    {QPalette::AlternateBase, kSurface},
    {QPalette::Text, kText},
    {QPalette::WindowText, kText},
    {QPalette::ButtonText, kText},
    {QPalette::Button, kRaised},
    {QPalette::Highlight, kSelection},
    {QPalette::HighlightedText, kText},
    {QPalette::PlaceholderText, kMuted},
    {QPalette::Link, kAccent},
};

struct Colours
{
    QColor accent;
    QColor text;
    QColor muted;
    QColor surface;
    QColor raised;
    QColor base;
    QColor backdrop;
    QColor selection;
    QColor onSelection;

    bool operator==(const Colours &) const = default;
};

Colours current;

const char *kStyleSheet = R"(
QWidget { color: rgb($text); }
QDialog, QMessageBox, QColorDialog, QFileDialog { background: rgb($surface); }
QToolTip { background: rgb($surface); color: rgb($text); border: 1px solid rgba($accent,110);
           padding: 4px 8px; border-radius: 6px; }
QLabel#caption { color: rgb($accent); font-size: 11px; font-weight: 600; letter-spacing: 1.2px; padding-top: 6px; }
QLabel#title { color: rgb($muted); font-size: 13px; }
QLineEdit, QPlainTextEdit, QDateEdit {
    background: rgba($text,16); border: 1px solid rgba($text,30); border-radius: 10px;
    padding: 7px 12px; selection-background-color: rgba($accent,110);
}
QLineEdit:focus, QPlainTextEdit:focus, QDateEdit:focus { border: 1px solid rgb($accent); }
QDateEdit:disabled { color: rgba($text,80); }
QDateEdit::drop-down { border: none; width: 22px; }
QPushButton {
    background: rgba($text,14); border: 1px solid rgba($text,28); border-radius: 10px;
    padding: 8px 18px;
}
QPushButton:hover { background: rgba($text,26); }
QPushButton:disabled { color: rgba($text,80); }
QPushButton#primary { background: rgba($accent,70); border-color: rgba($accent,170); font-weight: 600; }
QPushButton#primary:hover { background: rgba($accent,110); }
QPushButton#primary:disabled { background: rgba($text,10); border-color: rgba($text,24); }
QPushButton#style { min-height: 46px; padding: 2px 14px; text-align: left; }
QPushButton#style:checked { background: rgba($accent,55); border: 1px solid rgb($accent); }
QPushButton#swatch { border-radius: 15px; border: 2px solid rgba($text,40); padding: 0; }
QPushButton#swatch:checked { border: 3px solid rgb($accent); }
QPushButton#mode { background: transparent; border: none; border-radius: 8px; padding: 6px 18px;
                   color: rgb($muted); font-weight: 600; }
QPushButton#mode:hover { color: rgb($text); }
QPushButton#mode:checked { background: rgba($accent,70); color: rgb($text); }
QPushButton#mode:disabled { color: rgba($text,56); }
QFrame#segment { background: rgba($text,12); border: 1px solid rgba($text,24); border-radius: 11px; }
QFrame#bar { background: rgb($surface); border-bottom: 1px solid rgba($text,16); }
QFrame#editbar { background: rgb($base); border-bottom: 1px solid rgba($text,12); }
QFrame#floatbar { background: rgb($raised); border: 1px solid rgba($text,30); border-radius: 12px; }
QLabel#hint { color: rgb($muted); }
QToolButton#file { background: transparent; border: none; border-radius: 8px; padding: 6px 12px; font-weight: 600; }
QToolButton#file:hover, QToolButton#file:open { background: rgba($text,20); }
QToolButton#file::menu-indicator { image: none; }
QToolButton#tool { background: transparent; border: none; border-radius: 8px; padding: 6px; }
QToolButton#tool:hover { background: rgba($text,20); }
QToolButton#tool:checked { background: rgba($accent,70); }
QFrame#divider { background: rgba($text,24); margin: 0 6px; }
QMenu { background: rgb($raised); border: 1px solid rgba($text,26); border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 14px; border-radius: 6px; }
QMenu::item:selected { background: rgb($selection); color: rgb($onselection); }
QMenu::item:disabled { color: rgba($text,80); }
QMenu::separator { height: 1px; background: rgba($text,20); margin: 5px 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid rgba($text,60);
                       background: rgba($text,10); }
QCheckBox::indicator:checked { background: rgb($accent); border-color: rgb($accent); }
QSlider::groove:horizontal { height: 4px; background: rgba($text,30); border-radius: 2px; }
QSlider::sub-page:horizontal { background: rgb($accent); border-radius: 2px; }
QSlider::handle:horizontal { background: rgb($text); width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }
)";

const char *kSwatch = "QToolButton { background: %1; border-radius: 9px; min-width: 18px;"
                      " min-height: 18px; border: 2px solid %2; }";
const char *kStrike = "qlineargradient(x1:0, y1:1, x2:1, y2:0, stop:0 transparent, stop:0.44 transparent,"
                      " stop:0.46 %1, stop:0.54 %1, stop:0.56 transparent, stop:1 transparent)";

QPalette fallback()
{
    QPalette palette;
    for (const auto &[role, colour] : kFallback)
        palette.setColor(role, colour);
    return palette;
}

bool dark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();
}

Colours coloursOf(const QPalette &palette)
{
    const QColor base = palette.color(QPalette::Base);
    return {
        .accent = palette.color(QPalette::Link),
        .text = palette.color(QPalette::WindowText),
        .muted = palette.color(QPalette::PlaceholderText),
        .surface = palette.color(QPalette::Window),
        .raised = palette.color(QPalette::Button),
        .base = base,
        .backdrop = base.darker(kBackdropDarken),
        .selection = palette.color(QPalette::Highlight),
        .onSelection = palette.color(QPalette::HighlightedText),
    };
}

QString channels(const QColor &colour)
{
    return QStringLiteral("%1,%2,%3").arg(colour.red()).arg(colour.green()).arg(colour.blue());
}

QString swatch(const QString &background)
{
    return QString::fromLatin1(kSwatch).arg(background, faded(current.text, kRingAlpha).name(QColor::HexArgb));
}

QString styleSheet(const Colours &colours)
{
    const std::pair<QString, QColor> tokens[] = {
        {QStringLiteral("$accent"), colours.accent},
        {QStringLiteral("$text"), colours.text},
        {QStringLiteral("$muted"), colours.muted},
        {QStringLiteral("$surface"), colours.surface},
        {QStringLiteral("$raised"), colours.raised},
        {QStringLiteral("$base"), colours.base},
        {QStringLiteral("$selection"), colours.selection},
        {QStringLiteral("$onselection"), colours.onSelection},
    };
    QString sheet = QString::fromLatin1(kStyleSheet);
    for (const auto &[token, colour] : tokens)
        sheet.replace(token, channels(colour));
    return sheet;
}

void follow(QApplication &app)
{
    const QPalette palette = app.palette();
    if (!dark(palette)) {
        // Re-enters through the palette event.
        app.setPalette(fallback());
        return;
    }
    const Colours fresh = coloursOf(palette);
    if (fresh == current)
        return;
    current = fresh;
    app.setStyleSheet(styleSheet(current));
}

// Restyles on every palette change.
class Follower : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::ApplicationPaletteChange && watched == qApp)
            follow(*qApp);
        return false;
    }
};

} // namespace

QColor accent()
{
    return current.accent;
}

QColor text()
{
    return current.text;
}

QColor muted()
{
    return current.muted;
}

QColor surface()
{
    return current.surface;
}

QColor backdrop()
{
    return current.backdrop;
}

void apply(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
    QFont font = app.font();
    font.setPixelSize(13);
    app.setFont(font);
    app.installEventFilter(new Follower(&app));
    follow(app);
}

QLabel *caption(const QString &text)
{
    auto *label = new QLabel(text.toUpper());
    label->setObjectName("caption");
    return label;
}

void paintInk(QAbstractButton *button, const QColor &colour)
{
    button->setStyleSheet(swatch(colour.name()));
}

void paintHollow(QAbstractButton *button)
{
    button->setStyleSheet(swatch(QString::fromLatin1(kStrike).arg(danger.name())));
}

void lift(QWidget *panel)
{
    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(kShadowBlur);
    shadow->setOffset(0, kShadowDrop);
    shadow->setColor(QColor(0, 0, 0, kShadowAlpha));
    panel->setGraphicsEffect(shadow);
}

} // namespace theme
