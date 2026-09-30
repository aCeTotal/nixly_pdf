#include "fixture.h"

#include "mark/annotate.h"
#include "mark/pickmarks.h"

#include <QGuiApplication>
#include <QLineF>
#include <QTemporaryDir>
#include <algorithm>

namespace {

bool near(QPointF a, QPointF b)
{
    return QLineF(a, b).length() < 0.5;
}

bool sameBox(const QRectF &a, const QRectF &b)
{
    return near(a.topLeft(), b.topLeft()) && near(a.bottomRight(), b.bottomRight());
}

const Mark *findMark(const std::vector<Mark> &marks, MarkKind kind)
{
    const auto hit = std::ranges::find(marks, kind, &Mark::kind);
    return hit == marks.end() ? nullptr : &*hit;
}

std::vector<Mark> sampleMarks(int page)
{
    Mark box;
    box.page = page;
    box.box = QRectF(60, 60, 120, 80);
    Mark ring = box;
    ring.kind = MarkKind::Ellipse;
    ring.box.translate(0, 120);
    Mark cloud = box;
    cloud.kind = MarkKind::Cloud;
    cloud.box.translate(200, 0);
    cloud.ink = Qt::blue;
    Mark arrow = box;
    arrow.kind = MarkKind::Arrow;
    arrow.tail = QPointF(300, 300);
    arrow.tip = QPointF(400, 250);
    Mark callout = box;
    callout.kind = MarkKind::Callout;
    callout.box = QRectF(250, 400, 180, 60);
    callout.tip = QPointF(120, 520);
    callout.text = QStringLiteral("Look here");
    callout.size = 14;
    callout.fill = Qt::white;
    Mark picture = box;
    picture.kind = MarkKind::Image;
    picture.box = QRectF(60, 600, 100, 50);
    picture.image = QImage(40, 20, QImage::Format_RGB32);
    picture.image.fill(Qt::green);
    return {box, ring, cloud, arrow, callout, picture};
}

void checkMarks(const QString &source, const QString &path, FontLibrary &fonts)
{
    QString error;
    std::unique_ptr<Document> doc = Document::open(source, &error);
    ModuleSet modules(fonts);
    MarkSet marks;
    for (const Mark &mark : sampleMarks(doc->slot(0).id))
        marks.add(mark);
    const bool saved = doc->save(path, [&](fz_context *ctx, pdf_document *copy) {
        return annotate(ctx, copy, {*doc, marks});
    }, &error);
    check(saved, "saves marks as annotations");
    std::unique_ptr<Document> reopened = Document::open(path, &error);
    const std::vector<Mark> read = reopened ? pickMarks(*reopened) : std::vector<Mark>();
    check(read.size() == 6, "reads every mark back");
    const std::vector<Mark> wrote = sampleMarks(0);
    const Mark *box = findMark(read, MarkKind::Rectangle);
    check(box && sameBox(box->box, wrote[0].box) && box->ink == wrote[0].ink, "rectangle keeps place and colour");
    const Mark *ring = findMark(read, MarkKind::Ellipse);
    check(ring && sameBox(ring->box, wrote[1].box), "ellipse keeps its place");
    const Mark *cloud = findMark(read, MarkKind::Cloud);
    check(cloud && sameBox(cloud->box, wrote[2].box) && cloud->ink == Qt::blue, "cloud keeps place and colour");
    const Mark *arrow = findMark(read, MarkKind::Arrow);
    check(arrow && near(arrow->tail, wrote[3].tail) && near(arrow->tip, wrote[3].tip), "arrow keeps its direction");
    const Mark *callout = findMark(read, MarkKind::Callout);
    const bool bubble = callout && sameBox(callout->box, wrote[4].box) && near(callout->tip, wrote[4].tip);
    check(bubble && callout->text == "Look here" && callout->size == 14, "callout keeps box, tip and text");
    const Mark *picture = findMark(read, MarkKind::Image);
    const bool green = picture && qGreen(picture->image.pixel(picture->image.rect().center())) > 200;
    check(picture && sameBox(picture->box, wrote[5].box) && green, "image keeps place and pixels");
    check(reopened && pickMarks(*reopened).empty(), "lifted annotations leave the page");
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    const QString source = dir.filePath("sample.pdf");
    writeSample(QFile::encodeName(source));
    FontLibrary fonts;
    checkMarks(source, dir.filePath("marked.pdf"), fonts);
    return verdict();
}
