#include "window.h"

#include "edit/editbar.h"
#include "mark/markset.h"
#include "pdf/document.h"
#include "pdf/picture.h"
#include "pdf/renderer.h"
#include "toast.h"
#include "view/pageview.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>

void Window::drawMark(MarkKind kind)
{
    Mark mark;
    mark.kind = kind;
    mark.fill = kind == MarkKind::Callout ? QColor(Qt::white) : QColor();
    mark.image = kind == MarkKind::Image ? chooseImage() : QImage();
    if (kind == MarkKind::Image && mark.image.isNull()) {
        editBar->release();
        return;
    }
    view->armMark(mark);
}

QImage Window::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Insert image"), QString(),
                                                      tr("Images (*.png *.jpg *.jpeg *.gif *.bmp)"));
    if (path.isEmpty())
        return {};
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
        toast->pop(tr("Could not read %1").arg(QFileInfo(path).fileName()), Tone::Error);
    return image;
}

void Window::liftPicture(int index, const Picture &picture)
{
    Taken taken = takePicture(*doc, index, picture);
    if (!taken.error.isEmpty()) {
        toast->pop(tr("Could not pick up the picture: %1").arg(taken.error), Tone::Error);
        return;
    }
    Mark mark;
    mark.kind = MarkKind::Image;
    mark.page = doc->slot(index).id;
    mark.box = picture.box;
    mark.image = std::move(taken.image);
    mark.encoded = std::move(taken.encoded);
    renderer->forget(mark.page);
    view->forgetText(mark.page);
    view->pick(marks->add(std::move(mark)));
}
