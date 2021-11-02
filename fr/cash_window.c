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
#include "../numeric.h"
#include "process.h"

static GtkWidget * internal_input_string=NULL;
static GtkWidget * cash_window=NULL;

static gboolean AddText(GtkWidget * widget, GdkEventKey * event, const gchar * text) {
    char str[16];
    bzero(str, sizeof(str));
    size_t len, len1;
    int i;
    if (internal_input_string!=NULL) {
	gint start, end;
	if (gtk_editable_get_selection_bounds(GTK_EDITABLE(internal_input_string), &start, &end))
	    gtk_editable_delete_text(GTK_EDITABLE(internal_input_string), start, end);
	gtk_entry_append_text(GTK_ENTRY(internal_input_string), text);
	const char * s=gtk_entry_get_text(GTK_ENTRY(internal_input_string));
	char * p=strchr(s, '.');
	if (p!=NULL) {
	    len=p-s;
	    if (len==0) {
		str[0]='0';
		str[1]='.';
		len=2;
		}
	    else {
		for (i=0; i<len && i<10; i++) {
		    switch (s[i]) {
		    case '0' ... '9': str[i]=s[i];
			}
		    }
		str[i]='.';
		len=i;
		}
	    len1=strlen(p);
	    for (i=1; i<len1 && i<3; i++) {
		switch (p[i]) {
		case '0' ... '9': str[i+len]=p[i];
		    }
		}
	    }
	else {
	    len=strlen(s);
	    for (i=0; i<len && i<10; i++) {
		switch (s[i]) {
		case '0' ... '9': str[i]=s[i];
		    }
		}
	    }
	gtk_entry_set_text(GTK_ENTRY(internal_input_string), str);
	}
    return FALSE;
    }

static gboolean InputStringDelete(void) {
    char str[16];
    bzero(str, sizeof(str));
    if (internal_input_string!=NULL) {
	gint start, end;
	if (gtk_editable_get_selection_bounds(GTK_EDITABLE(internal_input_string), &start, &end))
	    gtk_editable_delete_text(GTK_EDITABLE(internal_input_string), start, end);
	const char * s=gtk_entry_get_text(GTK_ENTRY(internal_input_string));
	size_t len=strlen(s);
        if (len>sizeof(str)-1) len=sizeof(str)-1;
	if (len>1) memcpy(str, s, len-1);
	if (len>0) gtk_entry_set_text(GTK_ENTRY(internal_input_string), str);
	}
    return FALSE;
    }

static gboolean InputStringClear(void) {
    gtk_entry_set_text(GTK_ENTRY(internal_input_string), "");
    return FALSE;
    }
    
static gboolean CloseEvent(void) {
    process_event(CASH_CLOSE_EVENT);
    gtk_widget_destroy(cash_window);
    return FALSE;
    }

static gboolean CashInAction(void) {
    NUMERIC(v, 2);
    char * s=(char*)gtk_entry_get_text(GTK_ENTRY(internal_input_string));
    num_fromString(&v, s);
    process_event(CASHIN_EVENT, &v);
    gtk_widget_destroy(cash_window);
//    gtk_entry_set_text(GTK_ENTRY(internal_input_string), "");
    return FALSE;
    }

static gboolean CashOutAction(void) {
    NUMERIC(v, 2);
    char * s=(char*)gtk_entry_get_text(GTK_ENTRY(internal_input_string));
    num_fromString(&v, s);
    process_event(CASHOUT_EVENT, &v);
    gtk_widget_destroy(cash_window);
//    gtk_entry_set_text(GTK_ENTRY(internal_input_string), "");
    return FALSE;
    }

static int label_callback(GtkWidget * widget, struct tag_struct * tag) {
    char * arg=tag_getValue(tag, "label");
    if (arg==NULL) return 0;
    if (strcmp(arg, "num_0")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "0");
    else if (strcmp(arg, "num_1")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "1");
    else if (strcmp(arg, "num_2")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "2");
    else if (strcmp(arg, "num_3")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "3");
    else if (strcmp(arg, "num_4")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "4");
    else if (strcmp(arg, "num_5")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "5");
    else if (strcmp(arg, "num_6")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "6");
    else if (strcmp(arg, "num_7")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "7");
    else if (strcmp(arg, "num_8")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "8");
    else if (strcmp(arg, "num_9")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     "9");
    else if (strcmp(arg, "num_dot")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(AddText),
			     ".");
    else if (strcmp(arg, "close")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(CloseEvent),
			     NULL);
    else if (strcmp(arg, "delete")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(InputStringDelete),
			     NULL);
    else if (strcmp(arg, "clear")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(InputStringClear),
			     NULL);
    else if (strcmp(arg, "cashin")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(CashInAction),
			     NULL);
    else if (strcmp(arg, "cashout")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(CashOutAction),
			     NULL);
    else if (strcmp(arg, "input_string")==0) internal_input_string=widget;
    return 0;
    }

gboolean cashWindow_create(num_t * summ) {
    cash_window=window_create("/usr/share/mini-rmk/cash_window.xml",
				(label_callback_func) label_callback,
				NULL);
    if (cash_window==NULL) return FALSE;
    g_signal_connect(G_OBJECT(cash_window), "delete_event", G_CALLBACK(CloseEvent), NULL);
    gtk_window_set_transient_for(GTK_WINDOW(cash_window), GTK_WINDOW(main_window));
    gtk_window_set_position(GTK_WINDOW(cash_window), GTK_WIN_POS_CENTER_ALWAYS);
    char s[16];
    num_toString(s, summ);
    gtk_entry_set_text(GTK_ENTRY(internal_input_string), s);
    gtk_widget_show(cash_window);
    return FALSE;
    }
