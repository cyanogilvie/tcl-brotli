project('brotli', 'c',
  version:          '0.4',
  default_options:  ['c_std=c11', 'warning_level=3', 'werror=true'],
  meson_version:    '>=1.3.0',
)

cc = meson.get_compiler('c')
fs = import('fs')
add_project_arguments('-D_DEFAULT_SOURCE', language: 'c')

if get_option('debug')
  add_project_arguments(cc.get_supported_arguments(['-ggdb3']), language: 'c')
endif

# Suppress warnings that TEA-style code commonly triggers
add_project_arguments('-Wno-missing-field-initializers', language: 'c')

# --- Configuration data (becomes config.h) ---
conf = configuration_data()
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

# --- Accumulator variables for teabase ---
deps = []
inc = []

# Package sources
pkg_sources = files(
  'generic/brotli.c',
)

# --- Teabase init (must come first) ---
# Provides: pkg_dir, tclsh, tcl_dep, teaconf
subdir('teabase/init')

# --- Stubs generation via custom_target ---
deps += declare_dependency(
  sources: custom_target('genstubs',
    output: ['brotliStubInit.c', 'brotliDecls.h'],
    command: [
      tclsh,
      meson.project_source_root() / 'teabase/tools/genStubs.tcl',
      meson.project_build_root(),
      meson.project_source_root() / 'generic/brotli.decls',
    ],
  ),
)

# --- External dependencies (brotli library, prefer static) ---
deps += dependency('libbrotlienc', static: true)
deps += dependency('libbrotlidec', static: true)
#deps += dependency('libbrotlicommon', static: true)

# --- Teabase main (builds shared_module, sets up tests) ---
subdir('teabase')

# --- Configure the public header from .h.in ---
pkg_sources += configure_file(
  configuration:  teaconf,
  input:          'generic/brotli.h.in',
  output:         '@BASENAME@',
  install_dir:    pkg_dir,
)
