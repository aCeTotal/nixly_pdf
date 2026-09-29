qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'], include_type: 'system')
mupdf_dep = dependency('mupdf', include_type: 'system')
fontconfig_dep = dependency('fontconfig')

signature_fonts = get_option('signature_fonts')
fallback_fonts = get_option('fallback_fonts')
if signature_fonts == '' or fallback_fonts == ''
  error('Set -Dsignature_fonts and -Dfallback_fonts (nix develop exports SIGNATURE_FONTS and FALLBACK_FONTS).')
endif

moc = qt6.compile_moc(headers: files(
  'edit/editbar.h',
  'edit/runeditor.h',
  'app/topbar.h',
  'pdf/renderer.h',
  'view/pageview.h',
  'view/thumbstrip.h',
))

src_dir = include_directories('.')
core_deps = [qt6_dep, mupdf_dep, fontconfig_dep]

core_lib = static_library('nixly-pdf-core',
  files(
    'app/pageactions.cpp',
    'app/theme.cpp',
    'app/toast.cpp',
    'app/topbar.cpp',
    'app/window.cpp',
    'edit/editbar.cpp',
    'edit/runcache.cpp',
    'edit/runeditor.cpp',
    'pdf/content.cpp',
    'pdf/context.cpp',
    'pdf/document.cpp',
    'pdf/fontname.cpp',
    'pdf/fontreuse.cpp',
    'pdf/pagetree.cpp',
    'pdf/renderer.cpp',
    'pdf/stamp.cpp',
    'pdf/systemfont.cpp',
    'pdf/textedit.cpp',
    'pdf/textrun.cpp',
    'sign/scriptfonts.cpp',
    'sign/signaturelayout.cpp',
    'sign/signaturepreview.cpp',
    'sign/signdialog.cpp',
    'view/overlay.cpp',
    'view/pageinput.cpp',
    'view/pagelayout.cpp',
    'view/pagepainter.cpp',
    'view/pageview.cpp',
    'view/smoothvalue.cpp',
    'view/thumbpaint.cpp',
    'view/thumbstrip.cpp',
    'view/ticker.cpp',
  ),
  moc,
  cpp_args: [
    '-DSIGNATURE_FONT_DIR="' + signature_fonts + '"',
    '-DFALLBACK_FONT_DIR="' + fallback_fonts + '"',
  ],
  dependencies: core_deps,
)

executable('nixly-pdf',
  files('main.cpp'),
  link_with: core_lib,
  dependencies: core_deps,
  install: true,
)
