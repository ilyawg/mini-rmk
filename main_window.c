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
#include <stdarg.h>
#include <gtk/gtk.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "global.h"
#include "conf.h"
#include "receipt.h"
#include "main_window.h"
#include "numeric.h"
#include "kkm.h"
#include "parser.h"
#include "vararg.h"

#define MAIN_WINDOW_TITLE	"mini-rmk v2 build"BUILD_CODE

enum {
    AGENT_ERROR=0,
    AGENT_WARNING,
    AGENT_REFRESH,
    AGENT_UNDEF
    };

#define AGENT_CONNECT_INTERVAL	10
#define AGENT_CHECK_INTERVAL	120

struct record_receipt * mw_current_record=NULL;

static int internal_agent_status=AGENT_UNDEF;
static gboolean internal_agent_event=FALSE;
static DBusConnection * internal_dbus_connection=NULL;
static DBusGProxy * internal_dbus_proxy=NULL;

static GdkPixbuf * internal_error_pb=NULL;
static GdkPixbuf * internal_warning_pb=NULL;
static GdkPixbuf * internal_refresh_pb=NULL;
static GtkWidget * internal_agent_picture=NULL;
static GtkWidget * internal_action_picture=NULL;
static GtkWidget * internal_cash_picture=NULL;
static GtkWidget * internal_stop_picture=NULL;
static GtkWidget * internal_access_picture=NULL;
static GtkWidget * internal_lockdb_picture=NULL;
static gboolean internal_stop_flag=FALSE;
static gboolean internal_lockdb_flag=FALSE;

enum {
    NUM_COLUMN,
    NAME_COLUMN,
    PRICE_COLUMN,
    QUANTITY_COLUMN,
    SUMM_COLUMN,
    N_COLUMNS
    };

static GtkWidget * internal_doc_text=NULL;
static GtkWidget * internal_session_text=NULL;
static GtkWidget * internal_total_text=NULL;
static GtkWidget * internal_main_text=NULL;
static GtkWidget * internal_code_text=NULL;
static GtkWidget * internal_price_text=NULL;
static GtkWidget * internal_quantity_text=NULL;
static GtkWidget * internal_summ_text=NULL;
static GtkWidget * internal_kkm_text=NULL;
static GtkWidget * internal_clock_text=NULL;

static GtkWidget * internal_receipt_tree=NULL;
static GtkListStore * internal_receipt_table=NULL;

#define SCALE_ARRAY_SIZE	10
static struct {
    int length;
    int value;
    } internal_font_scale[SCALE_ARRAY_SIZE];

static int internal_main_string_font_size=50;

/*****************************************************************************/
#define INPUT_BUFSIZE	256
static int internal_input_buf[INPUT_BUFSIZE];
static int internal_input_count=0;
static int internal_input_sid=0;
gboolean global_access_flag=FALSE;

/****************************************************************************/
static gboolean delete_event(GtkWidget * widget, GdkEvent * event, gpointer data) {
    return FALSE;
    }

static void destroy(GtkWidget * widget, gpointer data) {
    gtk_main_quit();
    }

static void CardEvent(char * code) {
//printf("*** card code: <%s>\n", code);
//printf("\tpattern: <%s>\n", global_conf_card_code);
    size_t len=strlen(code);
    if (len==strlen(global_conf_card_code)) {
	char * ptr=global_conf_card_code;
	int i;
	gboolean f=TRUE;
	for (i=0; i<len; i++) {
	    if (*ptr!='*' && *code!=*ptr) {
		f=FALSE;
		break;
		}
	    ptr++; code++;
	    }
	if (f) {
	    if (!global_access_flag) {
		if (internal_access_picture!=NULL) gtk_widget_show(internal_access_picture);
		global_access_flag=TRUE;
		}
    	    return;
	    }
	}
    if (internal_access_picture!=NULL) gtk_widget_hide(internal_access_picture);
    global_access_flag=FALSE;
    }

void mw_accessDisable(void) {
    if (!global_access_flag) return;
    if (internal_access_picture!=NULL) gtk_widget_hide(internal_access_picture);
    global_access_flag=FALSE;
    }

