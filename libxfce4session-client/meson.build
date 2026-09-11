libxfce4session_client_version_api = '1'

libxfce4session_client_libname = 'libxfce4session-client'
libxfce4session_client_pkgname = '@0@-@1@'.format(libxfce4session_client_libname, libxfce4session_client_version_api)

libxfce4session_client_install_subdir = 'xfce4' / libxfce4session_client_pkgname / libxfce4session_client_libname
libxfce4session_client_install_dir = get_option('prefix') / get_option('includedir') / libxfce4session_client_install_subdir

libxfce4session_client_enum_headers = [
  'libxfce4session-client-enums.h',
]

libxfce4session_client_headers = libxfce4session_client_enum_headers + [
  'libxfce4session-client.h',
  'xfce-session-client.h',
]

# do not add this header to GI sources, it creates unsolvable conflicts with other
# libs whose identifier prefix is Xfce, e.g. libxfce4util and libxfce4panel
libxfce4session_client_config_h = configure_file(
  configuration: configuration_data({
    'LIBXFCE4SESSION_CLIENT_VERSION_MAJOR': version_parts[0],
    'LIBXFCE4SESSION_CLIENT_VERSION_MINOR': version_parts[1],
    'LIBXFCE4SESSION_CLIENT_VERSION_MICRO': version_parts[2],
  }),
  input: 'libxfce4session-client-config.h.in',
  output: 'libxfce4session-client-config.h',
  install: true,
  install_dir: libxfce4session_client_install_dir,
)

libxfce4session_client_sources_gir = [
  'libxfce4session-client-config.c',
  'xfce-session-client.c',
]

libxfce4session_client_sources = libxfce4session_client_sources_gir + [
  'xfce-session-client-private.h',
  'xfce-session-client-dbus.c',
  'xfce-session-client-dbus.h',
]
if enable_x11
  libxfce4session_client_sources += [
    'xfce-session-client-xsmp.c',
    'xfce-session-client-xsmp.h',
  ]
