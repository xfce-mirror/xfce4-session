session_sources = [
  'main.c',
  'xfsm-chooser.c',
  'xfsm-chooser.h',
  'xfsm-client.c',
  'xfsm-client.h',
  'xfsm-compat-gnome.c',
  'xfsm-compat-gnome.h',
  'xfsm-compat-kde.c',
  'xfsm-compat-kde.h',
  'xfsm-dns.c',
  'xfsm-dns.h',
  'xfsm-error.c',
  'xfsm-error.h',
  'xfsm-global.c',
  'xfsm-global.h',
  'xfsm-inhibition.c',
  'xfsm-inhibition.h',
  'xfsm-inhibitor.c',
  'xfsm-inhibitor.h',
  'xfsm-logout-dialog.c',
  'xfsm-logout-dialog.h',
  'xfsm-manager.c',
  'xfsm-manager.h',
  'xfsm-packagekit.c',
  'xfsm-packagekit.h',
  'xfsm-properties.c',
  'xfsm-properties.h',
  'xfsm-shutdown-fallback.c',
  'xfsm-shutdown-fallback.h',
  'xfsm-shutdown.c',
  'xfsm-shutdown.h',
  'xfsm-startup.c',
  'xfsm-startup.h',
]

c_args = [
  '-DG_LOG_DOMAIN="@0@"'.format('xfce4-session'),
  '-DWNCK_I_KNOW_THIS_IS_UNSTABLE=1',
  '-DXFSM_SHUTDOWN_HELPER_CMD="@0@"'.format(helper_path_prefix / 'xfce4' / 'session' / 'xfsm-shutdown-helper'),
]

if enable_x11
  c_args += [
    '-DICEAUTH_CMD="@0@"'.format(iceauth.full_path()),
  ]

  session_sources += [
    'ice-layer.c',
    'ice-layer.h',
    'sm-layer.c',
    'sm-layer.h',
    'xfsm-fadeout.c',
    'xfsm-fadeout.h',
  ]

  if get_option('legacy-session-management')
    session_sources += [
      'xfsm-legacy.c',
      'xfsm-legacy.h',
    ]
  endif
endif

session_sources += gnome.genmarshal(
  'xfsm-marshal',
  sources: 'xfsm-marshal.list',
  prefix: 'xfsm_marshal',
  internal: true,
  install_header: false,
)

session_sources += gnome.gdbus_codegen(
  'xfsm-client-dbus',
  sources: 'xfsm-client-dbus.xml',
  interface_prefix: 'org.xfce.Session',
  namespace: 'XfsmDbus',
  install_header: false,
)
session_sources += gnome.gdbus_codegen(
  'xfsm-manager-dbus',
  sources: 'xfsm-manager-dbus.xml',
  interface_prefix: 'org.xfce.Session',
  namespace: 'XfsmDbus',
  install_header: false,
)

session_sources += custom_target(
  'xfsm-chooser-icon.h',
  input: 'xfsm-chooser-icon.png',
  output: 'xfsm-chooser-icon.h',
  command: [gdk_pixbuf_csource, '--static', '--raw', '--stream', '--name=xfsm_chooser_icon_data', '@INPUT@'],
  capture: true,
)

executable(
  'xfce4-session',
  session_sources,
  sources: xfce_revision_h,
  c_args: c_args,
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gio,
    gio_unix,
    gtk,
    gmodule,
    libxfce4ui,
    libxfce4util,
    libxfce4windowing,
    xfconf,
    x11_deps,
    gtk_layer_shell,
    polkit,
    libm,
  ],
  link_with: [
    libxfsm,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

if polkit.found()
  policy_in = configure_file(
    configuration: configuration_data({
      'HELPER_PATH_PREFIX': helper_path_prefix,
    }),
    input: 'org.xfce.session.policy.in.in',
    output: 'org.xfce.session.policy.in',
    install: false,
  ) 

  i18n.merge_file(
    input: policy_in,
    output: 'org.xfce.session.policy',
    po_dir: '..' / 'po',
    type: 'xml',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'polkit-1' / 'actions',
  )
endif

install_data(
  'xfce-portals.conf',
  install_dir: get_option('prefix') / get_option('datadir') / 'xdg-desktop-portal',
)

install_man('xfce4-session.1')
