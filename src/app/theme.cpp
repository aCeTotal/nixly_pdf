#include "theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>

namespace theme {

namespace {

const char *kStyleSheet = R"(
QWidget { color: #f0f0f2; }
QDialog, QMessageBox, QColorDialog, QFileDialog { background: #1a1b26; }
QToolTip { background: #1a1b26; color: #f0f0f2; border: 1px solid rgba(122,162,247,110); padding: 4px 8px; border-radius: 6px; }
QLabel#caption { color: #7aa2f7; font-size: 11px; font-weight: 600; letter-spacing: 1.2px; padding-top: 6px; }
QLabel#title { color: #8b8f9a; font-size: 13px; }
QLineEdit, QPlainTextEdit, QDateEdit {
    background: rgba(255,255,255,16); border: 1px solid rgba(255,255,255,30); border-radius: 10px;
    padding: 7px 12px; selection-background-color: rgba(122,162,247,110);
}
QLineEdit:focus, QPlainTextEdit:focus, QDateEdit:focus { border: 1px solid #7aa2f7; }
QDateEdit:disabled { color: #5c6070; }
QDateEdit::drop-down { border: none; width: 22px; }
QPushButton {
    background: rgba(255,255,255,14); border: 1px solid rgba(255,255,255,28); border-radius: 10px;
    padding: 8px 18px;
}
QPushButton:hover { background: rgba(255,255,255,26); }
QPushButton:disabled { color: #5c6070; }
QPushButton#primary { background: rgba(122,162,247,70); border-color: rgba(122,162,247,170); font-weight: 600; }
QPushButton#primary:hover { background: rgba(122,162,247,110); }
QPushButton#primary:disabled { background: rgba(255,255,255,10); border-color: rgba(255,255,255,24); }
QPushButton#style { min-height: 46px; padding: 2px 14px; text-align: left; }
QPushButton#style:checked { background: rgba(122,162,247,55); border: 1px solid #7aa2f7; }
QPushButton#swatch { border-radius: 15px; border: 2px solid rgba(255,255,255,40); padding: 0; }
QPushButton#swatch:checked { border: 3px solid #7aa2f7; }
QPushButton#mode { background: transparent; border: none; border-radius: 8px; padding: 6px 18px; color: #8b8f9a; font-weight: 600; }
QPushButton#mode:hover { color: #f0f0f2; }
QPushButton#mode:checked { background: rgba(122,162,247,70); color: #f0f0f2; }
QPushButton#mode:disabled { color: #4a4d5a; }
QFrame#segment { background: rgba(255,255,255,12); border: 1px solid rgba(255,255,255,24); border-radius: 11px; }
QFrame#bar { background: #1a1b26; border-bottom: 1px solid rgba(255,255,255,16); }
QFrame#editbar { background: #16161f; border-bottom: 1px solid rgba(255,255,255,12); }
QLabel#hint { color: #8b8f9a; }
QToolButton#file { background: transparent; border: none; border-radius: 8px; padding: 6px 12px; font-weight: 600; }
QToolButton#file:hover, QToolButton#file:open { background: rgba(255,255,255,20); }
QToolButton#file::menu-indicator { image: none; }
QMenu { background: #1f2030; border: 1px solid rgba(255,255,255,26); border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 14px; border-radius: 6px; }
QMenu::item:selected { background: rgba(122,162,247,80); }
QMenu::item:disabled { color: #5c6070; }
QMenu::separator { height: 1px; background: rgba(255,255,255,20); margin: 5px 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid rgba(255,255,255,60); background: rgba(255,255,255,10); }
QCheckBox::indicator:checked { background: #7aa2f7; border-color: #7aa2f7; }
QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,30); border-radius: 2px; }
QSlider::sub-page:horizontal { background: #7aa2f7; border-radius: 2px; }
QSlider::handle:horizontal { background: #f0f0f2; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }
)";

} // namespace

void apply(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, surface);
    palette.setColor(QPalette::Base, QColor(22, 23, 32));
    palette.setColor(QPalette::AlternateBase, surface);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Button, surface);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, backdrop);
    palette.setColor(QPalette::PlaceholderText, muted);
    app.setPalette(palette);
    QFont font = app.font();
    font.setPixelSize(13);
    app.setFont(font);
    app.setStyleSheet(QString::fromLatin1(kStyleSheet));
}

} // namespace theme