static gboolean InputEvent(void) {
#define PREFIX_COUNT	1
    int prefix[]={0x2F};
#define SUFFIX_COUNT	3
    int suffix[]={0x32, 0x3D, 0x24};
    char str[21]; bzero(str, sizeof(str));
    char * trg=str;
    int count=internal_input_count;
    internal_input_count=0;
    if (count<5) return FALSE;
puts("card process...");
    int i=0;
    int mode=0;
    int ptr=0;
    do {
	if (mode==1 && count>SUFFIX_COUNT && memcmp(internal_input_buf+ptr, suffix, sizeof(suffix))==0) {
	    CardEvent(str);
	    ptr+=SUFFIX_COUNT;
	    bzero(str, sizeof(str)); trg=str; mode=0;
	    }
	else if (count>=PREFIX_COUNT && memcmp(internal_input_buf+ptr, prefix, sizeof(prefix))==0) {
	    ptr+=PREFIX_COUNT;
	    mode=1;
	    }
	else if (mode==1) {
	    switch (internal_input_buf[ptr]) {
	    case 0x0a: *trg='1'; trg++; break;
	    case 0x0b: *trg='2'; trg++; break;
	    case 0x0c: *trg='3'; trg++; break;
	    case 0x0d: *trg='4'; trg++; break;
	    case 0x0e: *trg='5'; trg++; break;
	    case 0x0f: *trg='6'; trg++; break;
	    case 0x10: *trg='7'; trg++; break;
	    case 0x11: *trg='8'; trg++; break;
	    case 0x12: *trg='9'; trg++; break;
	    case 0x13: *trg='0'; trg++; break;
		}
	    ptr++;
	    }
	else ptr++;
	} while (ptr<count);
    return FALSE;
    }

static gboolean key_event(GtkWidget * widget, GdkEventKey * event, gpointer data) {
//puts("* key_press_event *");
    if (internal_input_sid>0) {
	g_source_remove(internal_input_sid);
	internal_input_sid=0;
	}
    if (internal_input_count<INPUT_BUFSIZE) {
	internal_input_buf[internal_input_count]=event->hardware_keycode;
	internal_input_count++;
	}
    internal_input_sid=g_timeout_add(50, (GSourceFunc)InputEvent, NULL);
//printf("sid: %d\n", internal_input_sid);
    return TRUE;
    }

static void SetFontSize(GtkWidget * w, int v) {
    PangoFontDescription *font_desc=pango_font_description_copy(gtk_widget_get_style(w)->font_desc);
    pango_font_description_set_size(font_desc, v*PANGO_SCALE);
    gtk_widget_modify_font(w, font_desc);
    pango_font_description_free(font_desc);
    }

static void StornoFunc(GtkTreeView *tree,
	    	      GtkCellRenderer *cell,
	              GtkTreeModel *model,
	              GtkTreeIter *iter,
	              gpointer data) {
//printf("data_func: %u\n",data);
    gint num=0;
    gtk_tree_model_get(GTK_TREE_MODEL(internal_receipt_table), iter, NUM_COLUMN, &num, -1);
    struct record_receipt * rc=receipt_getRecordByNum(num);
    gboolean value=FALSE;
    if (rc!=NULL && num_cmp0(&rc->quantity)==0) value=TRUE;
    g_object_set(cell, "strikethrough", value, NULL);
    }

void mw_showSession(int state, int num) {
    char str[32];
    if (internal_session_text==NULL) return;
    switch (state) {
    	    case SESSION_OPEN: 
	sprintf(str,"Смена #%d",num);
        gtk_label_set_text(GTK_LABEL(internal_session_text),str);
	break;	    
	    case SESSION_CLOSED:
        gtk_label_set_text(GTK_LABEL(internal_session_text),"Смена закрыта");
	break;
	    case SESSION_ERROR:
        gtk_label_set_text(GTK_LABEL(internal_session_text),"ОШИБКА");
	break;
	    case SESSION_CLEAR:
        gtk_label_set_text(GTK_LABEL(internal_session_text),"");
	break;
    	    default:
        gtk_label_set_text(GTK_LABEL(internal_session_text),"НЕ ОПРЕДЕЛЕН");
	break;
	}
    }

void mw_showDocState(int doc_state, int doc_num) {
    char *text;
    if (internal_doc_text==NULL) return;
    switch (doc_state) {
	case DOC_SALE: text="ПРОДАЖА"; break;
        case DOC_RETURN: text="ВОЗВРАТ"; break;
        case DOC_CASHIN: text="ВНЕСЕНИЕ"; break;
        case DOC_CASHOUT: text="ВЫПЛАТА"; break;
	case DOC_CLOSED: text="Чек закрыт"; break;
	case DOC_CANCEL: text="АННУЛИРОВАН"; break;
	case DOC_CLEAR: text=""; break;
        default: text="ОШИБКА"; break;
	}
    if (doc_num<0 || doc_state==DOC_CLOSED || doc_state==DOC_CLEAR) 
	gtk_label_set_text(GTK_LABEL(internal_doc_text),text);
    else {
        char str[64];
	sprintf(str,"%u / %s",doc_num,text);
	gtk_label_set_text(GTK_LABEL(internal_doc_text),str);
	}
    }

void mw_showCode(int value) {
//puts("* mw_ShowCode()");
    char str[32];
    if (internal_code_text!=NULL) {
        sprintf(str,"%u",value);
        gtk_label_set_text(GTK_LABEL(internal_code_text), str);
	}
    }

void mw_showQuantity(num_t * v) {
    char str[16];
    if (internal_quantity_text!=NULL) {
        num_toString(str, v);
	gtk_label_set_text(GTK_LABEL(internal_quantity_text),str);
	}
    }

