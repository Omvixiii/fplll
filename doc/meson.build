doxygen = find_program('doxygen', required: false)
if doxygen.found()
  doxygen_configuration = configuration_data()
  doxygen_configuration.set('OUTPUT_DIRECTORY', meson.current_build_dir())
  doxygen_configuration.set('PROJECT_NUMBER', meson.project_version())
  doxygen_configuration.set('SOURCE_ROOT', meson.project_source_root())
  doxygen_configuration.set('BUILD_ROOT', meson.project_build_root())
  doxyfile = configure_file(
    input: 'Doxyfile.in',
    output: 'Doxyfile',
    configuration: doxygen_configuration,
  )
  run_target('doc', command: [doxygen, doxyfile])
else
  message('Doxygen not found; the doc target is unavailable')
endif
