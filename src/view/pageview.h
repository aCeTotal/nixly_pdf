#pragma once

#include "app/mode.h"
#include "edit/runcache.h"
#include "overlay.h"
#include "pagelayout.h"
#include "smoothvalue.h"
#include "ticker.h"

#include <QWidget>
#include <functional>
#include <optional>

class Renderer;
class RunEditor;

// Stamp painter centred at origin.
struct StampPreview
{
    QSizeF size;
    std::function<void(QPainter &)> paint;
};

// Continuous, smoothly scrolling page view.
class PageView : public QWidget
{
    Q_OBJECT

public:
    explicit PageView(QWidget *parent = nullptr);

    void setDocument(Document *doc, Renderer *renderer);
    void setMode(Mode mode);
    void relayout();
    void forgetText(int id);
    void goToPage(int index);
    int currentPage() const { return current; }
    void armStamp(StampPreview preview);

signals:
    void currentPageChanged(int index);
    void runEdited(int index, const TextRun &run, const QString &text);
    void stampPlaced(int index, QPointF center);
    void stampCancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Spot
    {
        int index;
        QPointF point;
    };
    struct Editing
    {
        int index;
        TextRun run;
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
    void paintPages(QPainter &painter);
    void paintOverlays(QPainter &painter);
    void paintEmpty(QPainter &painter);
    bool zoomKey(int key);
    double scrollStep(int key) const;
    void cancelStamp();
    void hoverText(QPointF pos);
    void beginEdit();
    void placeEditor();

    Document *doc = nullptr;
    Renderer *renderer = nullptr;
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
    std::optional<Editing> hovered;
    std::optional<Editing> editing;
    RunEditor *editor;

    std::optional<StampPreview> stamp;
    std::optional<QPointF> cursor;
};