void mw_showPrice(num_t * v) {
    char str[16];
    if (internal_price_text!=NULL) {
        num_toString(str, v);
	gtk_label_set_text(GTK_LABEL(internal_price_text),str);
	}
    }

void mw_showSum(num_t * v) {
    char str[16];
    if (internal_summ_text!=NULL) {
        num_toString(str, v);
	gtk_label_set_text(GTK_LABEL(internal_summ_text),str);
	}
    }

void mw_showTotal(num_t * v) {
    char str[16];
    if (internal_total_text!=NULL) {
        num_toString(str, v);
	gtk_label_set_text(GTK_LABEL(internal_total_text), str);
	}
    }

static gboolean show_time(void) {
    static int dot=0;
    time_t t=time(NULL);
    struct tm *ts=localtime(&t);
    char str[7];
    strftime(str,sizeof(str),(dot==0)?"%02H %02M":"%02H:%02M",ts);
    gtk_label_set_text(GTK_LABEL(internal_clock_text),str);
    dot=!dot;
    return TRUE;
    }

/****************************** agent tools *********************************/
static void AgentShow(const char * status) {
    if (strcmp(status, "UNAVAILABLE")==0) {
        if (internal_agent_status!=AGENT_WARNING) {
	    internal_agent_status=AGENT_WARNING;
	    gtk_image_set_from_pixbuf(GTK_IMAGE(internal_agent_picture), internal_warning_pb);
	    }
	}
    else if (strcmp(status, "LOCK")==0) {
	if (internal_lockdb_picture!=NULL) gtk_widget_show(internal_lockdb_picture);
	internal_lockdb_flag=TRUE;
        if (internal_agent_status!=AGENT_REFRESH) {
	    internal_agent_status=AGENT_REFRESH;
	    gtk_image_set_from_pixbuf(GTK_IMAGE(internal_agent_picture), internal_refresh_pb);
    	    }
	}
    else if (strcmp(status, "ACTION")==0) {
	if (internal_agent_status!=AGENT_REFRESH) {
	    internal_agent_status=AGENT_REFRESH;
	    gtk_image_set_from_pixbuf(GTK_IMAGE(internal_agent_picture), internal_refresh_pb);
	    }
	if (internal_lockdb_flag && internal_lockdb_picture!=NULL)
	    gtk_widget_hide(internal_lockdb_picture);
	internal_lockdb_flag=FALSE;
	}
    else if (strcmp(status, "DONE")==0) {
	if (internal_agent_status!=AGENT_REFRESH) {
	    internal_agent_status=AGENT_REFRESH;
	    gtk_image_set_from_pixbuf(GTK_IMAGE(internal_agent_picture), internal_refresh_pb);
	    }
	}
    else {
	gtk_widget_set_sensitive(internal_agent_picture, FALSE);
	return;
	}
    gtk_widget_set_sensitive(internal_agent_picture, TRUE);
    }

static gboolean AgentHandler(DBusGProxy *proxy, const char *status, gpointer user_data) {
    internal_agent_event=TRUE;
    AgentShow(status);
    return TRUE;
    }

static gboolean AgentStatus(gpointer arg) {
    DBusGProxy *proxy = arg;
    char * status=NULL;
    GError * error = NULL;
    if (internal_agent_event) internal_agent_event=FALSE;
    else {
	if (dbus_g_proxy_call(proxy, "status", &error, G_TYPE_INVALID,
		              G_TYPE_STRING, &status, G_TYPE_INVALID)) {
    	    if (status!=NULL) {
		AgentShow(status);
		dbus_free(status);
		}
	    }
	else {
#ifdef DEBUG
	    fprintf(stderr, "*** AgentStatus() DBUS error: %s\n", error->message);
#endif	
	    gtk_image_set_from_pixbuf(GTK_IMAGE(internal_agent_picture), internal_error_pb);
	    gtk_widget_set_sensitive(internal_agent_picture, TRUE);
	    internal_agent_status=AGENT_ERROR;
	    }
	}
    return TRUE;
    }

struct signal_args {
    int counter;
    char type[];
    };

static gboolean AgentSignal(gpointer data) {
    struct signal_args * sig=data;
    char * str=sig->type;
    DBusMessage * msg=dbus_message_new_signal("/", "mrmk.agent", "notify");
    if (msg==NULL) log_puts(0, "ОШИБКА D-bus: не удалось создать сообщение");
    else {
	dbus_message_append_args(msg,
				 DBUS_TYPE_STRING, &str,
				 DBUS_TYPE_UINT32, &sig->counter,
				 DBUS_TYPE_INVALID);
	dbus_message_set_destination(msg, "mrmk.agent");
        dbus_connection_send(internal_dbus_connection, msg, NULL);
        dbus_connection_flush(internal_dbus_connection);
        dbus_message_unref(msg);
        }
    free(data);
    return FALSE;
    }

void mw_agentNotify(char * type, int counter) {
    struct signal_args * sig=malloc(sizeof(struct signal_args)+strlen(type)+1);
    strcpy(sig->type, type);
    sig->counter=counter;
    g_idle_add(AgentSignal, sig);
    }

