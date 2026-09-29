#pragma once

#include <QString>

class Document;

bool movePage(Document &doc, int from, int to, QString *error);
bool insertBlankPage(Document &doc, int at, QString *error);
bool insertPdf(Document &doc, int at, const QString &path, QString *error);
bool deletePage(Document &doc, int index, QString *error);
