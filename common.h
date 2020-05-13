/*
wm-launch - launch X11 clients with unique IDs
Copyright (C) 2019-2020 James Reed

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdlib.h>
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