static gboolean AgentCall(gpointer data) {
    const char * method=data;
    dbus_g_proxy_call_no_reply(internal_dbus_proxy, method, G_TYPE_INVALID);
    return FALSE;
    }
        
void mw_agentLock(gboolean v) {
    if (v) g_idle_add(AgentCall, "lock");
    else g_idle_add(AgentCall, "unlock");
    }
/****************************************************************************/

static int FindRecord(struct record_receipt * rec, GtkTreePath * path) {
    if (gtk_tree_path_compare(rec->path, path)!=0) return 0;
printf("\tid: %d\n", rec->id);
    if (mw_current_record->num!=rec->num) {
	mw_current_record=rec;
	mw_showText("");
	mw_showCode(rec->code);
	mw_showQuantity(&rec->quantity);
	mw_showPrice(&rec->price);
	mw_showSum(&rec->summ);
	}
    return -1;
    }

static gboolean CursorChanged(void) {
//puts("* MoveCursor()");
    GtkTreePath * path;
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(internal_receipt_tree), &path, NULL);
    receipt_processAll((receipt_callback)FindRecord, path);
    gtk_tree_path_free(path);
    }

/************************************************************************/
static GtkWidget * ProcessTag(struct tag_struct * tag);

static GtkWidget * ProcessBox(struct tag_struct * tag) {
//puts("* ProcessBox()");
    gboolean homogeneous;
    char * arg=tag_getValue(tag, "homogeneous");
    if (arg==NULL || strcmp(arg, "true")==0) homogeneous=TRUE;
    else if (strcmp(arg, "false")==0) homogeneous=FALSE;
    else return NULL;					//FIXME!!!

    int spacing=0;
    arg=tag_getValue(tag, "spacing");
    if (arg!=NULL) spacing=atoi(arg);

    int width=-1;
    arg=tag_getValue(tag, "width");
    if (arg!=NULL) width=atoi(arg);

    int height=-1;
    arg=tag_getValue(tag, "height");
    if (arg!=NULL) height=atoi(arg);
    
    arg=tag_getValue(tag, "type");
    if (arg==NULL) return NULL;
    GtkWidget * box=NULL;
    if (strcmp(arg, "vertical")==0) box=gtk_vbox_new(homogeneous, spacing);
    else if (strcmp(arg, "horizontal")==0) box=gtk_hbox_new(homogeneous, spacing);
    else return NULL;
    if (width>0 || height>0) gtk_widget_set_size_request(box, width, height);
    tag_clear(tag);
    int item_type=xml_nextItem(tag, NULL, 0);
    while (item_type==ITEM_TAG) {
        if (strcmp(tag->name, "box")==0 && tag->type==TAG_END) {
	    gtk_widget_show(box);
	    return box;
	    }

	gboolean expand;
	arg=tag_getValue(tag, "expand");
	if (arg==NULL || strcmp(arg, "true")==0) expand=TRUE;
	else if (strcmp(arg, "false")==0) expand=FALSE;
	else return NULL;					//FIXME!!!

	gboolean fill;
	arg=tag_getValue(tag, "fill");
	if (arg==NULL || strcmp(arg, "true")==0) fill=TRUE;
	else if (strcmp(arg, "false")==0) fill=FALSE;
	else return NULL;					//FIXME!!!

	int padding=0;
	arg=tag_getValue(tag, "padding");
	if (arg!=NULL) padding=atoi(arg);

	GtkWidget * w=ProcessTag(tag);
	if (w==NULL) return NULL;
        gtk_box_pack_start(GTK_BOX(box), w, expand, fill, padding);
        tag_clear(tag);
	item_type=xml_nextItem(tag, NULL, 0);
	}
    return NULL;
    }

static GtkWidget * ProcessFrame(struct tag_struct * tag) {
//puts("* ProcessFrame()");
    char * text=tag_getValue(tag, "title");
    GtkWidget * frame=gtk_frame_new(text);
    tag_clear(tag);
    int item_type=xml_nextItem(tag, NULL, 0);
    if (item_type==ITEM_TAG && tag->type!=TAG_END) {
	GtkWidget * w=ProcessTag(tag);
	if (w==NULL) return NULL;
        gtk_container_add(GTK_CONTAINER(frame), w);
        tag_clear(tag);
	item_type=xml_nextItem(tag, NULL, 0);
	}
    if (item_type==ITEM_TAG && strcmp(tag->name, "frame")==0 && tag->type==TAG_END) {
        gtk_widget_show(frame);
        return frame;
        }
    return NULL;
    }

