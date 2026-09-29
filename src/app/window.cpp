#include "window.h"

#include "edit/editbar.h"
#include "pdf/document.h"
#include "pdf/renderer.h"
#include "toast.h"
#include "topbar.h"
#include "view/pageview.h"
#include "view/thumbstrip.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QVBoxLayout>

namespace {

constexpr int kPasswordTries = 3;
const QString kPdfFilter = QStringLiteral("PDF documents (*.pdf)");

} // namespace

Window::Window(const QString &path)
    : bar(new TopBar(buildFileMenu())), editBar(new EditBar), strip(new ThumbStrip), view(new PageView),
      toast(nullptr)
{
    resize(1280, 900);
    setAcceptDrops(true);
    auto *central = new QWidget;
    auto *column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(bar);
    column->addWidget(editBar);
    auto *row = new QHBoxLayout;
    row->setSpacing(0);
    row->addWidget(strip);
    row->addWidget(view, 1);
    column->addLayout(row, 1);
    setCentralWidget(central);
    toast = new Toast(central);

    connect(bar, &TopBar::modeChosen, this, [this](Mode next) { setMode(next); });
    connect(view, &PageView::currentPageChanged, strip, &ThumbStrip::setCurrent);
    connect(view, &PageView::runEdited, this, [this](int index, const TextRun &run, const QString &text) {
        editRun(index, run, text);
    });
    connect(view, &PageView::stampPlaced, this, [this](int index, QPointF centre) { placeSignature(index, centre); });
    connect(view, &PageView::stampCancelled, this, [this] { setMode(Mode::Read); });
    connect(strip, &ThumbStrip::pageChosen, view, &PageView::goToPage);
    connect(strip, &ThumbStrip::pageMoved, this, [this](int from, int to) { movePage(from, to); });
    connect(strip, &ThumbStrip::deleteRequested, this, [this](int index) { deletePage(index); });
    connect(editBar, &EditBar::addBlank, this, [this] { addBlank(); });
    connect(editBar, &EditBar::insertFile, this, [this] { insertFile(); });
    connect(editBar, &EditBar::deletePage, this, [this] { deletePage(view->currentPage()); });

    markDirty(false);
    if (!path.isEmpty())
        open(path);
}

Window::~Window() = default;

QMenu *Window::buildFileMenu()
{
    auto *menu = new QMenu(this);
    QAction *openAction = menu->addAction(tr("Open…"), QKeySequence::Open, this, [this] { chooseFile(); });
    menu->addSeparator();
    saveAction = menu->addAction(tr("Save"), QKeySequence::Save, this, [this] { save(); });
    saveAsAction = menu->addAction(tr("Save as…"), QKeySequence::SaveAs, this, [this] { saveAs(); });
    menu->addSeparator();
    QAction *quitAction = menu->addAction(tr("Quit"), QKeySequence::Quit, this, &QWidget::close);
    addActions({openAction, saveAction, saveAsAction, quitAction});
    return menu;
}

void Window::chooseFile()
{
    if (!confirmDiscard())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), QString(), kPdfFilter);
    if (!path.isEmpty())
        open(path);
}

void Window::open(const QString &path)
{
    QString error;
    std::unique_ptr<Document> next = Document::open(path, &error);
    if (!next) {
        toast->pop(tr("Could not open %1: %2").arg(QFileInfo(path).fileName(), error), Tone::Error);
        return;
    }
    if (next->locked() && !unlock(*next))
        return;
    auto nextRenderer = std::make_unique<Renderer>(*next);
    pending.reset();
    view->setDocument(next.get(), nextRenderer.get());
    strip->setDocument(next.get(), nextRenderer.get());
    renderer = std::move(nextRenderer);
    doc = std::move(next);
    setMode(Mode::Read);
    markDirty(false);
}

bool Window::unlock(Document &candidate)
{
    for (int attempt = 0; attempt < kPasswordTries; ++attempt) {
        bool accepted = false;
        const QString password = QInputDialog::getText(this, tr("Protected document"), tr("Password"),
                                                       QLineEdit::Password, QString(), &accepted);
        if (!accepted)
            return false;
        if (candidate.unlock(password))
            return true;
    }
    toast->pop(tr("Wrong password"), Tone::Error);
    return false;
}

bool Window::save()
{
    if (!doc)
        return false;
    QString error;
    if (!doc->save(doc->path(), &error)) {
        toast->pop(tr("Save failed: %1").arg(error), Tone::Error);
        return false;
    }
    markDirty(false);
    toast->pop(tr("Saved"), Tone::Info);
    return true;
}

bool Window::saveAs()
{
    if (!doc)
        return false;
    QString path = QFileDialog::getSaveFileName(this, tr("Save PDF as"), doc->path(), kPdfFilter);
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        path += QStringLiteral(".pdf");
    QString error;
    if (!doc->save(path, &error)) {
        toast->pop(tr("Save failed: %1").arg(error), Tone::Error);
        return false;
    }
    markDirty(false);
    toast->pop(tr("Saved as %1").arg(QFileInfo(path).fileName()), Tone::Info);
    return true;
}

bool Window::confirmDiscard()
{
    if (!dirty)
        return true;
    const auto choice = QMessageBox::question(this, tr("Unsaved changes"), tr("Save changes to this document?"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (choice == QMessageBox::Save)
        return save();
    return choice == QMessageBox::Discard;
}

void Window::markDirty(bool changed)
{
    dirty = changed;
    const QString name = doc ? QFileInfo(doc->path()).fileName() : QString();
    bar->setTitle(name.isEmpty() ? QString() : (changed ? QStringLiteral("• ") : QString()) + name);
    setWindowTitle(name.isEmpty() ? tr("Nixly PDF") : tr("%1 — Nixly PDF").arg(name));
    bar->setModesEnabled(doc != nullptr);
    saveAction->setEnabled(doc != nullptr);
    saveAsAction->setEnabled(doc != nullptr);
}

void Window::closeEvent(QCloseEvent *event)
{
    if (confirmDiscard())
        event->accept();
    else
        event->ignore();
}

void Window::dragEnterEvent(QDragEnterEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.front().toLocalFile().endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        event->acceptProposedAction();
}

void Window::dropEvent(QDropEvent *event)
{
    if (confirmDiscard())
        open(event->mimeData()->urls().front().toLocalFile());
}
