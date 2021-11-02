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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "global.h"
#include "window.h"
#include "../parser.h"

static void dummy_label_callback(void) {
    }

static GtkWidget * dummy_widget_callback(void) {
    return NULL;
    }

static void ProcessFont(GtkWidget * w, struct tag_struct * tag) {
    int font_size=-1;
    int font_weight=-1;
    char * arg=tag_getValue(tag, "font_size");
    if (arg!=NULL) font_size=atoi(arg);

    arg=tag_getValue(tag, "font_weight");
    if (arg!=NULL) {
	if (strcmp(arg, "ultralight")==0) font_weight=PANGO_WEIGHT_ULTRALIGHT;
	else if (strcmp(arg, "light")==0) font_weight=PANGO_WEIGHT_LIGHT;
	else if (strcmp(arg, "normal")==0) font_weight=PANGO_WEIGHT_NORMAL;
	else if (strcmp(arg, "semibold")==0) font_weight=PANGO_WEIGHT_SEMIBOLD;
	else if (strcmp(arg, "bold")==0) font_weight=PANGO_WEIGHT_BOLD;
	else if (strcmp(arg, "ultrabold")==0) font_weight=PANGO_WEIGHT_ULTRABOLD;
	else if (strcmp(arg, "heavy")==0) font_weight=PANGO_WEIGHT_HEAVY;
	}
    PangoFontDescription* font=NULL;
    arg=tag_getValue(tag, "font");
    if (arg!=NULL) font=pango_font_description_from_string(arg);

    if (font_size>0) {
	if (font==NULL) font=pango_font_description_copy(gtk_widget_get_style(w)->font_desc);
	pango_font_description_set_size(font, font_size*PANGO_SCALE);
	}

    if (font_weight>0) {
	if (font==NULL) font=pango_font_description_copy(gtk_widget_get_style(w)->font_desc);
	pango_font_description_set_weight(font, font_weight);
	}

    if (font!=NULL) {
	gtk_widget_modify_font(w, font);
	pango_font_description_free(font);
	}
    }

static GtkWidget * ProcessTag(struct tag_struct * tag, GtkWidget * window, label_callback_func label_callback, widget_callback_func widget_callback);

static GtkWidget * ProcessBox(struct tag_struct * box_tag, GtkWidget * window, label_callback_func label_callback, widget_callback_func widget_callback) {
//puts("* ProcessBox()");
    gboolean homogeneous;
    char * arg=tag_getValue(box_tag, "homogeneous");
    if (arg==NULL || strcmp(arg, "true")==0) homogeneous=TRUE;
    else if (strcmp(arg, "false")==0) homogeneous=FALSE;
    else return NULL;

    int spacing=0;
    arg=tag_getValue(box_tag, "spacing");
    if (arg!=NULL) spacing=atoi(arg);

//    int width=-1;
//    arg=tag_getValue(box_tag, "width");
//    if (arg!=NULL) width=atoi(arg);

//    int height=-1;
//    arg=tag_getValue(box_tag, "height");
//    if (arg!=NULL) height=atoi(arg);
    
    arg=tag_getValue(box_tag, "type");
    if (arg==NULL) return NULL;
    GtkWidget * box=NULL;
    if (strcmp(arg, "vertical")==0) box=gtk_vbox_new(homogeneous, spacing);
    else if (strcmp(arg, "horizontal")==0) box=gtk_hbox_new(homogeneous, spacing);
    else return NULL;

//    if (width>0 || height>0) gtk_widget_set_size_request(box, width, height);
    
    label_callback(box, box_tag);
        
    struct tag_struct tag;
    tag_init(&tag);
    int item_type=xml_nextItem(&tag, NULL, 0);
    while (item_type==ITEM_TAG && tag.type!=TAG_END) {
	gboolean expand;
	arg=tag_getValue(&tag, "expand");
	if (arg==NULL || strcmp(arg, "true")==0) expand=TRUE;
	else if (strcmp(arg, "false")==0) expand=FALSE;
	else break;

	gboolean fill;
	arg=tag_getValue(&tag, "fill");
	if (arg==NULL || strcmp(arg, "true")==0) fill=TRUE;
	else if (strcmp(arg, "false")==0) fill=FALSE;
	else break;

	int padding=0;
	arg=tag_getValue(&tag, "padding");
	if (arg!=NULL) padding=atoi(arg);

	GtkWidget * w=ProcessTag(&tag, window, label_callback, widget_callback);
	if (w==NULL) break;

        gtk_box_pack_start(GTK_BOX(box), w, expand, fill, padding);
        tag_clear(&tag);
	item_type=xml_nextItem(&tag, NULL, 0);
	}
    if (item_type!=ITEM_TAG || tag.type!=TAG_END || strcmp(tag.name, "box")!=0) {
	fputs("*** window.c: ProcessBox() <box>: неверный формат\n", stderr);
	gtk_widget_destroy(box);
	box=NULL;
	}
    tag_destroy(&tag);
    return box;
    }

