#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <X11/Xlib-xcb.h>

#include "common.h"

#ifdef DEBUG
#define LOG(fmt, ...) (printf(fmt, ##__VA_ARGS__))
#else
#define LOG(fmt, ...)
#endif

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
get_factory_path(void)
{
    static int get_factory = 1;
    static const char *factory = NULL;
    static char path[BUFSIZ];

    char dir[BUFSIZ / 2];
    const char *display = NULL;
    const char *rundir = NULL;

    if (*path)
        return path;

    if (get_factory) {
        factory = getenv("WM_LAUNCH_FACTORY");
        get_factory = 0;
    }

    if (!factory)
        return NULL;

    display = getenv("DISPLAY");

    if (!display) {
        fprintf(stderr, "DISPLAY not set\n");
        exit(EXIT_FAILURE);
    }

    rundir = getenv("XDG_RUNTIME_DIR");

    if (rundir)
        snprintf(dir, sizeof(dir), "%s/wm-launch/%s", rundir, display);
    else
        snprintf(dir, sizeof(dir), "/tmp/wm-launch/%d/%s", getuid(), display);

    snprintf(path, sizeof(path), "%s/%s", dir, factory);

    LOG("WM_LAUNCH_FACTORY=%s, file=%s\n", factory, path);

    return path;
}

static char *
get_launch_id(const char *factory_path)
{
    static int get_id_env = 1;
    static const char *id_env = NULL;
    static char id[IDSIZE];

    FILE *fh = NULL;

#define ERROR(path) { \
        LOG("%s", "\n"); \
        fprintf(stderr, "ERROR: %s :", path); \
        perror(NULL); \
    }

    if (factory_path) {
        if (access(factory_path, F_OK) != -1) {
            fh = fopen(factory_path, "r");

            if (!fh) {
                ERROR(factory_path);
                return NULL;
            }

            if (!fgets(id, sizeof(id), fh)) {
                fprintf(stderr, "ERROR: %s : failed to read factory file\n",
                        factory_path);
                fclose(fh);
                return NULL;
            }
            fclose(fh);

            if (unlink(factory_path) == -1)
                ERROR(factory_path);

            id[strcspn(id, "\n")] = 0;
        } else {
            LOG("%s", ": factory file not accessible, using");
        }
    } else {
        if (get_id_env) {
            id_env = getenv("WM_LAUNCH_ID");
            get_id_env = 0;
        }
        if (!*id) {
            if (id_env) {
                snprintf(id, sizeof(id), "%s", id_env);
            } else {
                LOG("%s", "\n");
                fprintf(stderr, "ERROR: WM_LAUNCH_ID not set\n");
                return NULL;
            }
        }
    }

#undef ERROR

    return id;
}

static void
set_wm_launch_id(xcb_connection_t *conn, xcb_window_t win, xcb_window_t parent,
                 const char *fname)
{
    static xcb_atom_t wm_launch_id = 0;
    static xcb_atom_t utf8_string = 0;

    if (!utf8_string)
        utf8_string = intern_atom(conn, "UTF8_STRING", 1);

    if (!wm_launch_id)
        wm_launch_id = intern_atom(conn, "WM_LAUNCH_ID", 0);

    LOG("window[0x%X] from %s", win, fname);
#ifndef DEBUG
    (void)fname;
#endif

    if (!window_is_root(conn, parent)) {
        LOG("%s\n", ": not top level, skipping");
        return;
    }

    const char *factory_path = get_factory_path();
    const char *id = get_launch_id(factory_path);

    if (!id)
        return;

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
