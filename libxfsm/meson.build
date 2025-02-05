libxfsm = static_library(
  'xfsm',
  [
    'xfsm-shutdown-common.h',
    'xfsm-util.c',
    'xfsm-util.h',
  ],
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('libxfsm'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    libxfce4ui,
    libxfce4util,
    xfconf,
  ],
  install: false,
)
