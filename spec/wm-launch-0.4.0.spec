Name: wm-launch
Version: 0.4.0
Release: 1%{?dist}
Summary: Tool to launch X11 clients with unique IDs

License: LGPL
URL: https://github.com/jcrd/wm-launch
Source0: https://github.com/jcrd/wm-launch/archive/v0.4.0.tar.gz

Requires: bash

BuildRequires: gcc
BuildRequires: perl
BuildRequires: go
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(xcb)

%global debug_package %{nil}

%description
wm-launch provides a shared library for use with LD_PRELOAD and a command-line tool to set IDs on newly created X11 windows.

%prep
%setup

%build
%make_build PREFIX=/usr

%install
%make_install PREFIX=/usr

%files
%license LICENSE
%doc README.md
%{_bindir}/%{name}
%{_bindir}/%{name}d
/usr/lib/wm-launch
/usr/lib/systemd/user/%{name}d.service
%{_mandir}/man1/%{name}.1.gz

%changelog
* Sun Nov 1 2020 James Reed <jcrd@tuta.io> - 0.4.0-1
- Release v0.4.0

* Wed Sep 16 2020 James Reed <jcrd@tuta.io> - 0.3.0-1
- Release v0.3.0

* Wed May 27 2020 James Reed <jcrd@tuta.io> - 0.2.0-1
- Release v0.2.0
- Build now requires go

* Wed May 27 2020 James Reed <jcrd@tuta.io> - 0.1.1-3
- Use rpm macros in files section

* Sun May 24 2020 James Reed <jcrd@tuta.io> - 0.1.1-2
- Use pkgconfig in build requires

* Mon May 11 2020 James Reed <jcrd@tuta.io> - 0.1.1
- Initial package