static GtkWidget * ProcessFrame(struct tag_struct * frame_tag, GtkWidget * window, label_callback_func label_callback, widget_callback_func widget_callback) {
//puts("* ProcessFrame()");
    char * arg=tag_getValue(frame_tag, "title");
    GtkWidget * frame=gtk_frame_new(arg);
    label_callback(frame, frame_tag);
    struct tag_struct tag;
    tag_init(&tag);
    int item_type=xml_nextItem(&tag, NULL, 0);
    if (item_type==ITEM_TAG && tag.type!=TAG_END) {
	GtkWidget * w=ProcessTag(&tag, window, label_callback, widget_callback);
	if (w!=NULL) {
    	    gtk_container_add(GTK_CONTAINER(frame), w);
    	    tag_clear(&tag);
	    item_type=xml_nextItem(&tag, NULL, 0);
	    }
	}
    if (item_type!=ITEM_TAG || tag.type!=TAG_END || strcmp(tag.name, "frame")!=0) {
	fputs("*** window.c: ProcessFrame() <frame>: неверный формат\n", stderr);
	gtk_widget_destroy(frame);
	frame=NULL;
        }
    tag_destroy(&tag);
    return frame;
    }

static GtkWidget * ProcessText(struct tag_struct * tag, label_callback_func label_callback) {
//puts("* ProcessText()");
    enum {
	ALIGN_LEFT=0,
	ALIGN_TOP,
	ALIGN_CENTER,
	ALIGN_RIGHT,
	ALIGN_BOTTOM,
	ALIGN_DEFAULT
	};
    gfloat align_values[]={0, 0, 0.5, 1, 1, 0.5};
    int size=-1;
    char * arg=tag_getValue(tag, "size");
    if (arg!=NULL) size=atoi(arg);

    int xalign;
    arg=tag_getValue(tag, "xalign");
    if (arg==NULL) xalign=ALIGN_DEFAULT;
    else if (strcmp(arg, "left")==0) xalign=ALIGN_LEFT;
    else if (strcmp(arg, "center")==0) xalign=ALIGN_CENTER;
    else if (strcmp(arg, "right")==0) xalign=ALIGN_RIGHT;
    else {
	printf("*** ProcessText(): xalign: `%s' - неверное значение\n", arg);
	return NULL;
	}

    int yalign;
    arg=tag_getValue(tag, "yalign");
    if (arg==NULL) yalign=ALIGN_DEFAULT;
    else if (strcmp(arg, "top")==0) yalign=ALIGN_TOP;
    else if (strcmp(arg, "center")==0) yalign=ALIGN_CENTER;
    else if (strcmp(arg, "bottom")==0) yalign=ALIGN_BOTTOM;
    else {
	printf("*** ProcessText(): yalign: `%s' - неверное значение\n", arg);
        return NULL;
	}

    arg=tag_getValue(tag, "value");
    GtkWidget * w=gtk_label_new(arg);
    ProcessFont(w, tag);
    if (xalign!=ALIGN_DEFAULT || yalign!=ALIGN_DEFAULT)
	gtk_misc_set_alignment(GTK_MISC(w), align_values[xalign], align_values[yalign]);

//    if (size>0) gtk_widget_set_size_request(w, -1, size);
    if (size>0) gtk_widget_set_size_request(w, 0, size);
    label_callback(w, tag);
    return w;
    }

static GtkWidget * ProcessButton(struct tag_struct * parent_tag, GtkWidget * window, label_callback_func label_callback, widget_callback_func widget_callback) {
    char * tag_name="button";
    char * arg=tag_getValue(parent_tag, "text");
    GtkWidget * button;
    if (arg==NULL) button=gtk_button_new();
    else button=gtk_button_new_with_label(arg);
    arg=tag_getValue(parent_tag, "picture");
    if (arg!=NULL) {
	char path[1024];
        strcpy(path, PIXMAP_DIR);
        strcat(path, "/");
	strcat(path, arg);
	GtkWidget * w=gtk_image_new_from_file(path);
	gtk_button_set_image(GTK_BUTTON(button), w);
	}
    label_callback(button, parent_tag);
    if (parent_tag->type==TAG_FULL && strcmp(parent_tag->name, tag_name)==0) return button;
    struct tag_struct tag;
    tag_init(&tag);
    int item_type=ITEM_TAG;
    if (parent_tag->type==TAG_BEGIN) {
        item_type=xml_nextItem(&tag, NULL, 0);
	if (item_type==ITEM_TAG && tag.type!=TAG_END) {
	    GtkWidget * w=ProcessTag(&tag, window, label_callback, widget_callback);
	    if (w!=NULL) {
    		gtk_container_add(GTK_CONTAINER(button), w);
    		tag_clear(&tag);
		item_type=xml_nextItem(&tag, NULL, 0);
		}
	    }
	}
    if (item_type!=ITEM_TAG || tag.type!=TAG_END || strcmp(tag.name, tag_name)!=0) {
	fputs("*** window.c: ProcessButton() <button>: неверный формат\n", stderr);
	gtk_widget_destroy(button);
	button=NULL;
        }
    tag_destroy(&tag);
    return button;
    }

