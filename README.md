# Nixly PDF

Fast PDF reader, editor and signer built on Qt 6 and MuPDF.

- **Read** – smooth continuous scrolling, tiled multi-threaded rendering, click or drag through the page strip.
- **Edit** – click any text to rewrite it in the document's own font, add blank pages, insert other PDFs, reorder pages by dragging, delete pages.
- **Sign** – write your name, pick one of ten handwriting fonts and an ink, add a date or extra lines, then click to place it.

## Run

```sh
nix run                     # build and start
nix run . -- file.pdf       # open a file directly
```

## Develop

```sh
nix develop
meson setup build -Dsignature_fonts=$SIGNATURE_FONTS -Dfallback_fonts=$FALLBACK_FONTS
ninja -C build && build/src/nixly-pdf
meson test -C build
```

## Layout

```
src/app     window, top bar, theme, toasts
src/pdf     MuPDF document, rendering, text rewriting, stamping
src/view    page view, page strip, smooth scrolling
src/edit    text run editing and page tools
src/sign    signature dialog, fonts and layout
tests       document editing tests
data        desktop entry and icon
```
