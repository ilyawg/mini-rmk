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
#include <sys/types.h>
#include <string.h>
#include <gtk/gtk.h>

#include "vararg.h"
#include "conf.h"
#include "commands.h"
#include "registers.h"
#include "main_window.h"
#include "receipt.h"
#include "kkm.h"
#include "localdb.h"
#include "global.h"
#include "recordset.h"

#define CONNECT_INTERVAL 20

extern int global_doc_num;
extern int global_session_num;
extern int global_sale_num;
extern int global_return_num;
extern int global_doc_state;
extern int global_session_state;
extern num_t global_total_summ;
extern num_t global_payment_summ;

int (*kkm_result_handler)(void);

static iconv_t internal_tokkm_codec;
static iconv_t internal_fromkkm_codec;

static gboolean internal_action=FALSE;
static GThread * internal_tid=NULL;

static int internal_fd=0;

char internal_kkm_num[21];

static gint internal_connect_sid=0;
static gboolean internal_connect_action=FALSE;

static size_t strconv(iconv_t codec, char *trg, size_t trgsize, char *src) {
    size_t srcsize=strlen(src);
    size_t len=trgsize;
    trgsize--;
    iconv(codec,&src,&srcsize,&trg,&trgsize);
    *trg=0;
    return len-trgsize;
    }

void kkm_textToKKM(char * trg, size_t size, char * src) {
    strconv(internal_tokkm_codec, trg, size, src);
    }

void kkm_textFromKKM(char * trg, size_t size, char * src) {
    strconv(internal_fromkkm_codec, trg, size, src);
    }

void kkm_getNum(char *s) {
    strcpy(s, internal_kkm_num);
    }

void kkm_init(void) {
    bzero(internal_kkm_num, sizeof(internal_kkm_num));
    internal_tokkm_codec=iconv_open("cp1251","utf8");
    internal_fromkkm_codec=iconv_open("utf8","cp1251");
    }

void kkm_destroy(void) {
    iconv_close(internal_tokkm_codec);
    iconv_close(internal_fromkkm_codec);
    }

/****************************************************************************/    
struct kkmstate {
    char n_kkm[21];
    int n_session;
    int n_doc;
    int n_sale;
    int n_return;
    int flags;
    };    

static int GetKKMState(struct kkmstate * st) {
    if (commands_fpState(FP_KKM_NUM, st->n_kkm,
			 FP_Z_NUM, &st->n_session,
			 END_ARGS)<0) return -1;
    usleep(COMMAND_INTERVAL);
    if (commands_kkmState(KKM_DOC_NUM, &st->n_doc,
			  KKM_FLAGS, &st->flags,
			  END_ARGS)<0) return -1;
    usleep(COMMAND_INTERVAL);
    if (commands_operReg(NUMBER_CHECK, &st->n_sale)<0) return -1;
    usleep(COMMAND_INTERVAL);
    if (commands_operReg(NUMBER_CHECK+2, &st->n_return)<0) return -1;
    log_printf(0, "KKM Зав.№: %s\n", st->n_kkm);
    log_printf(1, "Последняя смена №%d\n", st->n_session);
    log_printf(1, "Последний документ №%d\n", st->n_doc);
    return 0;
    }

int kkm_sync(void) {
    struct kkmstate st;
    bzero(&st, sizeof(st));
    int r=commands_wait(100);
    if (r<0) return -1;
    if (r==0) return 0;
    usleep(COMMAND_INTERVAL);
    if (GetKKMState(&st)<0) return 0;
    if (strcmp(internal_kkm_num, st.n_kkm)) return -1;
    if (!(st.flags & FLAG_SESSION)) {
        global_doc_num=st.n_doc;
        global_session_num=st.n_session;
        global_sale_num=st.n_sale;
        global_return_num=st.n_return;
        }
    return 1;
    }

