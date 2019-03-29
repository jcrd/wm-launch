#include <string.h>
#include <xcb/xcb.h>

#define IDSIZE 256

static xcb_atom_t
intern_atom(xcb_connection_t *conn, char *name, int only_if_exists)
{
    xcb_intern_atom_cookie_t c;
    xcb_intern_atom_reply_t *r;
    xcb_atom_t atom = 0;

    c = xcb_intern_atom(conn, only_if_exists, strlen(name), name);
    r = xcb_intern_atom_reply(conn, c, NULL);

    if (r) {
        atom = r->atom;
        free(r);
    }

    return atom;
}
