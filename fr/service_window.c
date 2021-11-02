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

#include <gtk/gtk.h>

#include "global.h"
#include "../parser.h"
#include "window.h"
#include "main_window.h"
#include "cash_window.h"
#include "service_window.h"
#include "process.h"

static GtkWidget * service_window=NULL;

static gboolean CloseEvent(void) {
    process_event(SERVICE_CLOSE_EVENT);
    gtk_widget_destroy(service_window);
    return FALSE;
    }

static gboolean CashEvent(void) {
    process_event(CASH_EVENT);
    gtk_widget_destroy(service_window);
    return FALSE;
    }

static gboolean XReportEvent(void) {
    process_event(XREPORT_EVENT);
    gtk_widget_destroy(service_window);
    return FALSE;
    }

static gboolean ZReportEvent(void) {
    process_event(SESSION_EVENT);
    gtk_widget_destroy(service_window);
    return FALSE;
    }

static gboolean ReturnEvent(void) {
    process_event(RETURN_EVENT);
    gtk_widget_destroy(service_window);
    return FALSE;
    }

static int label_callback(GtkWidget * widget, struct tag_struct * tag) {
    char * arg=tag_getValue(tag, "label");
    if (arg==NULL) return 0;
    if (strcmp(arg, "cash")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(CashEvent),
			     NULL);
    if (strcmp(arg, "return")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(ReturnEvent),
			     NULL);
    if (strcmp(arg, "xreport")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(XReportEvent),
			     NULL);
    if (strcmp(arg, "zreport")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(ZReportEvent),
			     NULL);
    else if (strcmp(arg, "close")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_press_event",
			     G_CALLBACK(CloseEvent),
			     NULL);
    return 0;
    }


void serviceWindow_create(void) {
    service_window=window_create("/usr/share/mini-rmk/service_window.xml",
				(label_callback_func) label_callback,
				NULL);
    if (service_window!=NULL) {
	g_signal_connect(G_OBJECT(service_window), "delete_event", G_CALLBACK(CloseEvent), NULL);
        gtk_window_set_transient_for(GTK_WINDOW(service_window), GTK_WINDOW(main_window));
	gtk_window_set_position(GTK_WINDOW(service_window), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_widget_show(service_window);
	}
    }
