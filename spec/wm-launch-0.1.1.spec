Name: wm-launch
Version: 0.1.1
Release: 1%{?dist}
Summary: Tool to launch X11 clients with unique IDs

License: LGPL
URL: https://github.com/jcrd/wm-launch
Source0: https://github.com/jcrd/wm-launch/archive/v0.1.1.tar.gz

Requires: bash

BuildRequires: gcc
BuildRequires: libX11-devel
BuildRequires: libxcb-devel
BuildRequires: perl

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
/usr/bin/%{name}
/usr/lib/wm-launch/%{name}-preload.so
/usr/share/man/man1/%{name}.1.gz

%changelog
* Mon May 11 2020 James Reed <jcrd@tuta.io> - 0.1.1
- Initial package
