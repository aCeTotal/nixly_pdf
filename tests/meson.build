fixture = files('fixture.cpp')

foreach name : ['core', 'grouping', 'editing', 'marks', 'convert']
  test(name,
    executable(name + '_test',
      files(name + '_test.cpp'),
      fixture,
      link_with: core_lib,
      include_directories: src_dir,
      dependencies: core_deps,
    ),
    env: ['QT_QPA_PLATFORM=offscreen'],
    timeout: 300,
  )
endforeach
