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
#include "../numeric.h"
#include "process.h"

static GtkWidget * internal_input_string=NULL;
static GtkWidget * internal_total_text=NULL;
static GtkWidget * internal_payment_text=NULL;
static GtkWidget * internal_payment_button=NULL;
static GtkWidget * receipt_window=NULL;

static num_t internal_payment_summ;
static num_t internal_total_summ;

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
    process_event(RECEIPT_CLOSE_EVENT);
    gtk_widget_destroy(receipt_window);
    return FALSE;
    }

static gboolean CancelEvent(void) {
    process_event(CANCEL_EVENT);
    gtk_widget_destroy(receipt_window);
    return FALSE;
    }

static gboolean PaymentEvent(void) {
    NUMERIC(v, 2);
    char * s=(char*)gtk_entry_get_text(GTK_ENTRY(internal_input_string));
    num_fromString(&v, s);
    process_event(RECEIPT_EVENT, &v);
    gtk_widget_destroy(receipt_window);
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
    else if (strcmp(arg, "close")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_press_event",
			     G_CALLBACK(CloseEvent),
			     NULL);
    else if (strcmp(arg, "cancel")==0)
	    g_signal_connect(G_OBJECT(widget),
			     "button_release_event",
			     G_CALLBACK(CancelEvent),
			     NULL);
    else if (strcmp(arg, "payment")==0) {
	internal_payment_button=widget;
	g_signal_connect(G_OBJECT(widget),
		         "button_release_event",
		         G_CALLBACK(PaymentEvent),
		         NULL);
	}
//    else if (strcmp(arg, "payment_2")==0)
//	    g_signal_connect(G_OBJECT(widget),
//			     "button_release_event",
//			     G_CALLBACK(PaymentEvent),
//			     "2");
    else if (strcmp(arg, "input_string")==0) internal_input_string=widget;
    else if (strcmp(arg, "payment_text")==0) internal_payment_text=widget;
    else if (strcmp(arg, "total_text")==0) internal_total_text=widget;
    else printf("unknown label: `%s'\n", arg);
    return 0;
    }

gboolean receiptWindow_create(num_t * summ) {
    receipt_window=window_create("/usr/share/mini-rmk/receipt_window.xml",
				(label_callback_func) label_callback,
				NULL);
    if (receipt_window==NULL) return FALSE;
    g_signal_connect(G_OBJECT(receipt_window), "delete_event", G_CALLBACK(CloseEvent), NULL);
    gtk_window_set_transient_for(GTK_WINDOW(receipt_window), GTK_WINDOW(main_window));
    gtk_window_set_position(GTK_WINDOW(receipt_window), GTK_WIN_POS_CENTER_ALWAYS);

    num_init(&internal_total_summ, 2);
    num_cpy(&internal_total_summ, summ);
    num_init(&internal_payment_summ, 2);

    char str[16];
    num_toString(str, &internal_total_summ);
    gtk_entry_set_text(GTK_ENTRY(internal_input_string), str);
//    if (internal_total_text!=NULL) 
    gtk_label_set_text(GTK_LABEL(internal_total_text), str);

    if (num_cmp0(summ)==0 && internal_payment_button!=NULL)
	gtk_widget_set_sensitive(internal_payment_button, FALSE);
    gtk_widget_show(receipt_window);
    return FALSE;
    }
