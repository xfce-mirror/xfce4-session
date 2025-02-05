executable(
  'xfsm-shutdown-helper',
  [
    'main.c',
  ],
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfsm-shutdown-helper'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gtk,
    libxfce4util,
  ],
  install: true,
  install_dir: helper_path_prefix / 'xfce4' / 'session',
)
