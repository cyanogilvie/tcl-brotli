project('brotli', 'c',
  version:          '0.4.4',
  default_options:  ['c_std=gnu2y,gnu23,gnu2x', 'warning_level=3', 'werror=true'],
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
  'brotli.c',
)

inc += include_directories('.')

tea_want_shims = []
tea_want_shims += 'Tcl_GetBytesFromObj'
tea_want_defer = true

tea_genstubs = files('brotli.decls', 'brotliStubLib.c')

# --- Teabase init (must come first) ---
# Provides: pkg_dir, tclsh, tcl_dep, teaconf
subdir('teabase/init')

# --- External dependencies (brotli library, prefer static) ---
deps += dependency('libbrotlienc', static: true)
deps += dependency('libbrotlidec', static: true)
#deps += dependency('libbrotlicommon', static: true)

# --- Teabase main (builds shared_module, sets up tests) ---
subdir('teabase')

# --- Configure the public header from .h.in ---
pkg_sources += configure_file(
  configuration:  teaconf,
  input:          'brotli.h.in',
  output:         '@BASENAME@',
  install_dir:    pkg_dir,
)

# --- pkg-config file for downstream meson / shell consumers ---
pc_conf = configuration_data()
pc_conf.set('PACKAGE_NAME',     meson.project_name())
pc_conf.set('PACKAGE_VERSION',  meson.project_version())
pc_conf.set('PKG_DIR_BASENAME', meson.project_name() + '-' + meson.project_version())
configure_file(
  input:          meson.project_name() + '.pc.in',
  output:         meson.project_name() + '.pc',
  configuration:  pc_conf,
  install:        true,
  install_dir:    tcl_dep.get_variable('libdir') / 'pkgconfig',
  install_tag:    meson.is_subproject() ? 'deps' : 'runtime',
)

# Override-dependency for meson subproject consumers. See parse_args.
brotli_dep = declare_dependency(
  link_with:            stubs_lib,
  sources:              genstubs[1],
  include_directories:  inc,
  compile_args:         '-DUSE_BROTLI_STUBS=1',
  variables: {
    'tcl_pkg_path': fs.parent(pkg_so.full_path()),
    'version':      meson.project_version(),
  },
)
meson.override_dependency(meson.project_name(), brotli_dep)
