# Nixly PDF

Fast PDF reader, editor and signer built on Qt 6 and MuPDF.

- **Read** – smooth continuous scrolling, tiled multi-threaded rendering, a page strip you scroll through with the wheel.
- **Edit** – click a paragraph, table cell or boxed block to lift it into one movable text box that keeps the document's own fonts and colours; restyle, rewrap, move or delete it. Add text and dates; draw arrows, callouts, rectangles, circles and clouds and place images, which stay editable PDF annotations in any reader. Recognise text on scanned pages; add, insert, drag to reorder and delete pages.
- **Sign** – write your name, pick one of eighteen handwriting fonts and an ink, then place and move it freely.
- **Protect** – save with a passphrase; AES-256 encryption that Acrobat and other readers open.

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
src/pdf     MuPDF document, rendering, page content rewriting
src/font    embedded and installed fonts
src/ocr     text recognition for scanned pages
src/view    page view, page strip, smooth scrolling
src/module  movable text, signatures and dates; lifting, typesetting, burning in
src/mark    shapes, callouts and images saved as annotations
src/edit    page tools, paragraph, panel and table grouping
src/sign    signature dialog, fonts and layout
tests       document editing tests
data        desktop entry and icon
```
