#pragma once

#include "app/mode.h"
#include "smoothvalue.h"
#include "ticker.h"

#include <QWidget>
#include <optional>
#include <vector>

class Document;
class Renderer;

// Page previews for scrubbing, reordering.
class ThumbStrip : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kWidth = 196;
    static constexpr double kThumb = 132;

    explicit ThumbStrip(QWidget *parent = nullptr);

    void setDocument(Document *doc, Renderer *renderer);
    void setMode(Mode mode);
    void relayout();
    void setCurrent(int index);

signals:
    void pageChosen(int index);
    void pageMoved(int from, int to);
    void deleteRequested(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Drag
    {
        int from;
        double grab;
        bool lifted;
    };

    bool frame(double seconds);
    int slotAt(double y) const;
    int dropSlot() const;
    double slotTop(int index) const;
    void follow(int index);
    void scrub(double y);
    void arrange(int to);
    void paintThumb(QPainter &painter, int index, double top, bool lifted);

    Document *doc = nullptr;
    Renderer *renderer = nullptr;
    Mode mode = Mode::Read;
    std::vector<double> tops;
    std::vector<SmoothValue> shifts;
    double extent = 0;
    SmoothValue scroll;
    int current = -1;
    std::optional<Drag> drag;
    double pointer = 0;
    Ticker ticker;
};
