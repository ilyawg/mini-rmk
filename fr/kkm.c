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
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <iconv.h>
#include <gtk/gtk.h>
#include <obstack.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#include "../vararg.h"
#include "conf.h"
#include "commands.h"
#include "main_window.h"
#include "receipt.h"
#include "kkm.h"
#include "localdb.h"
#include "global.h"
#include "../recordset.h"

#define KKM_NUM_SIZE		16
#define CONNECT_INTERVAL	50

static struct obstack internal_pool;
static int * internal_connect_sid;
static char * internal_kkm_num;
static iconv_t * internal_tokkm_codec;
static iconv_t * internal_fromkkm_codec;
static GThread * internal_tid=NULL;
static gboolean * internal_action=FALSE;

/************************************************************************************************/

static size_t strconv(iconv_t codec, char *trg, size_t trgsize, char *src) {
    size_t srcsize=strlen(src);
    size_t len=trgsize;
    trgsize--;
    iconv(codec,&src,&srcsize,&trg,&trgsize);
    *trg=0;
    return len-trgsize;
    }

void kkm_textToKKM(char * trg, size_t size, char * src) {
    strconv(*internal_tokkm_codec, trg, size, src);
    }

void kkm_textFromKKM(char * trg, size_t size, char * src) {
    strconv(*internal_fromkkm_codec, trg, size, src);
    }

/************************************************************************************************/

void kkm_getNum(char *s) {
    strcpy(s, internal_kkm_num);
    }

void kkm_init(void) {
    obstack_init(&internal_pool);
    internal_kkm_num=obstack_alloc(&internal_pool, KKM_NUM_SIZE);
    bzero(internal_kkm_num, KKM_NUM_SIZE);
    internal_tokkm_codec=obstack_alloc(&internal_pool, sizeof(iconv_t));
    *internal_tokkm_codec=iconv_open("cp1251","utf8");
    internal_fromkkm_codec=obstack_alloc(&internal_pool, sizeof(iconv_t));
    *internal_fromkkm_codec=iconv_open("utf8","cp1251");
    
    internal_connect_sid=obstack_alloc(&internal_pool, sizeof(int));
    internal_action=obstack_alloc(&internal_pool, sizeof(gboolean));    
    }

void kkm_destroy(void) {
    iconv_close(internal_tokkm_codec);
    iconv_close(internal_fromkkm_codec);
    obstack_free(&internal_pool, NULL);
    }

/****************************************************************************/    
static gboolean ConnectKKM(void);