endif
if enable_wayland
  libxfce4session_client_sources += [
    'libwayland-shim.c',
    'libwayland-shim.h',
    'stolen-from-libwayland.c',
    'stolen-from-libwayland.h',
    'xfce-session-client-wl-xdg.c',
    'xfce-session-client-wl-xdg.h',
  ]

  wl_protocols_pkgdatadir = wl_protocols.get_variable(pkgconfig: 'pkgdatadir')
  xdg_session_management_xml_dir = wl_protocols_pkgdatadir / 'staging' / 'xdg-session-management'
  if not fs.is_dir(xdg_session_management_xml_dir)
    xdg_session_management_xml_dir = meson.current_source_dir()
  endif

  protocols = {
    'xdg-session-management-v1': xdg_session_management_xml_dir / 'xdg-session-management-v1.xml',
    'xdg-shell': wl_protocols_pkgdatadir / 'stable' / 'xdg-shell' / 'xdg-shell.xml',
  }

  foreach name, file : protocols
    client_header = custom_target(
      '@0@-client-header'.format(name),
      input: file,
      output: '@0@-client.h'.format(name),
      command: [wayland_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@'],
    )

    private_code = custom_target(
      '@0@-private-code'.format(name),
      input: file,
      output: '@0@.c'.format(name),
      command: [wayland_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@'],
    )

    libxfce4session_client_sources += [client_header, private_code]
  endforeach
endif

libxfce4session_client_sources += gnome.genmarshal(
  'libxfce4session-client-marshal',
  sources: 'libxfce4session-client-marshal.list',
  prefix: '_libxfce4session_client_marshal',
  internal: true,
  install_header: false,
)

libxfce4session_client_sources += gnome.gdbus_codegen(
  'xfsm-client-dbus-client',
  sources: '..' / 'xfce4-session' / 'xfsm-client-dbus.xml',
  interface_prefix: 'org.xfce.Session',
  namespace: 'XfsmDbus',
  install_header: false,
)
libxfce4session_client_sources += gnome.gdbus_codegen(
  'xfsm-manager-dbus-client',
  sources: '..' / 'xfce4-session' / 'xfsm-manager-dbus.xml',
  interface_prefix: 'org.xfce.Session',
  namespace: 'XfsmDbus',
  install_header: false,
)

libxfce4session_client_enums = []
libxfce4session_client_enums += gnome.mkenums(
  'libxfce4session-client-enum-types.h',
  install_header: true,
  install_dir: libxfce4session_client_install_dir,
  sources: libxfce4session_client_enum_headers,
  fhead: '#if !defined(_LIBXFCE4SESSION_CLIENT_INSIDE_LIBXFCE4SESSION_CLIENT_H) && !defined(LIBXFCE4SESSION_CLIENT_COMPILATION)\n#error "Only <libxfce4session-client/libxfce4session-client.h> can be included directly, this file is not part of the public API."\n#endif\n\n#ifndef __LIBXFCE4SESSION_CLIENT_ENUM_TYPES_H__\n#define __LIBXFCE4SESSION_CLIENT_ENUM_TYPES_H__\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n\n',
  fprod: '/* enumerations from "@basename@" */\n\n',
  vhead: 'GType @enum_name@_get_type(void);\n#define XFCE_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n\n',
  ftail: 'G_END_DECLS\n\n#endif /* !__LIBXFCE4SESSION_CLIENT_ENUM_TYPES_H__ */',
)
libxfce4session_client_enums += gnome.mkenums(
  'libxfce4session-client-enum-types.c',
  install_header: false,
  sources: libxfce4session_client_enum_headers,
  fhead: '#include "libxfce4session-client-enums.h"\n#include "libxfce4session-client-enum-types.h"\n#include "libxfce4session-client-visibility.h"\n\n',
  fprod: '/* enumerations from "@basename@" */\n\n',
  vhead: 'GType\n@enum_name@_get_type(void)\n{\n    static GType type = 0;\n\n    if(!type) {\n        static const G@Type@Value values[] = {',
  vprod: '            { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '            { 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static("@EnumName@", values);\n    }\n\n    return type;\n}\n\n',
  ftail: '\n#define __LIBXFCE4SESSION_CLIENT_ENUM_TYPES_C__\n#include "libxfce4session-client-visibility.c"\n',
)

if enable_x11
  symbols_sed_script = '/@BEGIN_X11_SYMBOLS@/d; /@END_X11_SYMBOLS@/d;'
else
  symbols_sed_script = '/@BEGIN_X11_SYMBOLS@/,/@END_X11_SYMBOLS@/d;'
endif
if enable_wayland
  symbols_sed_script += '/@BEGIN_WAYLAND_SYMBOLS@/d; /@END_WAYLAND_SYMBOLS@/d'
else
  symbols_sed_script += '/@BEGIN_WAYLAND_SYMBOLS@/,/@END_WAYLAND_SYMBOLS@/d'
endif
symbols = configure_file(
  input: 'libxfce4session-client.symbols.in',
  output: 'libxfce4session-client.symbols',
  command: [sed, symbols_sed_script, '@INPUT@'],
  capture: true,
)

libxfce4session_client_sources += configure_file(
  input: symbols,
  output: 'libxfce4session-client-visibility.h',
  command: [xdt_gen_visibility, '--kind=header', '@INPUT@', '@OUTPUT@'],
)
libxfce4session_client_sources += configure_file(
  input: symbols,
  output: 'libxfce4session-client-visibility.c',
  command: [xdt_gen_visibility, '--kind=source', '@INPUT@', '@OUTPUT@'],
)

libxfce4session_client_deps = [
  gio_unix,
  gtk,
  libxfce4util,
  libm,
]
if need_libintl
  libxfce4session_client_deps += libintl
endif
if enable_x11
  libxfce4session_client_deps += [
    gdk_x11,
    libice,
    libsm,
  ]
endif
if enable_wayland
  libxfce4session_client_deps += wayland_deps
endif

libxfce4session_client_so_version = '0'
libxfce4session_client_version = '@0@.0.0'.format(libxfce4session_client_so_version)

libxfce4session_client = library(
  libxfce4session_client_pkgname.substring(3),
  sources: libxfce4session_client_sources + libxfce4session_client_enums,
  version: libxfce4session_client_version,
  soversion: libxfce4session_client_so_version,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format(libxfce4session_client_libname),
    '-DLIBXFCE4SESSION_CLIENT_COMPILATION=1',
  ],
  include_directories: [
    include_directories('..'),
  ],
  gnu_symbol_visibility: gnu_symbol_visibility,
  dependencies: libxfce4session_client_deps,
  install: true,
)

install_headers(
  libxfce4session_client_headers,
  subdir: libxfce4session_client_install_subdir,
)

pkgconfig.generate(
  libraries: [
    libxfce4session_client,
  ],
  requires: [
    gtk,
  ],
  version: meson.project_version(),
  name: libxfce4session_client_libname,
  filebase: libxfce4session_client_pkgname,
  description: 'Session management client library for Xfce',
  subdirs: [
    'xfce4' / libxfce4session_client_pkgname,
  ],
  install_dir: get_option('prefix') / get_option('libdir') / 'pkgconfig',
)

if get_option('introspection')
  libxfce4session_client_gir = gnome.generate_gir(
    libxfce4session_client,
    sources: libxfce4session_client_sources_gir + libxfce4session_client_headers + libxfce4session_client_enums,
    namespace: 'Libxfce4sessionclient',
    nsversion: '@0@.0'.format(libxfce4session_client_version_api),
    identifier_prefix: 'Xfce',
    symbol_prefix: [
      'xfce',
    ],
    export_packages: libxfce4session_client_pkgname,
    includes: [
      'GLib-2.0',
      'GObject-2.0',
      'Gio-2.0',
      'Gtk-3.0',
    ],
    header: 'libxfce4session-client' / 'libxfce4session-client.h',
    install: true,
    extra_args: [
      '-DLIBXFCE4SESSION_CLIENT_COMPILATION=1',
    ],
  )

  if generate_vala
    gnome.generate_vapi(
      libxfce4session_client_pkgname,
      install: true,
      packages: [
        'glib-2.0',
        'gobject-2.0',
        'gio-2.0',
        'gtk+-3.0',
      ],
      sources: libxfce4session_client_gir[0],
    )
  endif
endif

libxfce4session_client_dep = declare_dependency(
  dependencies: libxfce4session_client_deps,
  link_with: libxfce4session_client,
)
