#pragma once

#include "mode.h"

#include <QFrame>

class QButtonGroup;
class QLabel;
class QMenu;

// File menu, title, mode switch.
class TopBar : public QFrame
{
    Q_OBJECT

public:
    TopBar(QMenu *fileMenu, QWidget *parent = nullptr);
    void setTitle(const QString &title);
    void setMode(Mode mode);
    void setModesEnabled(bool enabled);

signals:
    void modeChosen(Mode mode);

private:
    QLabel *title;
    QButtonGroup *modes;
};
