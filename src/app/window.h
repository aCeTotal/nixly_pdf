#pragma once

#include "mode.h"
#include "ocr/ocr.h"
#include "sign/signature.h"

#include <QMainWindow>
#include <future>
#include <memory>

class Converter;
class Document;
class EditBar;
class FontLibrary;
class QImage;
class MarkSet;
class ModuleSet;
class PageView;
class Renderer;
class ThumbStrip;
class Toast;
class TopBar;
struct Passage;
struct Picture;
enum class MarkKind;

class Window : public QMainWindow
{
public:
    explicit Window(const QString &path);
    ~Window() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QMenu *buildFileMenu();
    void chooseFile();
    void openFiles(const QStringList &files);
    void openConverted(const QStringList &sources, const QStringList &pdfs, const QString &error);
    void open(const QString &path);
    bool unlock(Document &candidate);
    bool save();
    bool saveAs();
    bool saveEncrypted();
    QString chooseTarget(const QString &title);
    bool writeTo(const QString &path);
    bool confirmDiscard();
    void setMode(Mode next);
    void startSigning();
    void lift(int index, const Passage &passage);
    void addText();
    void addDate();
    void drawMark(MarkKind kind);
    void liftPicture(int index, const Picture &picture);
    QImage chooseImage();
    void recognizePage();
    void applyRecognized(int page, const OcrResult &result);
    void structureChanged(int focus);
    void addBlank();
    void insertFile();
    void deletePage(int index);
    void movePage(int from, int to);
    void markDirty(bool changed);

    std::unique_ptr<Document> doc;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<FontLibrary> fonts;
    std::unique_ptr<ModuleSet> modules;
    std::unique_ptr<MarkSet> marks;
    std::future<void> recognition;
    TopBar *bar;
    EditBar *editBar;
    ThumbStrip *strip;
    PageView *view;
    Toast *toast;
    Converter *converter;
    QString suggested;
    QAction *saveAction;
    QAction *saveAsAction;
    QAction *encryptAction;
    Mode mode = Mode::Read;
    bool dirty = false;
    unsigned generation = 0;
    Signature lastSignature;
};
