configure_file(
  configuration: configuration_data({
    '_datadir_': get_option('prefix') / get_option('datadir'),
    '_sysconfdir_': get_option('prefix') / get_option('sysconfdir'),
  }),
  input: 'startxfce4.in',
  output: 'startxfce4',
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
  install_mode: 'rwxr-xr-x',
) 

configure_file(
  configuration: configuration_data({
    '_sysconfdir_': get_option('prefix') / get_option('sysconfdir'),
  }),
  input: 'xinitrc.in',
  output: 'xinitrc',
  install: true,
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'xfce4',
) 

install_data(
  'xflock4',
  install_dir: get_option('prefix') / get_option('bindir'),
  install_mode: 'rwxr-xr-x',
)

install_data(
  'Xft.xrdb',
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'xfce4',
)
