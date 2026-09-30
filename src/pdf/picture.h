#pragma once

#include "context.h"

#include <QByteArray>
#include <QImage>
#include <QRectF>

class Document;

// Image drawn on a page.
struct Picture
{
    QRectF box;
    fz_matrix placement;
};

// Pixels plus untouched JPEG bytes.
struct Taken
{
    QImage image;
    QByteArray encoded;
    QString error;
};

QImage decodeImage(fz_context *ctx, fz_image *image);

// Original JPEG, if reusable losslessly.
QByteArray jpegOf(fz_context *ctx, fz_image *image);

// Removes picture from its page.
Taken takePicture(Document &doc, int index, const Picture &picture);
