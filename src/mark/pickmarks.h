#pragma once

#include "mark.h"

#include <vector>

class Document;

// Lifts editable annotations into marks.
std::vector<Mark> pickMarks(Document &doc);
