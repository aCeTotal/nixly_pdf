#pragma once

#include "mode.h"
#include "sign/signature.h"
#include "sign/signaturelayout.h"

#include <QMainWindow>
#include <memory>
#include <optional>

class Document;
class EditBar;
class PageView;
class Renderer;
class ThumbStrip;
class Toast;
class TopBar;
struct TextRun;

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
    void open(const QString &path);
    bool unlock(Document &candidate);
    bool save();
    bool saveAs();
    bool confirmDiscard();
    void setMode(Mode next);
    void startSigning();
    void placeSignature(int index, QPointF centre);
    void editRun(int index, const TextRun &run, const QString &text);
    void structureChanged(int focus);
    void addBlank();
    void insertFile();
    void deletePage(int index);
    void movePage(int from, int to);
    void markDirty(bool changed);

    std::unique_ptr<Document> doc;
    std::unique_ptr<Renderer> renderer;
    TopBar *bar;
    EditBar *editBar;
    ThumbStrip *strip;
    PageView *view;
    Toast *toast;
    QAction *saveAction;
    QAction *saveAsAction;
    Mode mode = Mode::Read;
    bool dirty = false;
    Signature lastSignature;
    std::optional<SignatureLayout> pending;
};
