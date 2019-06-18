#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <xcb/xcb.h>

int
main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    xcb_connection_t *conn = xcb_connect(NULL, NULL);

    if (!conn) {
        fprintf(stderr, "Failed to connect\n");
        return EXIT_FAILURE;
    }

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_window_t wid = xcb_generate_id(conn);
    xcb_void_cookie_t c;
    xcb_generic_error_t *e;

    c = xcb_create_window_checked(conn, XCB_COPY_FROM_PARENT, wid, screen->root,
            0, 0, 1, 1, 0, XCB_WINDOW_CLASS_COPY_FROM_PARENT,
            XCB_COPY_FROM_PARENT, 0, NULL);
    if ((e = xcb_request_check(conn, c))) {
        fprintf(stderr, "Failed to create window\n");
        free(e);
        xcb_disconnect(conn);
        return EXIT_FAILURE;
    }

    c = xcb_map_window_checked(conn, wid);
    if ((e = xcb_request_check(conn, c))) {
        fprintf(stderr, "Failed to map window\n");
        free(e);
        xcb_disconnect(conn);
        return EXIT_FAILURE;
    }

    printf("0x%X\n", wid);

    while (xcb_wait_for_event(conn));
    xcb_destroy_window(conn, wid);
    xcb_disconnect(conn);
}