/****************************************************************************/    
struct msg_struct {
    uint8_t prefix;
    uint8_t data[255];
    } __attribute__ ((packed)) msg_struct;

static int ServerAction(void) {
    static uint8_t buf[256];
    static struct msg_struct answer;
    static int answer_len=0;

    struct msg_struct msg;
    bzero(&msg, sizeof(msg));
    struct source_info src;
    int len=protocol_message(&msg, sizeof(msg), &src);
    if (len<0) return 0;
    if (len==0) return -1;
#ifdef ANSWER_INTERVAL
    usleep(ANSWER_INTERVAL);
#endif
    if (src.message_id==commands_command_id && answer_len>0) {
	if (memcmp(&msg, buf, len)==0) return commands_reply(&answer, answer_len, &src);
	log_puts(0, "Операция не выполнена в ККМ");
        if (global_doc_state!=DOC_CLOSED) global_doc_state=DOC_ERROR;
	gdk_threads_enter();
    	    if (global_doc_state==DOC_ERROR) {
		mw_showDocState(global_doc_state, global_doc_num+1);
		mw_stopOn();
		}
    	    mw_showText("Операция не выполнена в ККМ");
        gdk_threads_leave();
	answer_len=0;
        return commands_replyError(msg.prefix, -15, &src);
	}
    memcpy(buf, &msg, len);
    bzero(&answer, sizeof(answer));
    memcpy(&answer, &msg, len);
    int r=process_event(answer.prefix, answer.data);
    if (r<-255) {
	protocol_clear();
	answer_len=0;
	return 0;
	}
    if (r<0) {
        answer_len=0;
        return commands_replyError(answer.prefix, r, &src);
        }
    if (r==0) {
        answer_len=sizeof(msg)-sizeof(msg.data)+1;
	answer.prefix=msg.prefix;
        uint8_t b=0;
	memcpy(answer.data, &b, 1);
        }
    else
	answer_len=sizeof(answer)-sizeof(answer.data)+r;
    return commands_reply(&answer, answer_len, &src);
    }

static int DoServerAction(void) {
    fd_set ports;
    FD_ZERO(&ports);
    while (internal_action) {	
        FD_SET(internal_fd, &ports);
	select(FD_SETSIZE, &ports, NULL, NULL, NULL);
	if (FD_ISSET(internal_fd, &ports)) {
	    int r=ServerAction();
	    if (r<0) return -1;
	    if (kkm_result_handler!=NULL) {
		r=kkm_result_handler();
    		kkm_result_handler=NULL;
		if (r<0) return -1;
    		}
	    }
	}
    return 0;
    }

/****************************************************************************/    
static int ConnectAction(void) {
    int n=commands_echo();
    if (n<0) return -1;
    if (n==0) return 0;
    gdk_threads_enter();
	mw_showText("Подключение ККМ...");
	mw_wheelOn();
    gdk_threads_leave();
    struct kkmstate st;
    bzero(&st, sizeof(st));
    usleep(COMMAND_INTERVAL);
    if (GetKKMState(&st)<0) {
        gdk_threads_enter();
	    mw_wheelOff();
	    mw_clearText();
	gdk_threads_leave();
        return 0;
	}
    strcpy(internal_kkm_num, st.n_kkm);
    global_session_num=st.n_session;
    global_session_state=(st.flags & FLAG_SESSION)?SESSION_OPEN:SESSION_CLOSED;
    global_doc_num=st.n_doc;
    global_doc_state=(st.flags & FLAG_RECEIPT)?DOC_OPEN:DOC_CLOSED;
    global_sale_num=st.n_sale;
    global_return_num=st.n_return;

    if (actions_processConnect(internal_kkm_num)<0) {
        gdk_threads_enter();
    	    mw_showText("Ошибка обращения к БД");
    	    mw_wheelOff();
        gdk_threads_leave();
        return 0;
	}
//    log_printf(0, "KKM Зав.№: %s", internal_kkm_num);
//    log_printf(1, "Последняя смена №%d", global_last_session_num);
//    log_printf(1, "Последний документ №%d", global_last_doc_num);
    localdb_setWareTimeout(OPERATION_TIMEOUT+50);
    gdk_threads_enter();
	mw_clearText();
        mw_kkmConnect_on(internal_kkm_num);
	mw_showSession(global_session_state, global_session_num+1);
        mw_showDocState(global_doc_state, global_doc_num+1);
        receipt_showAll();
	mw_wheelOff();
	mw_showTotal(&global_total_summ);
	if (num_cmp0(&global_payment_summ)) {
	    char text[64];
	    strcpy(text, "Оплата: ");
	    char * s=strchr(text, 0);
	    num_toString(s, &global_payment_summ);
	    mw_showText(text);
	    }
	if (global_session_state==SESSION_ERROR || global_doc_state==DOC_ERROR) mw_stopOn();
    gdk_threads_leave();
    return 1;
    }