static GtkWidget * ProcessText(struct tag_struct * tag) {
//puts("* ProcessText()");
    enum {
	UNDEF_LABEL,
	MAIN_LABEL,
	KKM_NUM_LABEL,
	DOC_STATE_LABEL,
	SESSION_STATE_LABEL,
	CODE_LABEL,
	QUANTITY_LABEL,
	PRICE_LABEL,
	SUMM_LABEL,
	TOTAL_LABEL,
	CLOCK_LABEL
	};
    enum {
	ALIGN_LEFT=0,
	ALIGN_TOP,
	ALIGN_CENTER,
	ALIGN_RIGHT,
	ALIGN_BOTTOM,
	ALIGN_DEFAULT
	};
    gfloat align_values[]={0, 0, 0.5, 1, 1, 0.5};
    int label;
    char * arg=tag_getValue(tag, "label");
    if (arg==NULL) label=UNDEF_LABEL;
    else if (strcmp(arg, "kkm_num")==0) label=KKM_NUM_LABEL;
    else if (strcmp(arg, "doc_state")==0) label=DOC_STATE_LABEL;
    else if (strcmp(arg, "session_state")==0) label=SESSION_STATE_LABEL;
    else if (strcmp(arg, "code")==0) label=CODE_LABEL;
    else if (strcmp(arg, "quantity")==0) label=QUANTITY_LABEL;
    else if (strcmp(arg, "price")==0) label=PRICE_LABEL;
    else if (strcmp(arg, "summ")==0) label=SUMM_LABEL;
    else if (strcmp(arg, "total")==0) label=TOTAL_LABEL;
    else if (strcmp(arg, "main_string")==0) label=MAIN_LABEL;
    else if (strcmp(arg, "clock")==0) label=CLOCK_LABEL;
    else {
	printf("*** ProcessText(): label: <%s> - неверное значение", arg);
	return NULL;
	}

    int font_size=-1;
    arg=tag_getValue(tag, "font_size");
    if (arg!=NULL) {
	font_size=atoi(arg);
	if (label==MAIN_LABEL) internal_main_string_font_size=font_size;
	}

    PangoFontDescription* font=NULL;
    arg=tag_getValue(tag, "font");
    if (arg!=NULL) font=pango_font_description_from_string(arg);

    int size=-1;
    arg=tag_getValue(tag, "size");
    if (arg!=NULL) size=atoi(arg);

    int xalign;
    arg=tag_getValue(tag, "xalign");
    if (arg==NULL) xalign=ALIGN_DEFAULT;
    else if (strcmp(arg, "left")==0) xalign=ALIGN_LEFT;
    else if (strcmp(arg, "center")==0) xalign=ALIGN_CENTER;
    else if (strcmp(arg, "right")==0) xalign=ALIGN_RIGHT;
    else {
	printf("*** ProcessText(): xalign: `%s' - неверное значение", arg);
	return NULL;
	}

    int yalign;
    arg=tag_getValue(tag, "yalign");
    if (arg==NULL) yalign=ALIGN_DEFAULT;
    else if (strcmp(arg, "top")==0) yalign=ALIGN_TOP;
    else if (strcmp(arg, "center")==0) yalign=ALIGN_CENTER;
    else if (strcmp(arg, "bottom")==0) yalign=ALIGN_BOTTOM;
    else {
	printf("*** ProcessText(): yalign: `%s' - неверное значение", arg);
        return NULL;
	}

    char text[256]; bzero(text, sizeof(text));
    char data[256];
    int n=0;
    int item_type=ITEM_TAG;
    if (tag->type==TAG_BEGIN) {
	while (n<SCALE_ARRAY_SIZE) {
	    tag_clear(tag);
	    bzero(data, sizeof(data));
	    item_type=xml_nextItem(tag, data, sizeof(data));
	    if (item_type==ITEM_TEXT_STRING) strcpy(text, data);
	    if (item_type==ITEM_TAG) {
		if (label!=MAIN_LABEL || tag->type!=TAG_FULL) break;
		arg=tag_getValue(tag, "length");
		int v=0;
		if (arg!=NULL) v=atoi(arg);
		if (v<1) break;
		if (n>0 && internal_font_scale[n-1].length>=v) break;
		internal_font_scale[n].length=v;
		
		arg=tag_getValue(tag, "font_size");
		v=0;
		if (arg!=NULL) v=atoi(arg);
		if (v<1) break;
		internal_font_scale[n].value=v;
		n++;
		}
	    }
	}	
    if (item_type!=ITEM_TAG || strcmp(tag->name, "text")!=0) {
        if (font!=NULL) pango_font_description_free(font);
        return NULL;
        }
    GtkWidget * w=gtk_label_new(text);
    if (xalign!=ALIGN_DEFAULT || yalign!=ALIGN_DEFAULT)
	gtk_misc_set_alignment(GTK_MISC(w), align_values[xalign], align_values[yalign]);
    if (font_size>0) SetFontSize(w, font_size);
    if (font!=NULL) {
	gtk_widget_modify_font(w, font);
	pango_font_description_free(font);
	}
    if (size>0) gtk_widget_set_size_request(w, -1, size);
    switch (label) {
    case KKM_NUM_LABEL: internal_kkm_text=w; break;
    case DOC_STATE_LABEL: internal_doc_text=w; break;
    case SESSION_STATE_LABEL: internal_session_text=w; break;
    case CODE_LABEL: internal_code_text=w; break;
    case QUANTITY_LABEL: internal_quantity_text=w; break;
    case PRICE_LABEL: internal_price_text=w; break;
    case SUMM_LABEL: internal_summ_text=w; break;
    case TOTAL_LABEL: internal_total_text=w; break;
    case MAIN_LABEL:
        gtk_label_set_ellipsize(GTK_LABEL(w), PANGO_ELLIPSIZE_END);
        internal_main_text=w;
        break;
    case CLOCK_LABEL:
        gtk_misc_set_alignment(GTK_MISC(w), 1, 0.5);
        internal_clock_text=w;
        break;
        }
    gtk_widget_show(w);
    return w;
    }

