/* Wait for an XCB_MAP_REQUEST event and print the window's WM_LAUNCH_ID */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <xcb/xcb.h>

#include "../../common.h"

#define TIMEOUT 10

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

    const uint32_t values[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_void_cookie_t c = xcb_change_window_attributes_checked(conn,
            screen->root, XCB_CW_EVENT_MASK, &values);
    xcb_generic_error_t *e = xcb_request_check(conn, c);

    if (e) {
        fprintf(stderr, "Failed to register root event mask\n");
        free(e);
        goto error;
    }

    xcb_atom_t prop = intern_atom(conn, "WM_LAUNCH_ID", 0);
    xcb_atom_t type = intern_atom(conn, "UTF8_STRING", 1);

    struct timespec start;

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("Failed to get start time: ");
        goto error;
    }

    for (;;) {
        xcb_generic_event_t *ev;

        while ((ev = xcb_poll_for_event(conn))) {
            if ((ev->response_type & ~0x80) != XCB_MAP_REQUEST)
                goto next;
            const xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
            xcb_get_property_cookie_t c = xcb_get_property(conn, 0, e->window,
                    prop, type, 0, IDSIZE);
            xcb_get_property_reply_t *r = xcb_get_property_reply(conn, c, NULL);
            if (r) {
                printf("%.*s\n", xcb_get_property_value_length(r),
                        (char *)xcb_get_property_value(r));
                free(r);
                free(ev);
                xcb_disconnect(conn);
                return EXIT_SUCCESS;
            }
next:
            free(ev);
        }

        struct timespec now;

        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
            perror("Failed to get current time: ");
            goto error;
        }

        if (now.tv_sec - start.tv_sec > TIMEOUT) {
            fprintf(stderr, "Timed out\n");
            goto error;
        }
    }

error:
    xcb_disconnect(conn);
    return EXIT_FAILURE;
}
