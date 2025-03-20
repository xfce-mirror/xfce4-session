settings_sources = [
  'main.c',
  'session-editor.c',
  'xfae-dialog.c',
  'xfae-dialog.h',
  'xfae-model.c',
  'xfae-model.h',
  'xfae-window.c',
  'xfae-window.h',
  'xfce4-session-settings-common.h',
]

settings_sources += gnome.genmarshal(
  'xfce4-session-marshal',
  sources: 'xfce4-session-marshal.list',
  prefix: 'xfce4_session_marshal',
  internal: true,
  install_header: false,
)

settings_sources += gnome.gdbus_codegen(
  'xfsm-client-dbus-client',
  sources: '..' / 'xfce4-session' / 'xfsm-client-dbus.xml',
  interface_prefix: 'org.xfce.Session',
  namespace: 'Xfsm',
  install_header: false,
)
settings_sources += gnome.gdbus_codegen(
  'xfsm-manager-dbus-client',
  sources: '..' / 'xfce4-session' / 'xfsm-manager-dbus.xml',
  interface_prefix: 'org.xfce.Session',
  namespace: 'Xfsm',
  install_header: false,
)

settings_sources += gnome.compile_resources(
  'xfce4-session-resources',
  'xfce4-session.gresource.xml',
)

executable(
  'xfce4-session-settings',
  settings_sources,
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-session-settings'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gio,
    gmodule,
    gtk,
    libxfce4ui,
    libxfce4util,
    xfconf,
  ],
  link_with: [
    libxfsm,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'xfce-session-settings.desktop.in',
  output: 'xfce-session-settings.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)

install_data(
  'xfce4-session.xml',
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'xfce4' / 'xfconf' / 'xfce-perchannel-xml',
)