static GtkWidget * ProcessPicture(struct tag_struct * tag, label_callback_func label_callback) {
//puts("* ProcessPicture()");
    char * arg=tag_getValue(tag, "file");
    GtkWidget * w;
    if (arg==NULL) w=gtk_image_new();
    else {
	char path[1024];
        strcpy(path, PIXMAP_DIR);
        strcat(path, "/");
	strcat(path, arg);
	w=gtk_image_new_from_file(path);
	}
    label_callback(w, tag);
    return w;
    }    

static GtkWidget * ProcessPictureButton(struct tag_struct * tag, label_callback_func label_callback) {
    GtkWidget * w=ProcessPicture(tag, label_callback);
    if (w==NULL) return NULL;
    GtkWidget * box=gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(box), w);

    char * arg=tag_getValue(tag, "picture_show");
    if (arg==NULL || strcmp(arg, "true")==0) gtk_widget_show(w);
    else if (strcmp(arg, "false")!=0) {
	    fprintf(stderr, "*** window.c: ProcessPicture(): picture_show: `%s' - неверное значение\n", arg);
	    gtk_widget_destroy(box);
	    return NULL;
	    }
    label_callback(box, tag);
    return box;
    }

static GtkWidget * ProcessInput(struct tag_struct * tag, label_callback_func label_callback) {
//puts("* ProcessPicture()");
    GtkWidget * w=gtk_entry_new();
    char * arg=tag_getValue(tag, "text");
    if (arg!=NULL) gtk_entry_set_text(GTK_ENTRY(w), arg);
    ProcessFont(w, tag);
    label_callback(w, tag);
    return w;
    }


static GtkWidget * ProcessTag(struct tag_struct * tag, GtkWidget * window, label_callback_func label_callback, widget_callback_func widget_callback) {
    gboolean show;
    char * arg=tag_getValue(tag, "show");
    if (arg==NULL || strcmp(arg, "true")==0) show=TRUE;
    else if (strcmp(arg, "false")==0) show=FALSE;
    else {
	fprintf(stderr, "*** window.c: ProcessTag(): show: `%s' - неверное значение\n", arg);
	return NULL;
	}
    int width=-1;
    arg=tag_getValue(tag, "width");
    if (arg!=NULL) width=atoi(arg);

    int height=-1;
    arg=tag_getValue(tag, "height");
    if (arg!=NULL) height=atoi(arg);

    GtkWidget * w=NULL;
    if (strcmp(tag->name, "box")==0 && tag->type==TAG_BEGIN) w=ProcessBox(tag, window, label_callback, widget_callback);
    else if (strcmp(tag->name, "frame")==0 && (tag->type==TAG_BEGIN || tag->type==TAG_FULL)) w=ProcessFrame(tag, window, label_callback, widget_callback);
    else if (strcmp(tag->name, "text")==0 && (tag->type==TAG_BEGIN || tag->type==TAG_FULL)) w=ProcessText(tag, label_callback);
    else if (strcmp(tag->name, "button")==0 && (tag->type==TAG_BEGIN || tag->type==TAG_FULL)) w=ProcessButton(tag, window, label_callback, widget_callback);
    else if (strcmp(tag->name, "picture")==0 && tag->type==TAG_FULL) w=ProcessPicture(tag, label_callback);
    else if (strcmp(tag->name, "picture_button")==0 && (tag->type==TAG_BEGIN || tag->type==TAG_FULL)) w=ProcessPictureButton(tag, label_callback);
    else if (strcmp(tag->name, "input")==0 && tag->type==TAG_FULL) w=ProcessInput(tag, label_callback);
    else w=widget_callback(tag, window);
    if (w==NULL) {
	fprintf(stderr, "*** window.c: ProcessTag(): tag: `%s' - неверный формат\n", arg);
	return NULL;
	}
    if (width>0 || height>0) gtk_widget_set_size_request(w, width, height);
    if (show) gtk_widget_show(w);
    return w;
    }

