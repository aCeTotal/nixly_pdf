test('core',
  executable('core_test',
    files('core_test.cpp'),
    link_with: core_lib,
    include_directories: src_dir,
    dependencies: core_deps,
  ),
  env: ['QT_QPA_PLATFORM=offscreen'],
)
