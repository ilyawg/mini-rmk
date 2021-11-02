//////////////////////////////////////////////////////////////////////////////
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtk.h>
#include "../parser.h"

typedef int (*label_callback_func)(GtkWidget * widget, struct tag_struct * tag);
typedef GtkWidget * (*widget_callback_func)(struct tag_struct * tag, GtkWidget * window);
GtkWidget * window_create(char * form, label_callback_func label_callback, widget_callback_func widget_callback);

#endif //WINDOW_H