static int SetWindowSize(GtkWindow * w, char * s) {
    int x=0;
    int y=0;
    int i;
    char v[16];
    int n=0;
    for (i=0; i<=sizeof(v); i++) {
	switch(*s) {
	case '0' ... '9': v[n]=*s; n++; break;
	case 'x': v[n]=0; x=atoi(v); n=0; break;
	case 0: v[n]=0; y=atoi(v); gtk_window_set_default_size(w, x, y); return 0;
	default: return -1;
	    }
	s++;
	}
    return -1;
    }

static GtkWidget * ProcessWindow(struct tag_struct * tag, label_callback_func label_callback, widget_callback_func widget_callback) {
    char * name="window";
    if (xml_nextItem(tag, NULL, 0)!=ITEM_TAG || tag->type!=TAG_HEADER) return NULL;
    if (strcmp(tag->name, "xml")) return NULL;
    char * encoding=tag_getValue(tag, "encoding");
    if (encoding!=NULL && strcmp(encoding, "UTF-8")) return NULL;
    tag_clear(tag);

    if (xml_nextItem(tag, NULL, 0)!=ITEM_TAG || tag->type!=TAG_BEGIN
	|| strcmp(tag->name, name)) return NULL;

    GtkWidget * window=gtk_window_new(GTK_WINDOW_TOPLEVEL);

    char * arg=tag_getValue(tag, "title");
    if (arg!=NULL) gtk_window_set_title(GTK_WINDOW(window), arg);

    arg=tag_getValue(tag, "modal");
    if (arg!=NULL) {
	if (strcmp(arg, "true")==0) gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	else if (strcmp(arg, "false")!=0) {
	    printf("*** window.c: window_process(): modal: `%s' - неверное значение\n", arg);
//	    return NULL;
	    }
	}

    arg=tag_getValue(tag, "border_width");
    if (arg!=NULL) {
	int n=atoi(arg);
	if (n>0) gtk_container_set_border_width(GTK_CONTAINER(window), n);
	}

    gboolean hide_cursor=FALSE;
    arg=tag_getValue(tag, "hide_cursor");
    if (arg!=NULL) {
	if (strcmp(arg, "true")==0) hide_cursor=TRUE;
	else if (strcmp(arg, "false")!=0) {
	    printf("*** window.c: window_process(): hide_cursor: `%s' - неверное значение\n", arg);
	    }
	}
    
    arg=tag_getValue(tag, "size");
    if (arg!=NULL && SetWindowSize(GTK_WINDOW(window), arg)<0) {
        printf("*** window.c: window_process(): size: `%s' - неверное значение\n", arg);
	}

    tag_clear(tag);
    GtkWidget * w=NULL;
    int item_type=xml_nextItem(tag, NULL, 0);
    if (item_type==ITEM_TAG && tag->type!=TAG_END) {
	w=ProcessTag(tag, window, label_callback, widget_callback);
	tag_clear(tag);
	item_type=xml_nextItem(tag, NULL, 0);
	}
    if (item_type==ITEM_TAG && strcmp(tag->name, name)==0 && tag->type==TAG_END) {
	gtk_container_add(GTK_CONTAINER(window), w);
	gtk_widget_realize(window);
#ifdef GDK_BLANK_CURSOR
        GdkCursor * cursor=gdk_cursor_new(GDK_BLANK_CURSOR);
#else
        GdkPixmap * zero_pixmap=gdk_bitmap_create_from_data(window->window, "\0\0\0\0\0\0\0\0", 1, 1);
        GdkColor zero_color={0,0,0,0};
        GdkCursor * cursor=gdk_cursor_new_from_pixmap(zero_pixmap,
				                      zero_pixmap,
				                      &zero_color,
				                      &zero_color,
				                      1,1);
#endif
        gdk_window_set_cursor(window->window, cursor);
	return window;
	}
    printf("tag name: <%s>\n", tag->name);
    gtk_widget_destroy(window);
    return NULL;
    }

GtkWidget * window_create(char * form, label_callback_func label_callback, widget_callback_func widget_callback) {
    if (label_callback==NULL) label_callback=(label_callback_func) dummy_label_callback;
    if (widget_callback==NULL) widget_callback=(widget_callback_func) dummy_widget_callback;
    struct tag_struct tag;
    tag_init(&tag);
    xml_open(form);
    GtkWidget * w=ProcessWindow(&tag, label_callback, widget_callback);
    xml_close();
    tag_destroy(&tag);
    return w;
    }
