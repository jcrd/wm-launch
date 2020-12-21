/*
wm-launch - launch X11 clients with unique IDs
Copyright (C) 2019-2020 James Reed

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <X11/Xlib-xcb.h>

#include "common.h"

static int debug = 0;
#define LOG(fmt, ...) if (debug) (printf(fmt, ##__VA_ARGS__))

#define INIT_FUNC(func, handle) \
    if (!func) func = get_func(__func__, handle)

enum {
    XCB_HANDLE,
    X11_HANDLE,
};

static void *handles[2] = {NULL};

static void
close_xcb_handle(void)
{
    dlclose(handles[XCB_HANDLE]);
}

static void
close_x11_handle(void)
{
    dlclose(handles[X11_HANDLE]);
}

static void
check_dlerror(void)
{
    const char *err = dlerror();
    if (err) {
        fprintf(stderr, "%s\n", err);
        exit(EXIT_FAILURE);
    }
}

static void *
get_handle(int hid)
{
    void **handle = &handles[hid];

    if (*handle)
        return *handle;

    switch (hid) {
    case XCB_HANDLE:
        *handle = dlopen(XCB_SONAME, RTLD_NOW);
        atexit(close_xcb_handle);
        break;
    case X11_HANDLE:
        *handle = dlopen(X11_SONAME, RTLD_NOW);
        atexit(close_x11_handle);
        break;
    }

    check_dlerror();

    return *handle;
}

static void *
get_func(const char *fname, int hid)
{
    void *handle = get_handle(hid);
    void *func = dlsym(handle, fname);

    check_dlerror();

    return func;
}

static int
window_is_root(xcb_connection_t *conn, xcb_window_t win)
{
    static const xcb_setup_t *xcb_setup = NULL;
    xcb_screen_iterator_t iter;

    if (!xcb_setup)
        xcb_setup = xcb_get_setup(conn);

    iter = xcb_setup_roots_iterator(xcb_setup);

    while (iter.rem) {
        if (win == iter.data->root)
            return 1;
        xcb_screen_next(&iter);
    }

    return 0;
}

static char *
get_socket_path(void)
{
    static char path[BUFSIZ / 2];
    const char *display = NULL;
    const char *rundir = NULL;

    if (*path)
        return path;

    display = getenv("DISPLAY");

    if (!display) {
        fprintf(stderr, "DISPLAY not set\n");
        exit(EXIT_FAILURE);
    }

    rundir = getenv("XDG_RUNTIME_DIR");

    if (!rundir) {
        fprintf(stderr, "XDG_RUNTIME_DIR not set\n");
        exit(EXIT_FAILURE);
    }

    snprintf(path, sizeof(path), "%s/wm-launch/%s.sock", rundir, display);

    return path;
}

static char *
get_factory_name(void)
{
    static int get_name = 1;
    static char *name = NULL;

    if (get_name) {
        name = getenv("WM_LAUNCH_FACTORY");
        if (name && strcmp(name, "") == 0)
            name = NULL;
        get_name = 0;
    }

    return name;
}

static int
send_msg(char *msg, size_t msg_len, char *rsp, size_t rsp_len)
{
    struct sockaddr_un addr;
    int fd;
    int ret = 0;

    addr.sun_family = AF_UNIX;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Failed to create socket");
        return 0;
    }

    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", get_socket_path());

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Failed to connect to wm-launchd socket");
        goto err;
    }

    LOG("wm-launch-preload: Sending message: %s", msg);
    if (write(fd, msg, msg_len) == -1) {
        perror("Failed to write to wm-launchd socket");
        goto err;
    }

    if (!rsp) {
        ret = 1;
        goto err;
    }

    struct pollfd fds[] = {
        {fd, POLLIN, 0},
    };

    while (poll(fds, 1, -1) > 0)
        if (fds[0].revents & POLLIN) {
            if (read(fd, rsp, rsp_len) == -1) {
                perror("Failed to read from wm-launchd socket");
            } else {
                ret = 1;
                LOG("wm-launch-preload: Response: %s", rsp);
            }
            break;
        }

err:
    close(fd);
    return ret;
}

static char *
get_env_id(void)
{
    static char id[IDSIZE];
    static int get_env_id = 1;
    const char *env_id = NULL;

    if (get_env_id) {
        env_id = getenv("WM_LAUNCH_ID");
        if (env_id)
            snprintf(id, sizeof(id), "%s", env_id);
        get_env_id = 0;
    }

    return id;
}

static int
register_factory(void)
{
    static int pid = -1;
    char msg[BUFSIZ];
    char rsp[BUFSIZ];

    if (pid == -1)
        pid = getpid();

    snprintf(msg, sizeof(msg), "REGISTER_FACTORY %s %d\n",
            get_factory_name(), pid);
    send_msg(msg, sizeof(msg), rsp, sizeof(rsp));

    if (strcmp(rsp, "OK\n") == 0)
        return 1;

    return 0;
}

static void
remove_factory(void)
{
    char msg[BUFSIZ];
    char rsp[BUFSIZ];

    snprintf(msg, sizeof(msg), "REMOVE_FACTORY %s\n", get_factory_name());
    send_msg(msg, sizeof(msg), rsp, sizeof(rsp));
}

static char *
get_factory_id(void)
{
    static char id[IDSIZE];
    char fmt[16];
    char msg[BUFSIZ];
    char rsp[BUFSIZ];
    const char *factory = get_factory_name();

    snprintf(fmt, sizeof(fmt), "ID %%%lus", sizeof(id));
    snprintf(msg, sizeof(msg), "GET_ID %s\n", factory);

    if (!send_msg(msg, sizeof(msg), rsp, sizeof(rsp)))
        return NULL;

    if (sscanf(rsp, fmt, &id) == 0)
        LOG("wm-launch-preload: Factory %s has no IDs\n", factory);

    return id;
}

static void
set_wm_launch_id(xcb_connection_t *conn, xcb_window_t win, xcb_window_t parent,
                 const char *fname)
{
    static xcb_atom_t wm_launch_id = 0;
    static xcb_atom_t utf8_string = 0;
    static int get_debug = 1;
    const char *env_debug = NULL;
    const char *factory = get_factory_name();
    const char *id = get_env_id();

    if (!utf8_string)
        utf8_string = intern_atom(conn, "UTF8_STRING", 1);

    if (!wm_launch_id)
        wm_launch_id = intern_atom(conn, "WM_LAUNCH_ID", 0);

    if (get_debug) {
        env_debug = getenv("DEBUG");
        if (env_debug)
            debug = 1;
        get_debug = 0;
    }

    LOG("wm-launch-preload: window[0x%X] from %s", win, fname);

    if (!window_is_root(conn, parent)) {
        LOG(": %s\n", "not top level, skipping");
        return;
    }

    if (factory) {
        LOG("\n");
        if (register_factory())
            atexit(remove_factory);
        id = get_factory_id();
    }

    if (!*id) {
        LOG(": %s\n", "no ID specified");
        return;
    }

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, wm_launch_id,
                        utf8_string ? utf8_string : XCB_ATOM_STRING, 8,
                        strlen(id), (void *)id);

    LOG(": WM_LAUNCH_ID=%s\n", id);
}

xcb_void_cookie_t
xcb_create_window(
    xcb_connection_t *conn,
    uint8_t depth,
    xcb_window_t wid,
    xcb_window_t parent,
    int16_t x, int16_t y,
    uint16_t width, uint16_t height,
    uint16_t border_width,
    uint16_t _class,
    xcb_visualid_t visual,
    uint32_t value_mask,
    const void *value_list)
{
    static xcb_void_cookie_t (*func)(
        xcb_connection_t *conn,
        uint8_t depth,
        xcb_window_t wid,
        xcb_window_t parent,
        int16_t x, int16_t y,
        uint16_t width, uint16_t height,
        uint16_t border_width,
        uint16_t _class,
        xcb_visualid_t visual,
        uint32_t value_mask,
        const void *value_list) = NULL;

    xcb_void_cookie_t c;

    INIT_FUNC(func, XCB_HANDLE);

    c = (*func)(conn, depth, wid, parent, x, y, width, height, border_width,
                _class, visual, value_mask, value_list);

    set_wm_launch_id(conn, wid, parent, __func__);

    return c;
}

xcb_void_cookie_t
xcb_create_window_checked(
    xcb_connection_t *conn,
    uint8_t depth,
    xcb_window_t wid,
    xcb_window_t parent,
    int16_t x, int16_t y,
    uint16_t width, uint16_t height,
    uint16_t border_width,
    uint16_t _class,
    xcb_visualid_t visual,
    uint32_t value_mask,
    const void *value_list)
{
    static xcb_void_cookie_t (*func)(
        xcb_connection_t *conn,
        uint8_t depth,
        xcb_window_t wid,
        xcb_window_t parent,
        int16_t x, int16_t y,
        uint16_t width, uint16_t height,
        uint16_t border_width,
        uint16_t _class,
        xcb_visualid_t visual,
        uint32_t value_mask,
        const void *value_list) = NULL;

    xcb_void_cookie_t c;

    INIT_FUNC(func, XCB_HANDLE);

    c = (*func)(conn, depth, wid, parent, x, y, width, height, border_width,
                _class, visual, value_mask, value_list);

    set_wm_launch_id(conn, wid, parent, __func__);

    return c;
}

xcb_void_cookie_t
xcb_reparent_window(
    xcb_connection_t *conn,
    xcb_window_t window,
    xcb_window_t parent,
    int16_t x, int16_t y)
{
    static xcb_void_cookie_t (*func)(
        xcb_connection_t *conn,
        xcb_window_t window,
        xcb_window_t parent,
        int16_t x, int16_t y) = NULL;

    INIT_FUNC(func, XCB_HANDLE);

    set_wm_launch_id(conn, window, parent, __func__);

    return (*func)(conn, window, parent, x, y);
}

Window
XCreateWindow(
    Display *display,
    Window parent,
    int x, int y,
    unsigned int width, unsigned int height,
    unsigned int border_width,
    int depth,
    unsigned int class,
    Visual *visual,
    unsigned long valuemask,
    XSetWindowAttributes *attributes)
{
    static Window (*func)(
        Display *display,
        Window parent,
        int x, int y,
        unsigned int width, unsigned int height,
        unsigned int border_width,
        int depth,
        unsigned int class,
        Visual *visual,
        unsigned long valuemask,
        XSetWindowAttributes *attributes) = NULL;

    Window win;

    INIT_FUNC(func, X11_HANDLE);

    win = (*func)(display, parent, x, y, width, height, border_width, depth,
                  class, visual, valuemask, attributes);

    set_wm_launch_id(XGetXCBConnection(display), win, parent, __func__);

    return win;
}

Window
XCreateSimpleWindow(
    Display *display,
    Window parent,
    int x, int y,
    unsigned int width, unsigned int height,
    unsigned int border_width,
    unsigned long border,
    unsigned long background)
{
    static Window (*func)(
        Display *display,
        Window parent,
        int x, int y,
        unsigned int width, unsigned int height,
        unsigned int border_width,
        unsigned long border,
        unsigned long background) = NULL;

    Window win;

    INIT_FUNC(func, X11_HANDLE);

    win = (*func)(display, parent, x, y, width, height, border_width, border,
                  background);

    set_wm_launch_id(XGetXCBConnection(display), win, parent, __func__);

    return win;
}

int
XReparentWindow(
    Display *display,
    Window window,
    Window parent,
    int x, int y)
{
    static int (*func)(
        Display *display,
        Window window,
        Window parent,
        int x, int y) = NULL;

    INIT_FUNC(func, X11_HANDLE);

    set_wm_launch_id(XGetXCBConnection(display), window, parent, __func__);

    return (*func)(display, window, parent, x, y);
}