static int ConnectAction(void) {
    if (protocol_start()<0) {
	return -1;
	}
    gdk_threads_enter();
	mw_showText("Подключение ККМ...");
	mw_wheelOn();
    gdk_threads_leave();

    char n_kkm[16];
    int n_session, mode, submode, n_doc, sale_num, return_num;
    
    int r=commands_getState(KKM_SERIAL, n_kkm,
			    KKM_SESSION_NUM, &n_session,
			    KKM_MODE, &mode,
			    KKM_SUBMODE, &submode,
			    KKM_DOC_NUM, &n_doc,
			    END_ARGS);
/*    if (r==0) {
	r=commands_operReg(148);
	if (r>=0) {
	    sale_num=r;
	    r=commands_operReg(150);
	    if (r>=0) return_num=r;
	    }
	}*/
    if (r<0) {
        gdk_threads_enter();
	    mw_wheelOff();
	    mw_kkmConnect_off();
    	    mw_showText("Ошибка ККМ");
	gdk_threads_leave();
	return -1;
	}
    switch (submode) {
    case 0x01:
    case 0x02:
        gdk_threads_enter();
	    mw_wheelOff();
    	    mw_showText("Нет бумаги");
	gdk_threads_leave();
	return -1;
    case 0x03:
	commands_print();
        gdk_threads_enter();
	    mw_wheelOff();
    	    mw_clearText();
	gdk_threads_leave();
	return -1;
    case 0x04:
    case 0x05:
        gdk_threads_enter();
	    mw_wheelOff();
    	    mw_showText("Идет печать операции...");
	gdk_threads_leave();
	return -1;
	}
    
    strcpy(internal_kkm_num, n_kkm);
    global_session_num=n_session;
    global_doc_num=n_doc;
printf("mode: %02x\n", mode);
    switch (mode&0x0F) {
    case 0x2:
    case 0x3:
	global_session_state=SESSION_OPEN;
	global_doc_state=DOC_CLOSED;
	break;
    case 0x4:
	global_session_state=SESSION_CLOSED;
	global_doc_state=DOC_CLOSED;
	break;
    case 0x8:
	global_session_state=SESSION_UNDEF;
	if (mode==0x08) {
	    global_doc_state=DOC_SALE;
//	    global_check_num=sale_num;
	    }
	else if (mode==0x28) {
	    global_doc_state=DOC_RETURN;
//	    global_check_num=return_num;
	    }
	else global_doc_state=DOC_ERROR;
	break;
    default:
	global_doc_state=DOC_UNDEF;
	global_session_state=SESSION_UNDEF;
	break;
	}
    if (actions_processConnect(internal_kkm_num)<0) {
        gdk_threads_enter();
    	    mw_showText("Ошибка обращения к БД");
    	    mw_wheelOff();
        gdk_threads_leave();
        return -1;
	}
//    log_printf(0, "KKM Зав.№: %s", internal_kkm_num);
//    log_printf(1, "Последняя смена №%d", global_last_session_num);
//    log_printf(1, "Последний документ №%d", global_last_doc_num);
    gdk_threads_enter();
	mw_wheelOff();
	mw_clearText();
        mw_kkmConnect_on(internal_kkm_num);
	mw_showSession(global_session_state, global_session_num+1);
        mw_showDocState(global_doc_state, global_doc_num+1);
	if (global_doc_state==DOC_SALE || global_doc_state==DOC_RETURN) {
	    receipt_showAll();
	    mw_showTotal(global_receipt_summ);
	    }
	if (global_session_state==SESSION_ERROR || global_doc_state==DOC_ERROR) mw_stopOn();
        mw_unlock();
    gdk_threads_leave();
    scanner_enable(TRUE);
    return 0;
    }

static void ConnectProcess(void) {
    if (ConnectAction()<0) {
	protocol_closePort();
	*internal_connect_sid=g_timeout_add(CONNECT_INTERVAL*100, (GSourceFunc)ConnectKKM, NULL);
	}
    *internal_action=FALSE;
    }
	
static gboolean ConnectKKM(void) {
//puts("* ConnectKKM()");
    int r=protocol_openPort();
    if (r>0) {
	GError * err=NULL;
        *internal_action=TRUE;
        internal_tid=g_thread_create((GThreadFunc)ConnectProcess, NULL, TRUE, &err);
	if (internal_tid!=NULL) return FALSE;
        *internal_action=FALSE;
#ifdef DEBUG
        if (err!=NULL) fprintf(stderr,"* g_thread_create failed: %s\n", err->message);
	else fputs("* g_thread_create failed: unknown error\n",stderr);
#endif
	protocol_closePort();
	}
    return TRUE;
    }

static void Disconnect(char * text) {
    protocol_closePort();
    actions_disconnect();
    gdk_threads_enter();
	mw_wheelOff();
	mw_showDocState(DOC_CLEAR, 0);
	mw_showSession(SESSION_CLEAR,0);
	mw_showText(text);
        mw_clearTable();
        mw_showTotal(NULL);
        mw_stopOff();
        mw_accessDisable();
        mw_kkmConnect_off();
    gdk_threads_leave();
    }

