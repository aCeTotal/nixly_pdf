#pragma once

class MarkSet;
class ModuleSet;

// Editable things over pages.
struct Layers
{
    ModuleSet *modules = nullptr;
    MarkSet *marks = nullptr;
};
