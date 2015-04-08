//  Copyright © 2015 Andrew Comminos <andrew@morlunk.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include "xpand.h"

int XpandWindow(Window source_window, int scale) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "xpand: failed to open display\n");
        return -1;
    }

    int comp_event_base, comp_error_base;
    // require xcomposite
    if (!XCompositeQueryExtension(display, &comp_event_base, &comp_error_base)) {
        fprintf(stderr, "xpand: xcomposite is required\n");
        return -1;
    }
    int damage_event_base, damage_error_base;
    // require xdamage
    if (!XDamageQueryExtension(display, &damage_event_base, &damage_error_base)) {
        fprintf(stderr, "xpand: xdamage is required\n");
        return -1;
    }
    int render_event_base, render_error_base;
    // require xrender
    if (!XRenderQueryExtension(display, &render_event_base, &render_error_base)) {
        fprintf(stderr, "xpand: xrender is required\n");
        return -1;
    }

    XWindowAttributes attrs;
    if(XGetWindowAttributes(display, source_window, &attrs) < 0) {
        fprintf(stderr, "xpand: failed to get attrs for wid %d\n", source_window);
    }

    XTransform scale_transform = { .matrix = {{ scale, 0, 0 }, { 0, scale, 0 }, { 0, 0, 1 }}};
    int scaled_width = attrs.width * scale;
    int scaled_height = attrs.height * scale;

    XSetWindowAttributes xpand_attrs = { .event_mask = attrs.all_event_masks };
    Window xpand_window = XCreateWindow(display, attrs.root, attrs.x,
        attrs.y, scaled_width, scaled_height, attrs.border_width,
        CopyFromParent, InputOutput, CopyFromParent, CWEventMask,
        &xpand_attrs);

    //XUnmapWindow(display, source_window);
    XMapWindow(display, xpand_window);

    printf("xpand: event mask 0x%x\n", xpand_attrs.event_mask);
    
    Pixmap source_pixmap = XCompositeNameWindowPixmap(display, source_window);
    Damage damage = XDamageCreate(display, source_window, XDamageReportNonEmpty);
    GC gc = XCreateGC(display, xpand_window, 0, NULL);

    XEvent e;
    while (1) {
        // TODO: intercept resize events for window and rescale appropriately
        // TODO: use damage events to intercept window changes? we have to do something
        XNextEvent(display, &e);
        if (e.type == damage_event_base) {
            // Handle damage
            XCopyArea(display, source_window, xpand_window, gc, 0, 0, scaled_width, scaled_height, 0, 0);
            XDamageSubtract(display, damage, None, None);
        } else if (((XAnyEvent*)&e)->window == xpand_window) {
            // Perform transforms from scaled to unscaled coordinates
            switch (e.type) {
                case ButtonPress:
                case ButtonRelease:
                    ((XButtonEvent*)&e)->x /= scale;
                    ((XButtonEvent*)&e)->y /= scale;
                    break;
                case KeyPress:
                case KeyRelease:
                    ((XKeyEvent*)&e)->x /= scale;
                    ((XKeyEvent*)&e)->y /= scale;
                    break;
                case MotionNotify:
                    ((XMotionEvent*)&e)->x /= scale;
                    ((XMotionEvent*)&e)->y /= scale;
                    break;
            }
            printf("xpand: event received of type %d\n", e.type);
            // Forward all events to target window
            XSendEvent(display, source_window, False, 0, &e);
        }
    }
    XDamageDestroy(display, damage);
    XUnmapWindow(display, xpand_window);
    //XMapWindow(display, source_window);
    XDestroyWindow(display, xpand_window);
    XCloseDisplay(display);
}
