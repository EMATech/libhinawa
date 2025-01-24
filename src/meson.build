# Depends on glib-2.0 and gobject-2.0
gobject = dependency('gobject-2.0',
  version: '>=2.44.0'
)
dependencies = [
  gobject,
]

sources = [
  'fw_node.c',
  'fw_resp.c',
  'fw_req.c',
  'fw_fcp.c',
  'snd_unit.c',
  'snd_dice.c',
  'snd_efw.c',
  'snd_dg00x.c',
  'snd_motu.c',
  'snd_motu_register_dsp_parameter.c',
  'snd_tscm.c',
]

headers = [
  'hinawa.h',
  'fw_node.h',
  'fw_resp.h',
  'fw_req.h',
  'fw_fcp.h',
  'snd_unit.h',
  'snd_dice.h',
  'snd_efw.h',
  'snd_dg00x.h',
  'snd_motu.h',
  'snd_motu_register_dsp_parameter.h',
  'snd_tscm.h',
  'hinawa_enum_types.h',
]

privates = [
  'internal.h',
]

inc_dir = meson.project_name()

# Generate marshallers for GObject signals.
marshallers = gnome.genmarshal('hinawa_sigs_marshal',
  prefix: 'hinawa_sigs_marshal',
  sources: 'hinawa_sigs_marshal.list',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), inc_dir),
  stdinc: true,
)

enums = gnome.mkenums_simple('hinawa_enums',
  sources: ['hinawa_enum_types.h'],
  identifier_prefix: 'Hinawa',
  symbol_prefix: 'hinawa',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), inc_dir),
  header_prefix: '#include <hinawa.h>',
)

mapfile = 'hinawa.map'
vflag = '-Wl,--version-script,' + join_paths(meson.current_source_dir(), mapfile)

myself = library('hinawa',
  sources: sources + headers + privates + marshallers + enums,
  version: meson.project_version(),
  soversion: meson.project_version().split('.')[0],
  install: true,
  include_directories: backport,
  dependencies: dependencies,
  link_args : vflag,
  link_depends : mapfile,
)

install_headers(headers,
  subdir: inc_dir,
)

pkg = import('pkgconfig')
pkg.generate(myself,
  description: 'Input/Output library to audio and music units on IEEE 1394 bus',
  subdirs: inc_dir,
)

hinawa_gir = gnome.generate_gir(myself,
  sources: enums + headers + sources,
  nsversion: '3.0',
  namespace: 'Hinawa',
  symbol_prefix: 'hinawa_',
  identifier_prefix: 'Hinawa',
  export_packages: 'hinawa',
  dependencies: dependencies,
  includes: [
    'GLib-2.0',
    'GObject-2.0',
  ],
  header: 'hinawa.h',
  install: true,
)

# For test.
builddir = meson.current_build_dir()