static GtkWidget * ProcessPicture(struct tag_struct * tag) {
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

    arg=tag_getValue(tag, "label");
    if (arg==NULL) return w;
    if (strcmp(arg, "kkm")==0) {
	internal_cash_picture=w;
	return w;
	}
    if (strcmp(arg, "action")==0) {
	internal_action_picture=w;
	return w;
	}
    if (strcmp(arg, "stop")==0) {
	internal_stop_picture=w;
	return w;
	}
    if (strcmp(arg, "access")==0) {
	internal_access_picture=w;
	w=gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(w), internal_access_picture);
	g_signal_connect(G_OBJECT(w), "button_press_event", G_CALLBACK(mw_accessDisable), NULL);
	gtk_widget_show(w);
	return w;
	}
    if (strcmp(arg, "lockdb")==0) {
	internal_lockdb_picture=w;
	return w;
	}
    return NULL;
    }    

static GtkWidget * ProcessAgentPicture(struct tag_struct * tag) {
//puts("* ProcessAgentPicture()");
    char path[1024];
    strcpy(path, PIXMAP_DIR);
    strcat(path, "/");
    char * name=strchr(path, 0);
    GtkWidget * w=gtk_image_new();

    char * arg=tag_getValue(tag, "error");
    if (arg!=NULL) {
	strcpy(name, arg);
	internal_error_pb=gdk_pixbuf_new_from_file(path, NULL);
	}

    arg=tag_getValue(tag, "warning");
    if (arg!=NULL) {
	strcpy(name, arg);
	internal_warning_pb=gdk_pixbuf_new_from_file(path, NULL);
	}

    arg=tag_getValue(tag, "refresh");
    if (arg!=NULL) {
	strcpy(name, arg);
	internal_refresh_pb=gdk_pixbuf_new_from_file(path, NULL);
	}
    internal_agent_picture=w;
    return w;
    }    

static GtkWidget * ProcessReceiptTable(struct tag_struct * tag) {
//puts("* ProcessReceiptTable()");
    GtkWidget * w=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(w), GTK_SHADOW_IN);
    
    internal_receipt_table=gtk_list_store_new(N_COLUMNS,
	                                      G_TYPE_UINT,
	                                      G_TYPE_STRING,
	        		              G_TYPE_STRING,
		                              G_TYPE_STRING,
			                      G_TYPE_STRING);

    internal_receipt_tree=gtk_tree_view_new_with_model(GTK_TREE_MODEL(internal_receipt_table));
    GtkCellRenderer * renderer=gtk_cell_renderer_text_new();
    GTK_CELL_RENDERER(renderer)->xalign=0;
    GtkTreeViewColumn * column=gtk_tree_view_column_new_with_attributes("№", renderer, "text", NUM_COLUMN, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer, (GtkTreeCellDataFunc)StornoFunc, NULL, NULL);    
    gtk_tree_view_column_set_min_width(column, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(internal_receipt_tree), column);

    column=gtk_tree_view_column_new_with_attributes("Наименование товара", renderer, "text", NAME_COLUMN, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(internal_receipt_tree), column);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);

    renderer=gtk_cell_renderer_text_new();
    GTK_CELL_RENDERER(renderer)->xalign=1;
    column=gtk_tree_view_column_new_with_attributes("Цена", renderer, "text", PRICE_COLUMN, NULL);
//    gtk_tree_view_column_set_cell_data_func(column, renderer, (GtkTreeCellDataFunc)StornoFunc, NULL, NULL);    
    gtk_tree_view_column_set_min_width(column, 100);
    gtk_tree_view_append_column(GTK_TREE_VIEW(internal_receipt_tree), column);
    gtk_tree_view_column_set_alignment(column, 0.5);
    
    column=gtk_tree_view_column_new_with_attributes("Количество", renderer, "text", QUANTITY_COLUMN, NULL);
    gtk_tree_view_column_set_min_width(column, 100);
    gtk_tree_view_append_column(GTK_TREE_VIEW(internal_receipt_tree), column);
    gtk_tree_view_column_set_alignment(column, 0.5);

    column=gtk_tree_view_column_new_with_attributes("Стоимость", renderer, "text", SUMM_COLUMN, NULL);
    gtk_tree_view_column_set_min_width(column, 100);
    gtk_tree_view_append_column(GTK_TREE_VIEW(internal_receipt_tree), column);
    gtk_tree_view_column_set_alignment(column, 0.5);

    char * arg=tag_getValue(tag, "font_size");
    if (arg!=NULL) {
	int font_size=atoi(arg);
	SetFontSize(internal_receipt_tree, font_size);
	}

    gtk_container_add(GTK_CONTAINER(w), internal_receipt_tree);
    g_signal_connect(G_OBJECT(internal_receipt_tree), "cursor_changed", G_CALLBACK(CursorChanged), NULL);
    gtk_widget_show(internal_receipt_tree);
    gtk_widget_show(w);
    return w;
    }
