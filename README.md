# wm-launch [![CircleCI](https://circleci.com/gh/jcrd/wm-launch.svg?style=svg)](https://circleci.com/gh/jcrd/wm-launch)

**wm-launch** provides a shared library for use with `LD_PRELOAD` and a
command-line tool to set IDs on newly created X11 windows. It is intended to be
used by a window manager to uniquely identify clients it launches.

## Building

### Dependencies
* libxcb
* libx11

Build with `make`.

### Installing

Install with `make install`.

### Testing

Tests have additional dependencies:
* xvfb
* xterm

Run tests with `make test`.

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

A command-line tool is provided for convenience:
```
wm-launch [-f FACTORY] WM_LAUNCH_ID COMMAND...
```

### Window factories

Window factories are daemons that create X11 windows based on a client's
request, e.g. `emacsd` and `emacsclient`, `urxvtd` and `urxvtc`.

To correctly set the `WM_LAUNCH_ID` of these windows, first run the daemon with
`LD_PRELOAD` and `WM_LAUNCH_FACTORY`:
```
LD_PRELOAD=/usr/lib/wm-launch/wm-launch-preload.so WM_LAUNCH_FACTORY=emacs emacsd
```
`WM_LAUNCH_FACTORY` specifies the filename in the runtime directory containing
the next ID to use.

Then launch with `wm-launch -f emacs id2 emacsclient` which writes the ID to
`emacs` in the runtime directory. The `LD_PRELOAD` for `emacsd` then reads and
deletes this file when the new window is created.

## Limitations
* X11 clients launched from windows with `LD_PRELOAD` and `WM_LAUNCH_ID` in their
  environment (such as a terminal) will inherit the `WM_LAUNCH_ID` value.

## Window manager integration
* Integration with Awesome WM is provided by
  [awesome-launch](https://github.com/jcrd/awesome-launch).

## License

wm-launch is licensed under the GNU Lesser General Public License v2.1 or later
(see [LICENSE](LICENSE)).

## Acknowledgements

wm-launch is based on
[ld-preload-xcreatewindow-net-wm-pid](https://github.com/deepfire/ld-preload-xcreatewindow-net-wm-pid).
