#pragma once

#include <QLineEdit>

struct TextRun;

// Inline editor for one run.
class RunEditor : public QLineEdit
{
    Q_OBJECT

public:
    explicit RunEditor(QWidget *parent);
    void open(const TextRun &run);
    void place(const QRectF &box, double pixelSize);

signals:
    void committed(const QString &text);
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void commit();
    void cancel();
    void fit();

    int baseWidth = 0;
};
