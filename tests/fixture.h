#pragma once

#include "module/moduleset.h"
#include "pdf/document.h"
#include "pdf/textrun.h"

#include <QByteArray>
#include <QRgb>
#include <vector>

// Prints and counts a result.
void check(bool ok, const char *what);
// Exit code for all checks.
int verdict();

// Helvetica F1, Courier F2 pages.
void writePages(const QByteArray &path, const std::vector<QByteArray> &contents);
void writeSample(const QByteArray &path);
// Page one as a scan.
void writeScan(const QByteArray &source, const QByteArray &path);

const TextRun *findRun(const std::vector<TextRun> &runs, const QString &text);
bool pageHas(Document &doc, int index, const QString &text);
QRgb pixel(const QString &path, int page, QPoint at);
bool save(Document &doc, ModuleSet &modules, const QString &path);
