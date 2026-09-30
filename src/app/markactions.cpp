#include "window.h"

#include "edit/editbar.h"
#include "mark/mark.h"
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
