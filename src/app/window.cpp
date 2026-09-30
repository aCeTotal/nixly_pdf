#include "window.h"

#include "convert/converter.h"
#include "convert/formats.h"
#include "edit/editbar.h"
#include "lockdialog.h"
#include "mark/annotate.h"
#include "mark/pickmarks.h"
#include "module/flatten.h"
#include "module/moduleset.h"
#include "pdf/document.h"
#include "pdf/pagetree.h"
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
      toast(nullptr), converter(new Converter(this))
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
    connect(view, &PageView::liftRequested, this,
            [this](int index, const Passage &passage) { lift(index, passage); });
    connect(view, &PageView::pictureRequested, this,
            [this](int index, const Picture &picture) { liftPicture(index, picture); });
    connect(view, &PageView::placementCancelled, this, [this] { setMode(mode); });
    connect(strip, &ThumbStrip::pageChosen, view, &PageView::goToPage);
    connect(strip, &ThumbStrip::pageMoved, this, [this](int from, int to) { movePage(from, to); });
    connect(strip, &ThumbStrip::deleteRequested, this, [this](int index) { deletePage(index); });
    connect(editBar, &EditBar::addBlank, this, [this] { addBlank(); });
    connect(editBar, &EditBar::insertFile, this, [this] { insertFile(); });
    connect(editBar, &EditBar::deletePage, this, [this] { deletePage(view->currentPage()); });
    connect(editBar, &EditBar::addText, this, [this] { addText(); });
    connect(editBar, &EditBar::addDate, this, [this] { addDate(); });
    connect(editBar, &EditBar::drawMark, this, [this](MarkKind kind) { drawMark(kind); });
    connect(view, &PageView::toolFinished, editBar, &EditBar::release);
    connect(editBar, &EditBar::recognize, this, [this] { recognizePage(); });

    connect(converter, &Converter::finished, this,
            [this](const QStringList &sources, const QStringList &pdfs, const QString &error) {
                openConverted(sources, pdfs, error);
            });

    markDirty(false);
    if (!path.isEmpty())
        openFiles({path});
}

Window::~Window()
{
    if (recognition.valid())
        recognition.wait();
}

QMenu *Window::buildFileMenu()
{
    auto *menu = new QMenu(this);
    QAction *openAction = menu->addAction(tr("Open…"), QKeySequence::Open, this, [this] { chooseFile(); });
    menu->addSeparator();
    saveAction = menu->addAction(tr("Save"), QKeySequence::Save, this, [this] { save(); });
    saveAsAction = menu->addAction(tr("Save as…"), QKeySequence::SaveAs, this, [this] { saveAs(); });
    encryptAction = menu->addAction(tr("Save encrypted…"), this, [this] { saveEncrypted(); });
    menu->addSeparator();
    QAction *quitAction = menu->addAction(tr("Quit"), QKeySequence::Quit, this, &QWidget::close);
    addActions({openAction, saveAction, saveAsAction, quitAction});
    return menu;
}

void Window::chooseFile()
{
    if (!confirmDiscard())
        return;
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open"), QString(), openFilter());
    if (!files.isEmpty())
        openFiles(files);
}

// PDFs open directly, others convert.
void Window::openFiles(const QStringList &files)
{
    if (files.size() == 1 && sourceOf(files.front()) == Source::Pdf) {
        open(files.front());
        return;
    }
    if (converter->busy()) {
        toast->pop(tr("Still converting the previous file"), Tone::Info);
        return;
    }
    toast->pop(tr("Converting %1 …").arg(QFileInfo(files.front()).fileName()), Tone::Info);
    converter->start(files);
}

// Opens first PDF, appends rest.
void Window::openConverted(const QStringList &sources, const QStringList &pdfs, const QString &error)
{
    if (!error.isEmpty()) {
        toast->pop(error, Tone::Error);
        return;
    }
    open(pdfs.front());
    if (!doc || doc->path() != pdfs.front())
        return;
    const QFileInfo first(sources.front());
    suggested = first.dir().filePath(first.completeBaseName() + QStringLiteral(".pdf"));
    QString failure;
    for (qsizetype i = 1; i < pdfs.size() && failure.isEmpty(); ++i)
        insertPdf(*doc, doc->count(), pdfs[i], &failure);
    if (!failure.isEmpty())
        toast->pop(tr("Could not add every file: %1").arg(failure), Tone::Error);
    if (pdfs.size() > 1)
        structureChanged(0);
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
    if (recognition.valid())
        recognition.wait();
    ++generation;
    auto nextRenderer = std::make_unique<Renderer>(*next);
    auto nextFonts = std::make_unique<FontLibrary>();
    auto nextModules = std::make_unique<ModuleSet>(*nextFonts);
    auto nextMarks = std::make_unique<MarkSet>();
    for (Mark &mark : pickMarks(*next))
        nextMarks->add(std::move(mark));
    connect(nextModules.get(), &ModuleSet::changed, this, [this] { markDirty(true); });
    connect(nextMarks.get(), &MarkSet::changed, this, [this] { markDirty(true); });
    const Layers layers{nextModules.get(), nextMarks.get()};
    view->setDocument(next.get(), nextRenderer.get(), layers);
    strip->setDocument(next.get(), nextRenderer.get(), layers);
    modules = std::move(nextModules);
    marks = std::move(nextMarks);
    fonts = std::move(nextFonts);
    renderer = std::move(nextRenderer);
    doc = std::move(next);
    suggested.clear();
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
    return suggested.isEmpty() ? writeTo(doc->path()) : saveAs();
}

bool Window::writeTo(const QString &path)
{
    Document::Finisher finish;
    if (!modules->empty() || !marks->empty())
        finish = [this](fz_context *ctx, pdf_document *copy) {
            const QString failure = flatten(ctx, copy, {*doc, *modules});
            return failure.isEmpty() ? annotate(ctx, copy, {*doc, *marks}) : failure;
        };
    QString error;
    if (!doc->save(path, finish, &error)) {
        toast->pop(tr("Save failed: %1").arg(error), Tone::Error);
        return false;
    }
    suggested.clear();
    markDirty(false);
    toast->pop(tr("Saved %1").arg(QFileInfo(path).fileName()), Tone::Info);
    return true;
}

bool Window::saveAs()
{
    const QString path = doc ? chooseTarget(tr("Save PDF as")) : QString();
    return !path.isEmpty() && writeTo(path);
}

bool Window::saveEncrypted()
{
    LockDialog lock(this);
    if (!doc || lock.exec() != QDialog::Accepted)
        return false;
    const QString path = chooseTarget(tr("Save encrypted PDF as"));
    if (path.isEmpty())
        return false;
    doc->protect(lock.passphrase());
    return writeTo(path);
}

QString Window::chooseTarget(const QString &title)
{
    const QString start = suggested.isEmpty() ? doc->path() : suggested;
    QString path = QFileDialog::getSaveFileName(this, title, start, kPdfFilter);
    if (!path.isEmpty() && !path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        path += QStringLiteral(".pdf");
    return path;
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
    encryptAction->setEnabled(doc != nullptr);
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
    const auto readable = [](const QUrl &url) {
        return url.isLocalFile() && sourceOf(url.toLocalFile()) != Source::Unknown;
    };
    if (!urls.isEmpty() && std::ranges::all_of(urls, readable))
        event->acceptProposedAction();
}

void Window::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls())
        files << url.toLocalFile();
    if (confirmDiscard())
        openFiles(files);
}
