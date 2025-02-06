sizes = [16, 24, 32, 48, 64, 96, 128]
names = [
  'org.xfce.session'
]
action_sizes = [24, 48, 96]
action_names = [
  'xfsm-hibernate',
  'xfsm-lock',
  'xfsm-logout',
  'xfsm-reboot',
  'xfsm-shutdown',
  'xfsm-suspend',
  'xfsm-switch-user',
]

foreach size : sizes
  foreach name : names
    install_data(
      '@0@x@0@'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
    )
  endforeach
endforeach

foreach name : names
  install_data(
    'scalable' / '@0@.svg'.format(name),
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  )
endforeach

foreach size : action_sizes
  foreach name : action_names
    install_data(
      '@0@x@0@-actions'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'actions',
    )
  endforeach
endforeach
