#include "window.h"

#include "edit/editbar.h"
#include "pdf/document.h"
#include "pdf/pagetree.h"
#include "pdf/renderer.h"
#include "pdf/stamp.h"
#include "pdf/textedit.h"
#include "toast.h"
#include "sign/signdialog.h"
#include "topbar.h"
#include "view/pageview.h"
#include "view/thumbstrip.h"

#include <QFileDialog>

void Window::setMode(Mode next)
{
    if (next == Mode::Sign) {
        startSigning();
        return;
    }
    mode = next;
    pending.reset();
    bar->setMode(next);
    view->setMode(next);
    strip->setMode(next);
    if (next == Mode::Edit)
        editBar->reveal();
    else
        editBar->conceal();
}

void Window::startSigning()
{
    SignDialog dialog(lastSignature, this);
    if (dialog.exec() != QDialog::Accepted) {
        bar->setMode(mode);
        return;
    }
    lastSignature = dialog.signature();
    setMode(Mode::Read);
    pending.emplace(lastSignature);
    bar->setMode(Mode::Sign);
    view->armStamp({pending->size(), [layout = *pending](QPainter &painter) { layout.paint(painter); }});
    toast->pop(tr("Click where the signature goes  ·  Esc cancels"), Tone::Info);
}

void Window::placeSignature(int index, QPointF centre)
{
    const QString error = placeStamp(*doc, index, pending->stampLines(centre));
    pending.reset();
    bar->setMode(Mode::Read);
    if (!error.isEmpty()) {
        toast->pop(tr("Could not sign: %1").arg(error), Tone::Error);
        return;
    }
    const int id = doc->slot(index).id;
    renderer->forget(id);
    view->forgetText(id);
    markDirty(true);
    toast->pop(tr("Signed page %1").arg(index + 1), Tone::Info);
}

void Window::editRun(int index, const TextRun &run, const QString &text)
{
    const EditOutcome outcome = replaceRun(*doc, index, run, text);
    if (!outcome.error.isEmpty()) {
        toast->pop(tr("Could not change text: %1").arg(outcome.error), Tone::Error);
        return;
    }
    const int id = doc->slot(index).id;
    renderer->forget(id);
    view->forgetText(id);
    markDirty(true);
    if (!outcome.substitute.isEmpty())
        toast->pop(tr("The embedded font lacks these letters, so %1 was used").arg(outcome.substitute), Tone::Info);
}

void Window::structureChanged(int focus)
{
    view->relayout();
    strip->relayout();
    markDirty(true);
    view->goToPage(focus);
}

void Window::addBlank()
{
    const int at = view->currentPage() + 1;
    QString error;
    if (!insertBlankPage(*doc, at, &error)) {
        toast->pop(tr("Could not add page: %1").arg(error), Tone::Error);
        return;
    }
    structureChanged(at);
}

void Window::insertFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Insert pages from"), QString(),
                                                      tr("PDF documents (*.pdf)"));
    if (path.isEmpty())
        return;
    const int at = view->currentPage() + 1;
    const int before = doc->count();
    QString error;
    if (!insertPdf(*doc, at, path, &error)) {
        toast->pop(tr("Could not insert: %1").arg(error), Tone::Error);
        return;
    }
    structureChanged(at);
    toast->pop(tr("Inserted %n page(s)", nullptr, doc->count() - before), Tone::Info);
}

void Window::deletePage(int index)
{
    if (!doc || doc->count() <= 1 || index < 0)
        return;
    QString error;
    if (!::deletePage(*doc, index, &error)) {
        toast->pop(tr("Could not delete page: %1").arg(error), Tone::Error);
        return;
    }
    structureChanged(std::min(index, doc->count() - 1));
    toast->pop(tr("Deleted page %1").arg(index + 1), Tone::Info);
}

void Window::movePage(int from, int to)
{
    QString error;
    if (!::movePage(*doc, from, to, &error)) {
        toast->pop(tr("Could not move page: %1").arg(error), Tone::Error);
        return;
    }
    structureChanged(to);
}
