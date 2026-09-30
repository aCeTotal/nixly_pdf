#pragma once

#include <QString>

// What converts a file.
enum class Source { Pdf, Drawn, Office, Unknown };

Source sourceOf(const QString &path);

// Open dialog filter, all sources.
QString openFilter();
