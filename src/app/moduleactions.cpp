#include "window.h"

#include "module/moduleset.h"
#include "module/pickup.h"
#include "pdf/document.h"
#include "pdf/renderer.h"
#include "toast.h"
#include "view/pageview.h"

#include <QLocale>

namespace {

constexpr double kTextSize = 12;
constexpr double kDateSize = 11;
constexpr double kTextWidth = 220;
constexpr double kWrapSlack = 1.05;
const QString kPlainFamily = QStringLiteral("Liberation Sans");

Module plainModule(ModuleKind kind, double size)
{
    Module module;
    module.kind = kind;
    module.size = size;
    module.font.family = kPlainFamily;
    return module;
}

} // namespace

void Window::lift(int index, const Passage &passage)
{
    const Lifted lifted = pickUp(*doc, *fonts, {index, passage});
    if (!lifted.error.isEmpty()) {
        toast->pop(tr("Could not pick up text: %1").arg(lifted.error), Tone::Error);
        return;
    }
    renderer->forget(lifted.module.page);
    view->forgetText(lifted.module.page);
    const int id = modules->add(lifted.module);
    view->select(id);
    view->editText(id);
}

void Window::addText()
{
    Module module = plainModule(ModuleKind::Text, kTextSize);
    module.width = kTextWidth;
    view->armPlacement(module);
    toast->pop(tr("Click where the text goes"), Tone::Info);
}

void Window::addDate()
{
    Module module = plainModule(ModuleKind::Date, kDateSize);
    module.date = QDate::currentDate();
    module.text = QLocale().toString(module.date, QLocale::ShortFormat);
    view->armPlacement(module);
    toast->pop(tr("Click where the date goes"), Tone::Info);
}

void Window::recognizePage()
{
    if (recognition.valid()) {
        toast->pop(tr("Still recognising the previous page"), Tone::Info);
        return;
    }
    const int index = view->currentPage();
    const int page = doc->slot(index).id;
    const unsigned run = generation;
    toast->pop(tr("Recognising text on page %1 …").arg(index + 1), Tone::Info);
    recognition = std::async(std::launch::async, [this, index, page, run, target = doc.get()] {
        OcrResult result = recognize(*target, index);
        QMetaObject::invokeMethod(this, [this, page, run, result = std::move(result)] {
            recognition.wait();
            recognition = {};
            if (run == generation)
                applyRecognized(page, result);
        });
    });
}

void Window::applyRecognized(int page, const OcrResult &result)
{
    if (!result.error.isEmpty()) {
        toast->pop(tr("Text recognition failed: %1").arg(result.error), Tone::Error);
        return;
    }
    for (const OcrBlock &block : result.blocks) {
        Module module = plainModule(ModuleKind::Text, block.size);
        module.page = page;
        module.anchor = block.baseline;
        module.angle = block.angle;
        module.width = block.leading > 0 ? (block.box.right() - block.baseline.x()) * kWrapSlack : 0;
        module.leading = block.leading;
        module.text = block.text;
        module.color = QColor::fromRgb(block.ink);
        module.cover = block.box;
        module.coverColor = QColor::fromRgb(block.paper);
        modules->add(module);
    }
    setMode(Mode::Edit);
    toast->pop(tr("Found %n paragraph(s) — click one to change it", nullptr, int(result.blocks.size())), Tone::Info);
}
