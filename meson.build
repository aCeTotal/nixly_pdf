project('nixly-pdf', 'cpp',
  version: '0.1',
  default_options: ['cpp_std=c++20', 'warning_level=3', 'buildtype=release', 'b_ndebug=if-release']
)

install_data('data/nixly-pdf.desktop',
  install_dir: get_option('datadir') / 'applications'
)
install_data('data/nixly-pdf.svg',
  install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps'
)

subdir('src')

subdir('tests')
