executable(
  'xfce4-session-logout',
  [
    'main.c',
  ],
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-session-logout'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gio,
    gtk,
    libxfce4ui,
    libxfce4util,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'xfce4-session-logout.desktop.in',
  output: 'xfce4-session-logout.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)

install_man('xfce4-session-logout.1')
