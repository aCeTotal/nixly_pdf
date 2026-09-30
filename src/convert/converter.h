#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <future>
#include <memory>

// Converts files to PDFs sequentially.
class Converter : public QObject
{
    Q_OBJECT

public:
    explicit Converter(QObject *parent = nullptr);
    ~Converter() override;
    void start(const QStringList &files);
    bool busy() const { return running; }

signals:
    void finished(const QStringList &sources, const QStringList &pdfs, const QString &error);

private:
    void next();
    void finish(const QString &error);
    void officeDone();
    void startOffice(const QString &file, const QString &folder);
    void startDrawn(const QString &file, const QString &target);

    QStringList sources;
    QStringList waiting;
    QStringList made;
    QString expected;
    std::unique_ptr<QTemporaryDir> scratch;
    QProcess office;
    QTimer patience;
    std::future<void> drawing;
    bool running = false;
};
