#include "signature.h"

#include "scriptfonts.h"

#include <algorithm>

Module signatureModule(const Signature &signature)
{
    const std::vector<ScriptFont> &fonts = scriptFonts();
    Module module;
    module.kind = ModuleKind::Signature;
    module.text = signature.name;
    module.size = signature.size;
    module.color = signature.color;
    if (!fonts.empty())
        module.font.family = fonts[size_t(std::clamp(signature.font, 0, int(fonts.size()) - 1))].family;
    return module;
}
