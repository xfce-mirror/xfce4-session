Summary: 	Session manager
Name: 		xfce4-session
Version: 	0.0.23
Release: 	1
License:	BSD
URL: 		http://www.xfce.org/
Source0: 	%{name}-%{version}.tar.gz
Group: 		User Interface/Desktops
BuildRoot: 	%{_tmppath}/%{name}-root
Requires:	libxfcegui4 >= 3.92.2
Requires:	libxfce4mcs >= 3.91.0
Requires:	xfce-mcs-manager >= 3.91.1
BuildRequires: 	libxfcegui4-devel >= 3.92.2
BuildRequires:	libxfce4mcs-devel >= 3.91.0
BuildRequires:	xfce-mcs-manager-devel >= 3.91.1

%description
xfce4-session is the session manager for the XFce desktop environment

%prep
%setup -q

%build
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT mandir=%{_mandir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc AUTHORS COPYING ChangeLog ChangeLog.pre-xfce-devel NEWS README TODO
%{_sysconfdir}/xfce4/
%{_datadir}/xfce4/splash/
%{_datadir}/locale/
%{_bindir}/*
%{_sbindir}/*
%{_datadir}/xfce4/themes/
%{_libdir}/xfce4/mcs-plugins/

