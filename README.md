# wm-launch [![CircleCI](https://circleci.com/gh/jcrd/wm-launch.svg?style=svg)](https://circleci.com/gh/jcrd/wm-launch)

**wm-launch** provides a shared library for use with `LD_PRELOAD` and a
command-line tool to set IDs on newly created X11 windows. It is intended to be
used by a window manager to uniquely identify clients it launches.

## Packages

* **RPM** package available from [copr][1]. [![Copr build status](https://copr.fedorainfracloud.org/coprs/jcrd/wm-launch/package/wm-launch/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/jcrd/wm-launch/package/wm-launch/)

  Install with:
  ```
  dnf copr enable jcrd/wm-launch
  dnf install wm-launch
  ```

## Usage

`LD_PRELOAD` can be specified along with `WM_LAUNCH_ID` in the
environment of a command:
```
LD_PRELOAD=/usr/lib/wm-launch/wm-launch-preload.so WM_LAUNCH_ID=id1 xterm
```
The window created by `xterm` will have the property `WM_LAUNCH_ID`:
```
$ xprop WM_LAUNCH_ID
> WM_LAUNCH_ID(UTF8_STRING) = "id1"
```

A command-line tool is provided for convenience and for interacting with window
factories:
```
usage: wm-launch [options] WM_LAUNCH_ID COMMAND...

options:
  -h          Show help message
  -s          Launch with systemd-run
  -j          Launch with firejail
  -f FACTORY  Launch via a window factory
  -v          Show version
```

### Window factories
A window factory is an X11 client responsible for creating the windows of new
clients. It can be either implicit or explicit, the key difference being that
implicit factories create their own window.

#### Implicit factory
An implicit factory is an X11 client that reuses a single instance to create
additional windows each time it's launched, e.g. `qutebrowser`, `kitty -1`.

To correctly set the `WM_LAUNCH_ID` of an implicit factory, always run it via
`wm-launch` with the same argument to the `-f` flag:
```
wm-launch -f qute id2 qutebrowser
```

#### Explicit factory
An explicit window factory is a daemon that creates windows based on a client's
request, e.g. `emacsd` and `emacsclient`, `urxvtd` and `urxvtc`.

To correctly set the `WM_LAUNCH_ID` of an explicit factory, run the daemon with
`LD_PRELOAD` and `WM_LAUNCH_FACTORY`:
```
LD_PRELOAD=/usr/lib/wm-launch/wm-launch-preload.so WM_LAUNCH_FACTORY=emacs emacsd
```

Then launch with `wm-launch -f emacs id3 emacsclient`.

### wm-launchd

wm-launchd must be running to handle window factories. Enable the systemd
service to run when the `graphical-session.target` is reached:
```
systemctl --user enable wm-launchd
```

### systemd-run
A client can be launched with `systemd-run` by running it via `wm-launch` using
the `-s` flag. This will run the command in a user scope with
`systemd-run --user --scope`.

### Firejail
A client can be launched with [firejail](https://github.com/netblue30/firejail)
by running it via `wm-launch` using the `-j` flag. This sets the required
environment variables in the sandbox created by firejail.

## Limitations
* Be aware of environment variable inheritance. This becomes a problem when
  launching a client from a terminal created by a factory, i.e.
  `WM_LAUNCH_FACTORY` is present in its environment. The new client will inherit
  this variable and expect a factory file with its ID to exist. This scenario
  can be avoided by launching the client with `wm-launch -f ""`.

## Window manager integration
* Integration with Awesome WM is provided by
  [awesome-launch](https://github.com/jcrd/awesome-launch).

## Building

### Dependencies
* libxcb
* libx11
* perl
* go

Build with `make`.

### Installing

Install with `make install`.

### Testing

Tests have additional dependencies:
* xvfb

Run tests locally with `make test` or use `make test-podman` to run them in a
[supplantr/wm-launch](https://hub.docker.com/r/supplantr/wm-launch) container
which includes all dependencies.

*Note:* firejail [cannot](https://github.com/netblue30/firejail/issues/2579)
run in containers. It must be tested locally.

## License

wm-launch is licensed under the GNU Lesser General Public License v2.1 or later
(see [LICENSE](LICENSE)).

## Acknowledgements

wm-launch-preload.c is based on
[ld-preload-xcreatewindow-net-wm-pid](https://github.com/deepfire/ld-preload-xcreatewindow-net-wm-pid).

[1]: https://copr.fedorainfracloud.org/coprs/jcrd/wm-launch/
