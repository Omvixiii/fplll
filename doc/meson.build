# SPDX-FileCopyrightText: 2026 Omvixiii
# SPDX-License-Identifier: LGPL-2.1-or-later

# A missing Doxygen leaves the doc target out instead of failing configuration.
doxygen = find_program('doxygen', required: false)

if doxygen.found()
    doxyfile = configure_file(
        input: 'Doxyfile.in',
        output: 'Doxyfile',
        configuration: {
            'OUTPUT_DIRECTORY': meson.current_build_dir(),
            'PROJECT_NAME': project_name,
            'PROJECT_NUMBER': meson.project_version(),
            'SOURCE_ROOT': meson.project_source_root(),
            'BUILD_ROOT': meson.project_build_root(),
        },
    )

    run_target('doc', command: [doxygen, doxyfile])
else
    message('Doxygen not found: the doc target is not available')
endif
