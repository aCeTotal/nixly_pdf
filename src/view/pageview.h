#pragma once

#include "app/mode.h"
#include "edit/runcache.h"
#include "layers.h"
#include "mark/markset.h"
#include "module/moduleset.h"
#include "overlay.h"
#include "pagelayout.h"
#include "smoothvalue.h"
#include "ticker.h"

#include <QWidget>
#include <optional>

class CalloutEditor;
class MarkBar;
class ModuleBar;
class ModuleEditor;
class Renderer;

// Continuous, smoothly scrolling page view.
class PageView : public QWidget
{
    Q_OBJECT

public:
    explicit PageView(QWidget *parent = nullptr);

    void setDocument(Document *doc, Renderer *renderer, Layers layers);
    void setMode(Mode mode);
    void relayout();
    void forgetText(int id);
    void goToPage(int index);
    int currentPage() const { return current; }
    void armPlacement(const Module &prototype);
    void armMark(const Mark &prototype);
    void pick(int id);
    void select(int id);
    void editText(int id);

signals:
    void currentPageChanged(int index);
    void liftRequested(int index, const Passage &passage);
    void pictureRequested(int index, const Picture &picture);
    void placementCancelled();
    void toolFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Spot
    {
        int index;
        QPointF point;
    };
    enum class Grip { Move, Width, Size };
    struct Drag
    {
        int id;
        Grip grip;
        QPointF grab;
        Module start;
        QRectF bounds;
    };
    struct Hover
    {
        int index;
        Passage passage;
        std::optional<Picture> picture;
    };
    struct Ghost
    {
        Module module;
        Layout layout;
    };
    struct MarkDrag
    {
        int id;
        int handle;
        QPointF grab;
        Mark start;
    };
    struct Sketch
    {
        int index;
        QPointF from;
        QPointF press;
        Mark mark;
    };

    bool frame(double seconds);
    void zoomTo(double zoom, QPointF anchor);
    void rebuild(double zoom, QPointF anchor);
    void clampScroll();
    void scrolled();
    double fitZoom() const;
    ScrollState scrollState() const;
    std::optional<Spot> spotAt(QPointF pos) const;
    QRectF toView(int index, const QRectF &box) const;
    QTransform pageToView(int index) const;
    bool zoomKey(int key);
    double scrollStep(int key) const;
    bool moduleKey(QKeyEvent *event);
    static std::optional<QPointF> nudgeOf(const QKeyEvent *event);
    void cancelPlacement();
    void paintPages(QPainter &painter);
    void paintOverlays(QPainter &painter);
    void paintEmpty(QPainter &painter);

    bool interactive() const;
    void paintModules(QPainter &painter, int index);
    void paintSelection(QPainter &painter);
    void paintGhost(QPainter &painter);
    std::optional<Grip> gripAt(QPointF pos) const;
    bool pressModule(QPointF pos);
    void dragModule(QPointF pos);
    bool placeModule(QPointF pos);
    void hoverText(QPointF pos);
    void showBar();
    void syncEditors();
    QPoint barPoint(const QRectF &area, const QWidget *panel) const;

    void buildModuleTools();
    void buildMarkTools();
    void paintMarks(QPainter &painter, int index);
    void paintMarkSelection(QPainter &painter);
    std::optional<int> markHandleAt(QPointF pos) const;
    bool pressMark(QPointF pos);
    void dragMark(QPointF pos);
    void startSketch(QPointF pos);
    void drawSketch(QPointF pos);
    void finishSketch(QPointF pos);
    void editCallout(int id);
    bool markKey(QKeyEvent *event);
    void showMarkBar();
    void syncMarks();

    Document *doc = nullptr;
    Renderer *renderer = nullptr;
    ModuleSet *modules = nullptr;
    Mode mode = Mode::Read;
    PageLayout layout;
    SmoothValue scrollX;
    SmoothValue scrollY;
    SmoothValue zoom{0.0005};
    QPointF zoomAnchor;
    int settledScale = 0;
    int previousScale = 0;
    bool fitted = true;
    int current = -1;
    Ticker ticker;
    ScrollOverlay overlay;
    std::optional<double> gripOffset;

    RunCache runs;
    std::optional<Hover> hovered;
    int selected = 0;
    std::optional<Drag> drag;
    std::optional<Ghost> ghost;
    std::optional<QPointF> cursor;
    ModuleEditor *editor;
    ModuleBar *bar;

    MarkSet *marks = nullptr;
    int picked = 0;
    std::optional<Mark> armed;
    std::optional<Sketch> sketch;
    std::optional<MarkDrag> markDrag;
    MarkBar *markBar;
    CalloutEditor *callout;
};
