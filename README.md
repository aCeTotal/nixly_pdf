# Nixly PDF

Fast PDF reader, editor and signer built on Qt 6 and MuPDF. Opens images, Office files, e-books and more as PDF.

- **Read** – smooth continuous scrolling, tiled multi-threaded rendering, a page strip you scroll through with the wheel.
- **Open anything** – drop or open PDFs, images, SVG, EPUB, XPS, text, and Word, Excel, PowerPoint or LibreOffice files; several at once become one document.
- **Edit** – click a paragraph, table cell, boxed block or picture to lift it off the page and move it; lifted text keeps the document's own fonts and colours and can be restyled, rewrapped or deleted. Add text and dates; draw arrows, callouts, rectangles, circles and clouds and place images, which stay editable PDF annotations in any reader. Recognise text on scanned pages; add, insert, drag to reorder and delete pages.
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
src/convert other file types to PDF
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