/*****************************************************************************/

static GtkWidget * ProcessTag(struct tag_struct * tag) {
    if (strcmp(tag->name, "box")==0 && tag->type==TAG_BEGIN) return ProcessBox(tag);
    if (strcmp(tag->name, "frame")==0 && tag->type==TAG_BEGIN) return ProcessFrame(tag);
    if (strcmp(tag->name, "text")==0 && (tag->type==TAG_BEGIN || tag->type==TAG_FULL)) return ProcessText(tag);
    if (strcmp(tag->name, "picture")==0 && tag->type==TAG_FULL) return ProcessPicture(tag);
    if (strcmp(tag->name, "agent_picture")==0 && tag->type==TAG_FULL) return ProcessAgentPicture(tag);
    if (strcmp(tag->name, "receipt_table")==0 && tag->type==TAG_FULL) return ProcessReceiptTable(tag);
printf("*** ProcessTag() fail: <%s>\n", tag->name);
    return NULL;
    }

static GtkWidget * ProcessWindow(struct tag_struct * tag) {
    if (xml_nextItem(tag, NULL, 0)!=ITEM_TAG || tag->type!=TAG_HEADER) return NULL;
    if (strcmp(tag->name, "xml")) return NULL;
    char * encoding=tag_getValue(tag, "encoding");
    if (encoding!=NULL && strcmp(encoding, "UTF-8")) return NULL;
    tag_clear(tag);
    if (xml_nextItem(tag, NULL, 0)!=ITEM_TAG || tag->type!=TAG_BEGIN
	|| strcmp(tag->name, "main_window")) return NULL;
    GtkWidget * w=NULL;
    tag_clear(tag);
    int item_type=xml_nextItem(tag, NULL, 0);
    if (item_type==ITEM_TAG && tag->type!=TAG_END) {
	w=ProcessTag(tag);
	tag_clear(tag);
	item_type=xml_nextItem(tag, NULL, 0);
	}
    if (item_type==ITEM_TAG && strcmp(tag->name, "main_window")==0 && tag->type==TAG_END) return w;
    return NULL;
    }

int mw_init(void) {
    struct tag_struct tag;
    bzero(internal_font_scale, sizeof(internal_font_scale));
    tag_init(&tag);
    xml_open("/usr/share/mini-rmk/main_window.xml");
    GtkWidget * w=ProcessWindow(&tag);
    xml_close();
    tag_destroy(&tag);
    if (w==NULL) return -1;

    GtkWidget * window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), MAIN_WINDOW_TITLE);
    gtk_window_fullscreen(GTK_WINDOW(window));
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL);
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
    g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(key_event), NULL);
    gtk_container_add(GTK_CONTAINER(window), w);
    gtk_widget_show(w);
    gtk_widget_show(window);
#ifdef GDK_BLANK_CURSOR
    GdkCursor * cursor=gdk_cursor_new(GDK_BLANK_CURSOR);
#else
    GdkScreen * screen=gdk_display_get_default_screen(gdk_display_get_default());
    GdkPixmap * zero_pixmap=gdk_bitmap_create_from_data(gdk_screen_get_root_window(screen),
					    "\0\0\0\0\0\0\0\0", 1, 1);
    GdkColor zero_color={0,0,0,0};
    GdkCursor * cursor=gdk_cursor_new_from_pixmap(zero_pixmap,
				                  zero_pixmap,
				                  &zero_color,
				                  &zero_color,
				                  1,1);
#endif
    gdk_window_set_cursor(window->window, cursor);

    GError * error=NULL;
    DBusGConnection * bus=dbus_g_bus_get(DBUS_BUS_SESSION, &error);
    if (!bus) log_printf(0, "DBUS: Couldn't connect to session bus: %s", error->message);
    else {
	internal_dbus_connection=dbus_g_connection_get_connection(bus);
	DBusGProxy *proxy=dbus_g_proxy_new_for_name (bus, "mrmk.agent", "/", "mrmk.agent");
	if (proxy==NULL) log_printf(0,"DBUS: Failed to get name owner: %s", error->message);
	else {
	    internal_dbus_proxy=proxy;
	    dbus_g_proxy_add_signal (proxy, "status", G_TYPE_STRING, G_TYPE_INVALID);
	    dbus_g_proxy_connect_signal (proxy, "status", G_CALLBACK (AgentHandler), NULL, NULL);
//	    gtk_image_set_from_pixbuf(GTK_IMAGE(internal_agent_picture), internal_error_pb);
//	    gtk_widget_set_sensitive(internal_agent_picture, FALSE);
	    AgentStatus(proxy);
	    gtk_widget_show(internal_agent_picture);
	    g_timeout_add (60000, AgentStatus, proxy);
	    }
	}
    g_timeout_add(500,(GSourceFunc)show_time,NULL);
    return 0;
    }

