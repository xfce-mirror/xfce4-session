[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/xfce/xfce4-session/-/blob/master/COPYING)

# xfce4-session


Xfce4-session is a session manager for Xfce. Its task is to save the state of your desktop (opened applications and their location) and restore it during a next startup. You can create several different sessions and choose one of them on startup. 

----

### Homepage

[Xfce4-session documentation](https://docs.xfce.org/xfce/xfce4-session/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/xfce/xfce4-session/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Xfce4-session source code](https://gitlab.xfce.org/xfce/xfce4-session)

### Download a Release Tarball

[Xfce4-session archive](https://archive.xfce.org/src/xfce/xfce4-session)
    or
[Xfce4-session tags](https://gitlab.xfce.org/xfce/xfce4-session/-/tags)

### Installation

From source: 

    % cd xfce4-session
    % meson setup build
    % meson compile -C build
    % meson install -C build

From release tarball:

    % tar xf xfce4-session-<version>.tar.xz
    % cd xfce4-session-<version>
    % meson setup build
    % meson compile -C build
    % meson install -C build

### Uninstallation

    % ninja uninstall -C build

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/xfce/xfce4-session/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