gboolean kkm_error(int err, gboolean f) {
    int mode;
    char * text;
    if (err==-115) {
	err=commands_getState(KKM_MODE, &mode, END_ARGS);
	if (err==0) {
	    switch(mode&0x0F) {
	    case 0x2:	text="ККМ: Чек закрыт - операция невозможна"; break;
	    case 0x3:	text="ККМ: Превышена длительность смены"; break;
	    case 0x4:	text="ККМ: Смена закрыта - операция невозможна"; break;
	    case 0x8:	text="ККМ: Чек открыт - операция невозможна"; break;
	    default: 	text="Неверное состояние ККМ"; break;
		}
	    gdk_threads_enter();
    		mw_showText(text);
		mw_wheelOff();
    		mw_unlock();
	    gdk_threads_leave();
	    if (f) scanner_enable(TRUE);
	    return FALSE;
	    }
	}
    switch (err) {
    case -44:	text="ККМ: Смена не открыта"; break;
    case -46:	text="ККМ: Нет денег для выплаты"; break;
    case -59:	text="Переполнение накопления в смене"; break;
    case -61:	text="Смена не открыта - операция невозможна"; break;
    case -69:	text="Сумма оплаты меньше итога чека"; break;
    case -70:	text="ККМ: Не хватает наличности в кассе"; break;
    case -72:	text="ККМ: Переполнение итога чека"; break;
    case -73:	text="Операция невозможна в чеке данного типа"; break;
    case -74:	text="ККМ: Не хватает наличности в кассе"; break;
    case -75:	text="ККМ: Буфер чека переполнен"; break;
    case -78:	text="Превышена длительность смены"; break;
    case -80:	text="ККМ: Операция не выполнена"; break;
    case -81:	text="ККМ: Переполнение наличности в смене"; break;
    case -85:	text="Чек закрыт - операция невозможна"; break;
    case -94:	text="ККМ: Некорректная операция"; break;
    case -95:	text="ККМ: Отрицательный итог чека"; break;
    case -96:	text="ККМ: Переполнение при умножении"; break;
    case -97:	text="ККМ: Переполнение диапазона цены"; break;
    case -98:	text="ККМ: Переполнение диапазона количества"; break;
    case -107:	text="ККМ: Нет чековой ленты"; break;
    case -108:	text="ККМ: Нет контрольной ленты"; break;
    case -111:	text="ККМ: Переполнение по выплате в смене"; break;
    case -113:	text="ККМ: Ошибка отрезчика"; break;
    case -115:	text="ККМ: Команда не поддерживается в данном режиме"; break;
    case -127:	text="ККМ: Переполнение диапазона итога чека"; break;
    case -132:	text="ККМ: Переполнение наличности"; break;
    case -133:	text="ККМ: Переполнение по продажам в смене"; break;
    case -135:	text="ККМ: Переполнение по возвратам в смене"; break;
    case -137:	text="ККМ: Переполнение по внесению в смене"; break;
    case -142:	text="ККМ: Нулевой итог чека"; break;
    case -148:	text="ККМ: Исчерпан лимит операций в чеке"; break;
    default:
printf("err: %d\n", err);
	text="Ошибка ККМ";
	break;
	}    
    if (err==-256 || err==-88) {
	Disconnect(text);
	*internal_connect_sid=g_timeout_add(CONNECT_INTERVAL*100, (GSourceFunc)ConnectKKM, NULL);
	return FALSE;
	}
    gdk_threads_enter();
        mw_showText(text);
	mw_wheelOff();
        mw_unlock();
    gdk_threads_leave();
    if (f) scanner_enable(TRUE);
    return FALSE;
    }

/*****************************************************************************/

void kkm_serverStart(void) {
    if (*internal_connect_sid>0) {
	g_source_remove(*internal_connect_sid);
	}
    if (ConnectKKM()) *internal_connect_sid=g_timeout_add(CONNECT_INTERVAL*100, (GSourceFunc)ConnectKKM, NULL);
    }

void kkm_serverStop(void) {
    if (internal_action) {
        internal_action=FALSE;
        g_thread_join(internal_tid);
	}
    }