static size_t wstrlen(char *str) {
    size_t count=0;
    while (*str!=0) {
	size_t len=mblen(str,MB_CUR_MAX);
	if (len<=0) break;
	count++;
	str+=len;
	}
//printf("wstrlen=%d\n",count);
    return count;
    }
    
void mw_showText(char *text) {
//printf("* mw_ShowText(): <%s>\n",text);
    size_t font=internal_main_string_font_size;
    size_t len=wstrlen(text);
    int n;
    for (n=0; n<SCALE_ARRAY_SIZE; n++) {
	if (internal_font_scale[n].length==0) break;
	if (len>internal_font_scale[n].length) font=internal_font_scale[n].value;
	}
printf("text: %d, font: %d\n", len, font);

    if (internal_code_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_code_text), "");
    if (internal_price_text!=NULL)
        gtk_label_set_text(GTK_LABEL(internal_price_text), "");
    if (internal_quantity_text!=NULL)
        gtk_label_set_text(GTK_LABEL(internal_quantity_text), "");
    if (internal_summ_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_summ_text), "");
    if (internal_main_text!=NULL) {
	SetFontSize(internal_main_text, font);
	gtk_label_set_text(GTK_LABEL(internal_main_text), text);
	}
    }

void mw_clearText(void) {
    if (internal_code_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_code_text),NULL);
    if (internal_price_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_price_text),NULL);
    if (internal_quantity_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_quantity_text),NULL);
    if (internal_summ_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_summ_text),NULL);
    if (internal_main_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_main_text),NULL);
    }

void mw_clearTable(void) {
    mw_current_record=NULL;
    gtk_list_store_clear(internal_receipt_table);
    }

gboolean mw_wheelOn(void) {
    if (internal_action_picture!=NULL)
	gtk_widget_show(internal_action_picture);
    return FALSE;
    }

gboolean mw_wheelOff(void) {
    if (internal_action_picture!=NULL)
	gtk_widget_hide(internal_action_picture);
    return FALSE;
    }

void mw_kkmConnect_on(char * kkm) {
    if (internal_kkm_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_kkm_text), kkm);
    if (internal_cash_picture!=NULL)
	gtk_widget_show(internal_cash_picture);
    }

void mw_kkmConnect_off(void) {
    if (internal_kkm_text!=NULL)
	gtk_label_set_text(GTK_LABEL(internal_kkm_text), "");
    if (internal_cash_picture!=NULL)
	gtk_widget_hide(internal_cash_picture);
    }

void mw_stopOn(void) {
    if (!internal_stop_flag) {
	if (internal_stop_picture!=NULL)
	    gtk_widget_show(internal_stop_picture);
	internal_stop_flag=TRUE;
	}
    }

void mw_stopOff(void) {
    if (internal_stop_flag) {
	if (internal_stop_picture!=NULL)
	    gtk_widget_hide(internal_stop_picture);
	internal_stop_flag=FALSE;
	}
    }

GtkTreePath * mw_newRecord(void) {
    GtkTreeIter iter;
    gtk_list_store_append(internal_receipt_table, &iter);
    return gtk_tree_model_get_path(GTK_TREE_MODEL(internal_receipt_table), &iter);
    }

/*****************************************************************************/
void mw_setCursor(struct record_receipt * r) {
    if (r!=NULL) mw_current_record=r;
    if (mw_current_record!=NULL) 
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(internal_receipt_tree), 
			         mw_current_record->path,
				 NULL,
				 FALSE);
    }

void mw_addRecord(struct record_receipt *rc) {
    GtkTreeIter iter;
    gboolean r=gtk_tree_model_get_iter(GTK_TREE_MODEL(internal_receipt_table), &iter, rc->path);
    char price[16]; num_toString(price, &rc->price);
    char quantity[16]; num_toString(quantity, &rc->quantity);
    char summ[16]; num_toString(summ, &rc->summ);
    gtk_list_store_set(internal_receipt_table, &iter,
			NUM_COLUMN, rc->num,
			NAME_COLUMN, rc->longtitle,
			PRICE_COLUMN, price,
			QUANTITY_COLUMN, quantity,
			SUMM_COLUMN, summ,
			-1);
    }

void mw_showRecord(struct record_receipt *rc) {
    mw_addRecord(rc);
    mw_setCursor(rc);
    }
