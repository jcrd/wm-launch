Name: {{{ git_cwd_name name="wm-launch" }}}
Version: {{{ git_cwd_version lead="$(git tag | sed -n 's/^v//p' | sort --version-sort -r | head -n1)" }}}
Release: 1%{?dist}
Summary: Tool to launch X11 clients with unique IDs

License: GPLv3+
URL: https://github.com/jcrd/wm-launch
VCS: {{{ git_cwd_vcs }}}
Source0: {{{ git_cwd_pack }}}

Requires: bash
Requires: coreutils
Requires: glib2

BuildRequires: gcc
BuildRequires: perl
BuildRequires: go
BuildRequires: pkgconfig(x11)
BuildRequires: pkgconfig(xcb)

%global debug_package %{nil}

%description
wm-launch provides a shared library for use with LD_PRELOAD and a command-line tool to set IDs on newly created X11 windows.

%prep
{{{ git_cwd_setup_macro }}}

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
{{{ git_cwd_changelog }}}
