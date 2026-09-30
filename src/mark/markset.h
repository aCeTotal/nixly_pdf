#pragma once

#include "mark.h"

#include <QObject>
#include <vector>

// Document marks.
class MarkSet : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    int add(Mark mark);
    void update(const Mark &mark);
    void remove(int id);
    void dropPage(int page);

    const Mark *find(int id) const;
    std::vector<int> onPage(int page) const;
    int hit(int page, QPointF point, double reach) const;
    bool empty() const { return marks.empty(); }

signals:
    void changed(int page);

private:
    std::vector<Mark> marks;
    int nextId = 1;
};
