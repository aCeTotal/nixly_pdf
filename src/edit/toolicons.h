#pragma once

#include <QIcon>

enum class Tool { Text, Date, Image, Arrow, Callout, Rectangle, Ellipse, Cloud, Recognize, Blank, Insert, Delete };

// Line icon in app style.
QIcon toolIcon(Tool tool);
