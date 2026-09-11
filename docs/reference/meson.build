if get_option('gtk-doc')
  libxfce4session_client_doc_name = 'libxfce4session-client'

  conf_data = configuration_data()
  conf_data.set_quoted('PACKAGE_NAME', libxfce4session_client_doc_name)
  conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
  conf_data.set_quoted('PACKAGE_STRING', '@0@-@1@'.format(libxfce4session_client_doc_name, meson.project_version()))

  configure_file(
    input: 'gtkdocentities.ent.in',
    output: 'gtkdocentities.ent',
    configuration: conf_data,
  )

  ignored_headers = [
    'libwayland-shim.h',
    'libxfce4session-client-marshal.h',
    'libxfce4session-client-visibility.h',
    'stolen-from-libwayland.h',
    'xdg-session-management-v1-client.h',
    'xdg-shell-client.h',
    'xfce-session-client-dbus.h',
    'xfce-session-client-private.h',
    'xfce-session-client-wl-xdg.h',
    'xfce-session-client-xsmp.h',
    'xfsm-client-dbus-client.h',
    'xfsm-manager-dbus-client.h',
  ]

  configure_file(
    configuration: configuration_data({
      'COPYRIGHT_YEAR': copyright_year,
    }),
    input: 'libxfce4session-client-docs.xml.in',
    output: 'libxfce4session-client-docs.xml',
    install: false,
  )

  gnome.gtkdoc(
    libxfce4session_client_doc_name,
    src_dir: [
      'libxfce4session-client',
    ],
    ignore_headers: ignored_headers,
    main_xml: meson.project_build_root() / 'docs' / 'reference' / 'libxfce4session-client-docs.xml',
    dependencies: [
      libxfce4session_client_dep,
    ],
    gobject_typesfile: 'libxfce4session-client.types',
    install: true,
    check: false,
    scan_args: '--rebuild-types',
  )
endif
