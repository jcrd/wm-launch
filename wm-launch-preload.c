#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
get_runtime_dir(void)
{
    static char dir[BUFSIZ / 2];
    const char *display = NULL;
    const char *rundir = NULL;

    if (*dir)
        return dir;

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

    return dir;
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

static char *
get_factory_lock_path(void)
{
    static char path[BUFSIZ + 4];

    if (*path)
        return path;

    const char *name = get_factory_name();
    if (!name)
        return NULL;
    const char *dir = get_runtime_dir();
    snprintf(path, sizeof(path), "%s/%s%s", dir, name, ".lck");

    return path;
}

static char *
get_factory_file_path(void)
{
    static char path[BUFSIZ];

    if (*path)
        return path;

    const char *name = get_factory_name();
    if (!name)
        return NULL;
    const char *dir = get_runtime_dir();
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    return path;
}

static char *
get_launch_id(const char *file)
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

    if (file) {
        if (access(file, F_OK) != -1) {
            fh = fopen(file, "r");

            if (!fh) {
                ERROR(file);
                return NULL;
            }

            if (!fgets(id, sizeof(id), fh)) {
                fprintf(stderr, "ERROR: %s : failed to read factory file\n",
                        file);
                fclose(fh);
                return NULL;
            }
            fclose(fh);

            if (unlink(file) == -1)
                ERROR(file);
        } else {
            LOG("%s", ": factory file not accessible, using");
        }
    } else {
        if (get_id_env) {
            id_env = getenv("WM_LAUNCH_ID");
            if (id_env)
                snprintf(id, sizeof(id), "%s", id_env);
            get_id_env = 0;
        }
    }

#undef ERROR

    return id;
}

static void
cleanup_factory(void)
{
    const char *lock = get_factory_lock_path();
    const char *file = get_factory_file_path();

    if (access(lock, F_OK) != -1) {
        remove(lock);
        LOG("cleanup: removed %s\n", lock);
    }
    if (access(file, F_OK) != -1) {
        remove(file);
        LOG("cleanup: removed %s\n", file);
    }
}

static void
set_wm_launch_id(xcb_connection_t *conn, xcb_window_t win, xcb_window_t parent,
                 const char *fname)
{
    static xcb_atom_t wm_launch_id = 0;
    static xcb_atom_t utf8_string = 0;
    static int get_debug = 1;

    if (!utf8_string)
        utf8_string = intern_atom(conn, "UTF8_STRING", 1);

    if (!wm_launch_id)
        wm_launch_id = intern_atom(conn, "WM_LAUNCH_ID", 0);

    if (get_debug) {
        const char *var = getenv("DEBUG");
        if (var)
            debug = 1;
        get_debug = 0;
    }

    LOG("window[0x%X] from %s", win, fname);

    if (!window_is_root(conn, parent)) {
        LOG("%s\n", ": not top level, skipping");
        return;
    }

    const char *lock = get_factory_lock_path();
    const char *file = get_factory_file_path();

    if (lock && access(lock, F_OK) == -1) {
        int fd = open(lock, O_CREAT, 0);
        close(fd);
        LOG(" (lock=%s)", lock);
        atexit(cleanup_factory);
    }

    if (file)
        LOG(": factory=%s", file);

    const char *id = get_launch_id(file);

    if (!*id) {
        LOG(": WM_LAUNCH_ID not set\n");
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