static int ConnectProcess(void) {
//puts("* ConnectAction()");
    struct timeval tout;
    fd_set ports;
    FD_ZERO (&ports);
    while (internal_action) {	
	int r=ConnectAction();
	if (r<0) return -1;
	if (r>0) return 1;
	FD_SET (internal_fd, &ports);
        tout.tv_sec=2;
	tout.tv_usec=0;
	select (FD_SETSIZE, &ports, NULL, NULL, &tout);
	if (FD_ISSET(internal_fd, &ports)) {
	    uint8_t prefix=0;
	    struct source_info src;
	    int r=protocol_message(&prefix, 1, &src);
	    if (r==0) return -1;
	    if (r>0) {
#ifdef ANSWER_INTERVAL
		usleep(ANSWER_INTERVAL);
#endif
		commands_replyError(prefix, -2, &src);
		}
	    }
	}
    return 0;
    }

static gboolean ConnectKKM(void);

static void ServerProcess(void) {
//puts("* ServerProcess()");
    int r=ConnectProcess();
    if (r>0) r=DoServerAction();
    protocol_closePort();
    if (r<0) {
        actions_disconnect();
        num_clear(&global_payment_summ);
	num_clear(&global_total_summ);
        gdk_threads_enter();
	    mw_showDocState(DOC_CLEAR, 0);
	    mw_showSession(SESSION_CLEAR,0);
	    mw_clearText();
	    mw_clearTable();
	    mw_showTotal(&global_total_summ);
	    mw_stopOff();
	    mw_accessDisable();
	    mw_kkmConnect_off();
        gdk_threads_leave();
        internal_connect_sid=g_timeout_add(CONNECT_INTERVAL*100, (GSourceFunc)ConnectKKM, NULL);
	}
//puts("** ServerProcess()");
    }

static gboolean ConnectKKM(void) {
//puts("* ConnectKKM()");
    int r=protocol_openPort();
    if (r>0) {
	internal_fd=r;
	GError * err=NULL;
        internal_action=TRUE;
        internal_tid=g_thread_create((GThreadFunc)ServerProcess, NULL, TRUE, &err);
	if (internal_tid!=NULL) return FALSE;
        internal_action=FALSE;
#ifdef DEBUG
        if (err!=NULL) fprintf(stderr,"* g_thread_create failed: %s\n", err->message);
	else fputs("* g_thread_create failed: unknown error\n",stderr);
#endif
	protocol_closePort();
	}	    
    return TRUE;
    }

/*****************************************************************************/
void kkm_serverStart(void) {
    if (internal_connect_sid>0) {
	g_source_remove(internal_connect_sid);
	}
    if (ConnectKKM()) internal_connect_sid=g_timeout_add(CONNECT_INTERVAL*100, (GSourceFunc)ConnectKKM, NULL);
    }

void kkm_serverStop(void) {
    if (internal_action) {
        internal_action=FALSE;
        g_thread_join(internal_tid);
	}
    }
