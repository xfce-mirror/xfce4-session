project(
  'xfce4-session',
  'c',
version : '4.20.0-dev',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.57.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2']
)

project_namespace = 'xfce'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.72.0',
  'gdk-pixbuf': '>= 2.42.8',
  'gtk': '>= 3.24.0',
  'libxfce4ui': '>= 4.18.4',
  'libxfce4util': '>= 4.19.2',
  'libxfce4windowing': '>= 4.19.2',
  'xfconf': '>= 4.12.0',

  'libice': '>= 1.0.10',
  'libsm': '>= 1.2.3',
  'libwnck': '>= 3.10.0',
  'libx11': '>= 1.6.7',

  'gtk-layer-shell': '>= 0.7.0',

  'polkit': '>= 0.102',
}

glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio = dependency('gio-2.0', version: dependency_versions['glib'])
gio_unix = dependency('gio-unix-2.0', version: dependency_versions['glib'])
gmodule = dependency('gmodule-2.0', version: dependency_versions['glib'])
gdk_pixbuf = dependency('gdk-pixbuf-2.0', version: dependency_versions['gdk-pixbuf'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['libxfce4util'])
libxfce4windowing = dependency('libxfce4windowing-0', version: dependency_versions['libxfce4windowing'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfconf'])

xdt_csource = find_program('xdt-csource', required: true)
gdk_pixbuf_csource = find_program(gdk_pixbuf.get_variable(pkgconfig: 'gdk_pixbuf_csource'), required: true)
gzip = find_program('gzip', required: true)

# Feature: 'x11'
x11_deps = []
x11_deps += dependency('gdk-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
x11_deps += dependency('ice', version: dependency_versions['libice'], required: get_option('x11'))
libice = x11_deps[-1]
x11_deps += dependency('sm', version: dependency_versions['libsm'], required: get_option('x11'))
x11_deps += dependency('libwnck-3.0', version: dependency_versions['libwnck'], required: get_option('x11'))
x11_deps += dependency('x11', version: dependency_versions['libx11'], required: get_option('x11'))

# Feature: 'wayland'
wayland_deps = []
wayland_deps += dependency('gdk-wayland-3.0', version: dependency_versions['gtk'], required: get_option('wayland'))

feature_cflags = []

enable_x11 = not get_option('x11').disabled()
foreach dep : x11_deps
  enable_x11 = enable_x11 and dep.found()
endforeach
if enable_x11
  feature_cflags += '-DENABLE_X11=1'

  if cc.has_function('_IceTransNoListen', dependencies: libice)
    feature_cflags += '-DHAVE__ICETRANSNOLISTEN=1'
  endif

  iceauth = find_program('iceauth', required: true)

  xsession_prefix = get_option('xsession-prefix')
  if xsession_prefix == ''
    xsession_prefix = '/usr'
  endif

  if get_option('legacy-session-management')
    feature_cflags += '-DENABLE_LEGACY_SESSION_MANAGEMENT=1'
  endif
endif

enable_wayland = not get_option('wayland').disabled()
foreach dep : wayland_deps
  enable_wayland = enable_wayland and dep.found()
endforeach
if enable_wayland
  feature_cflags += '-DENABLE_WAYLAND=1'

  wayland_session_prefix = get_option('wayland-session-prefix')
  if wayland_session_prefix == ''
    wayland_session_prefix = '/usr'
  endif

  gtk_layer_shell = dependency('gtk-layer-shell-0', version: dependency_versions['gtk-layer-shell'], required: get_option('gtk-layer-shell'))
  if gtk_layer_shell.found()
    feature_cflags += '-DHAVE_GTK_LAYER_SHELL=1'
  endif
else
  gtk_layer_shell = dependency('', required: false)
endif

if not enable_x11 and not enable_wayland
  error('At least one of the X11 and Wayland backends must be enabled')
endif

polkit = dependency('polkit-gobject-1', version: dependency_versions['polkit'], required: get_option('polkit'))

helper_path_prefix = get_option('helper-path-prefix')
if helper_path_prefix == ''
  helper_path_prefix = get_option('prefix') / get_option('libdir')
endif

build_os = build_machine.system()
if build_os == 'cygwin'
  feature_cflags += '-DHAVE_OS_CYGWIN=1'
endif

host_os = get_option('host-os')
if host_os == ''
  host_os = host_machine.system()
endif
if host_os == 'linux'
  feature_cflags += '-DBACKEND_TYPE_LINUX=1'
elif host_os == 'freebsd'
  feature_cflags += '-DBACKEND_TYPE_FREEBSD=1'
elif host_os == 'openbsd'
  feature_cflags += '-DBACKEND_TYPE_OPENBSD=1'
elif host_os == 'solaris'
  feature_cflags += '-DBACKEND_TYPE_SOLARIS=1'
endif

functions = [
  'getaddrinfo',
  'gethostbyname',
  'gethostname',
  'setsid',
  'sigprocmask',
]
foreach function : functions
  if cc.has_function(function)
    feature_cflags += '-DHAVE_@0@=1'.format(function.to_upper())
  endif
endforeach

headers = [
  'sys/resource.h',
  'sys/socket.h',
  'sys/stat.h',
  'sys/sysctl.h',
  'sys/time.h',
  'sys/types.h',
  'sys/utsname.h',
  'sys/wait.h',
  'errno.h',
  'fcntl.h',
  'math.h',
  'memory.h',
  'netdb.h',
  'stdarg.h',
  'stdlib.h',
  'stdio.h',
  'signal.h',
  'string.h',
  'time.h',
  'unistd.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

libm = cc.find_library('m', required: true)

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

if enable_x11
  i18n.merge_file(
    input: 'xfce.desktop.in',
    output: 'xfce.desktop',
    po_dir: 'po',
    type: 'desktop',
    install: true,
    install_dir: xsession_prefix / get_option('datadir') / 'xsessions',
  )
endif

if enable_wayland
  i18n.merge_file(
    input: 'xfce-wayland.desktop.in',
    output: 'xfce-wayland.desktop',
    po_dir: 'po',
    type: 'desktop',
    install: true,
    install_dir: wayland_session_prefix / get_option('datadir') / 'wayland-sessions',
  )
endif

subdir('icons')
subdir('labwc')
subdir('libxfsm')
subdir('po')
subdir('scripts')
subdir('settings')
subdir('xfce4-session')
subdir('xfce4-session-logout')
subdir('xfsm-shutdown-helper')

gnome.post_install(
  gtk_update_icon_cache: true,
)
