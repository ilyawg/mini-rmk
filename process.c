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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "conf.h"
#include "global.h"
#include "process.h"
#include "actions.h"
#include "kkm.h"
#include "protocol.h"
#include "commands.h"
#include "registers.h"
#include "receipt.h"
#include "main_window.h"
#include "localdb.h"
#include "global.h"
#include "numeric.h"
#include "recordset.h"

extern int global_doc_state;
extern int global_session_state;
extern int global_session_num;
extern int global_doc_num;
extern int global_sale_num;
extern int global_return_num;
extern num_t global_total_summ;
extern num_t global_payment_summ;

extern int oper_check_num;
extern int oper_doc_num;
extern int oper_check_type;
extern int oper_seller;
extern int oper_payment_type;
extern num_t oper_summ;
extern num_t oper_quantity;
extern struct record_receipt * oper_rc;
extern struct tm oper_date_time;

static gboolean internal_kkm_lock=FALSE;
static GMutex * internal_mutex=NULL;

enum {TIME_BEGIN, TIME_END};
static int GetTime(int mode) {
    static struct timeval begin;
    struct timeval end, d;
    switch (mode) {
    case TIME_BEGIN: gettimeofday(&begin, NULL); return 0;
    case TIME_END:
	gettimeofday(&end, NULL);
	end.tv_sec-=begin.tv_sec;
	end.tv_usec-=begin.tv_usec;
	if (end.tv_usec<0) {
	    end.tv_sec--;
	    end.tv_usec+=1000000;
	    }
	return end.tv_sec*1000+end.tv_usec/1000;
	}
    return -1;
    }

int process_init(void) {
    internal_mutex=g_mutex_new();
    if (internal_mutex==NULL) return -1;
    return 0;
    }

void process_destroy(void) {
    g_mutex_free(internal_mutex);
    }

static void StartThread(void (*thread_func)(void)) {
    GError * err=NULL;
    GThread * tid=g_thread_create((GThreadFunc)thread_func, NULL, FALSE, &err);
    if (tid==NULL) {
	if (err!=NULL)
	    fprintf(stderr,"* g_thread_create failed: %s\n", err->message);
	else fputs("* g_thread_create failed: unknown error\n",stderr);
	}
    }

static void log_status(char *operation) {
    char * s;
    switch(global_doc_state) {
    case DOC_ERROR:	s="ОШИБКА"; break;
    case DOC_SALE:	s="ПРОДАЖА"; break;
    case DOC_RETURN:	s="ВОЗВРАТ"; break;
    case DOC_CASHIN:	s="ВНЕСЕНИЕ"; break;
    case DOC_CASHOUT:	s="ВЫПЛАТА"; break;
    case DOC_CLOSED:	s="ЗАКРЫТ"; break;
    case DOC_CANCEL:	s="АННУЛИРОВАН"; break;
    default:		s="НЕОПРЕДЕЛЕН"; break;
	}	
    log_printf(0, "%s: Смена %d, Документ %d/%s", operation, global_session_num+1, global_doc_num+1, s);
    }

static void log_db_fail(char * operation) {
    log_status(operation);
    log_puts(1, "* Ошибка обращения к БД");
    }

#ifdef OPERATION_JOURNAL
static void control_cash(char * operation, num_t * summ, int n_doc, int n_check) {
    char n_kkm[21]; kkm_getNum(n_kkm);
    char s[16];
    num_toString(s, summ);
    control_doOpen(n_kkm, global_session_num+1);
    control_printf(0, "%s: сумма: %s, документ: %d, чек: %d", operation, s, n_doc, n_check);
    }

static void control_receipt(char * operation, int n_doc, int n_check, int check_type) {
    char n_kkm[21]; kkm_getNum(n_kkm);
    char * s;
    switch (check_type) {
    case DOC_SALE: s="ПРОДАЖА"; break;
    case DOC_RETURN: s="ВОЗВРАТ"; break;
    default: s=""; break;
	}
    control_doOpen(n_kkm, global_session_num+1);
    control_printf(0, "%s: документ: %d, чек: %d, %s", operation, n_doc, n_check, s);
    }    

    
static void control_storno(int check_type, int code, char * barcode, num_t * quantity, num_t * summ) {
    char n_kkm[21]; kkm_getNum(n_kkm);
    char s_quantity[16]; num_toString(s_quantity, quantity);
    char s_summ[16]; num_toString(s_summ, summ);
    char * st;
    switch (check_type) {
    case DOC_SALE: st="ПРОДАЖА"; break;
    case DOC_RETURN: st="ВОЗВРАТ"; break;
    default: st=""; break;
	}    
    control_doOpen(n_kkm, global_session_num+1);
    control_printf(0, "СТОРНО: код: %d, сумма: %s, %s", code, s_summ, st);
    control_printf(1, "  * количество: %s, штрихкод: `%s'", s_quantity, barcode);
    }

static void control_registration(int check_type, int code, num_t * quantity, num_t *price, num_t * summ, char * barcode) {
    char n_kkm[21]; kkm_getNum(n_kkm);
    char s_price[16]; num_toString(s_price, price);
    char s_quantity[16]; num_toString(s_quantity, quantity);
    char s_summ[16]; num_toString(s_summ, summ);
    char * st;
    switch (check_type) {
    case DOC_SALE: st="ПРОДАЖА"; break;
    case DOC_RETURN: st="ВОЗВРАТ"; break;
    default: st=""; break;
	}    
    control_doOpen(n_kkm, global_session_num+1);
    control_printf(0, "РЕГИСТРАЦИЯ: код: %d, сумма: %s, %s", code, s_summ, st);
    control_printf(1, "  * цена: %s, количество: %s, штрихкод: `%s'", s_price, s_quantity, barcode);
    }

static void control_payment(num_t * summ, int payment_type) {
    char n_kkm[21]; kkm_getNum(n_kkm);
    char s[16];
    num_toString(s, summ);
    control_doOpen(n_kkm, global_session_num+1);
    control_printf(0, "ОПЛАТА: %d сумма: %s", payment_type, s);
    }

static void control_session(num_t * summ, int n_doc, int n_session) {
    char n_kkm[21]; kkm_getNum(n_kkm);
    char s[16]; num_toString(s, summ);
    control_doOpen(n_kkm, global_session_num+1);
    control_printf(0, "ЗАКРЫТИЕ СМЕНЫ: документ: %d, смена: %d, итог: %s", n_doc, n_session, s);
    }
#endif //OPERATION_JOURNAL

/**************************** cash process ****************************/    
static int CashAction(void) {
    int n_doc=0;
    usleep(500000);
    int r=commands_wait(20);
    if (r<0) {
	global_doc_num=oper_doc_num;
	log_puts(0, "ВНЕСЕНИЕ/ВЫПЛАТА: Ошибка обмена данными");
        gdk_threads_enter();
    	    mw_showText("Ошибка обмена данными");
    	    mw_showDocState(DOC_CLOSED, 0);
    	    mw_wheelOff();
        gdk_threads_leave();
	return -1;
	}
    if (r==0 || commands_kkmState(KKM_DOC_NUM, &n_doc, END_ARGS)<0) {
	global_doc_num=oper_doc_num;
	log_puts(0, "ВНЕСЕНИЕ/ВЫПЛАТА: Ошибка ККМ");
        gdk_threads_enter();
    	    mw_showText("Ошибка ККМ");
    	    mw_showDocState(DOC_CLOSED, 0);
    	    mw_wheelOff();
        gdk_threads_leave();
	return 0;
	}
    if (n_doc!=oper_doc_num) {
	log_puts(0, "ВНЕСЕНИЕ/ВЫПЛАТА: Операция не выполнена в ККМ");
        gdk_threads_enter();
	    mw_clearText();
	    mw_showDocState(DOC_CLOSED, 0);
    	    mw_wheelOff();
	gdk_threads_leave();
	return 0;
	}
    char str[64];
    bzero(str,sizeof(str));
    switch (oper_check_type) {
    case DOC_CASHIN:	strcpy(str,"Внесено: "); break;
    case DOC_CASHOUT:	strcpy(str,"Изъято: "); break;
        }
    num_toString(strchr(str,0), &oper_summ);
    global_doc_num=oper_doc_num;
    gdk_threads_enter();
	mw_showText(str);
        mw_showDocState(DOC_CLOSED, 0);
	mw_accessDisable();
        mw_wheelOff();
    gdk_threads_leave();
    return 0;
    }

int CashProcess(int check_type, void * data) {
    struct {
	uint8_t sum[6];
	uint8_t date[3];
	uint8_t time[3];
	uint8_t seller;
	uint16_t n_doc;
	uint16_t n_check;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, data, sizeof(msg));
    oper_doc_num=msg.n_doc;
    oper_check_type=check_type;
    gdk_threads_enter();
	mw_wheelOn();
        switch (oper_check_type) {
	case DOC_CASHIN:	mw_showText("Внесение..."); break;
	case DOC_CASHOUT:	mw_showText("Выплата..."); break;
        }
	mw_showDocState(check_type, msg.n_doc);
    gdk_threads_leave();
    num_init(&oper_summ, 2);
    memcpy(&oper_summ.value, msg.sum, sizeof(msg.sum));
#ifdef OPERATION_JOURNAL
    control_cash("ВНЕСЕНИЕ/ВЫПЛАТА", &oper_summ, oper_doc_num, msg.n_check);
#endif
    kkm_result_handler=CashAction;
    return 0;
    }

/************************** code_process() **********************************/
static void RegistrationThread(void) {
    if (global_doc_state==DOC_CLOSED) global_doc_state=oper_check_type;
#ifdef OPERATION_JOURNAL
    control_registration(oper_check_type, oper_rc->code, &oper_rc->quantity, &oper_rc->price, &oper_rc->summ, oper_rc->barcode);
#endif
    int r=actions_processRegistration();
    num_addThis(&global_total_summ, &oper_rc->summ);
    if (r<0) {
	log_db_fail("РЕГИСТРАЦИЯ");
	global_doc_state=DOC_ERROR;
	}
    gdk_threads_enter();
	if (r<0) {
	    mw_showText("Ошибка обращения к БД");
	    mw_stopOn();
	    }
        oper_rc->path=mw_newRecord();
	mw_showDocState(global_doc_state, global_doc_num+1);
        mw_showRecord(oper_rc);
	mw_showTotal(&global_total_summ);
	mw_accessDisable();
	mw_wheelOff();
    gdk_threads_leave();
    receipt_addRecord(oper_rc);
    internal_kkm_lock=FALSE;
    }

static void StornoThread(void) {
    int r=actions_processStorno();
//printf("**** r=%d\n", r);
    num_addThis(&oper_rc->quantity, &oper_quantity);
    num_addThis(&oper_rc->summ, &oper_summ);
    num_addThis(&global_total_summ, &oper_summ);
    if (r<0) {
	log_db_fail("СТОРНО");
        global_doc_state=DOC_ERROR;
	}
    gdk_threads_enter();
	if (r<0) {
	    mw_showDocState(DOC_ERROR, global_doc_num+1);
	    mw_showText("Ошибка обращения к БД");
	    mw_stopOn();
	    }
        mw_showRecord(oper_rc);
	mw_showTotal(&global_total_summ);
	mw_accessDisable();
	mw_wheelOff();
    gdk_threads_leave();
    internal_kkm_lock=FALSE;
    }

/****************************************************************************/
static int CodeRegistration(int code, num_t * quantity, void * buf) {
    struct ware_dbrecord ware;
    ware.ware_id=code;
    int r=localdb_getWare(&ware);
    if (GetTime(TIME_END)>OPERATION_TIMEOUT) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
    	    mw_showText("Товар недоступен");
	    mw_wheelOff();
        gdk_threads_leave();
        return -256;
        }
    if (r<=0) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
    	    mw_showText("Неверный код товара");
	    mw_wheelOff();
        gdk_threads_leave();
        return -10;
        }
    oper_rc=receipt_newRecord();
    oper_rc->code=code;
    strcpy(oper_rc->longtitle, ware.longtitle);
    strcpy(oper_rc->shortcut, ware.shortcut);
    num_fromString(&oper_rc->price, ware.price);
    num_dup(&oper_rc->quantity, quantity);
    num_mul(&oper_rc->summ, &oper_rc->price, quantity);
    gdk_threads_enter();
	mw_showText(ware.longtitle);
        mw_showCode(code);
	mw_showQuantity(quantity);
        mw_showPrice(&oper_rc->price);
	mw_showSum(&oper_rc->summ);
    gdk_threads_leave();
    internal_kkm_lock=TRUE;
    StartThread(RegistrationThread);
    struct {
	uint8_t err;
        uint8_t name[40];
        uint8_t price[5];
        uint8_t depart;
        uint8_t tax[4];
        } __attribute__ ((packed)) msg;
    bzero(&msg, sizeof(msg));
    char str[41];
    size_t l=kkm_textToKKM(str, sizeof(str), ware.shortcut);
    memset(msg.name, 0x20, sizeof(msg.name));
    memcpy(msg.name, str, l);
    memcpy(msg.price, &oper_rc->price.value, sizeof(msg.price));
    msg.depart=1;
    memcpy(buf, &msg, sizeof(msg));
    return sizeof(msg);
    }

static int CodeStorno(int code, num_t * quantity, void * buf) {
    oper_rc=receipt_findRecordByCode(code, quantity);
    if (oper_rc==NULL) {;
        gdk_threads_enter();
	    mw_showText("Нет позиции для сторно");
	    mw_wheelOff();
        gdk_threads_leave();
	return -10;
	}
    num_dup(&oper_quantity, quantity); num_negThis(&oper_quantity);
    num_init(&oper_summ, 2);
    num_mul(&oper_summ, &oper_rc->price, &oper_quantity);
    gdk_threads_enter();
	mw_showText("Сторно");
        mw_showCode(code);
	mw_showQuantity(quantity);
        mw_showPrice(&oper_rc->price);
	mw_showSum(&oper_summ);
	mw_wheelOff();
    gdk_threads_leave();
    internal_kkm_lock=TRUE;
    StartThread(StornoThread);
#ifdef OPERATION_JOURNAL
    control_storno(oper_check_type, code, "", quantity, &oper_summ);
#endif
    struct {
	uint8_t err;
        uint8_t name[40];
        uint8_t price[5];
        uint8_t depart;
        uint8_t tax[4];
        } __attribute__ ((packed)) msg;
    bzero(&msg, sizeof(msg));
    char str[41];
    size_t l=kkm_textToKKM(str, sizeof(str), oper_rc->shortcut);
    memset(msg.name, 0x20, sizeof(msg.name));
    memcpy(msg.name, str, l);
    memcpy(msg.price, &oper_rc->price.value, sizeof(msg.price));
    msg.depart=1;
    memcpy(buf, &msg, sizeof(msg));
    return sizeof(msg);
    }    

static int Forbidden(void) {
    gdk_threads_enter();
        mw_showText("Операция запрещена");
    gdk_threads_leave();
    return -10;
    }

int CodeProcess(void * buf) {
    GetTime(TIME_BEGIN);
    struct {
	uint8_t check_type;
	uint8_t storno;
	uint8_t quantity[5];
	uint8_t code[5];
	uint8_t seller;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));
    if (msg.storno==1 && global_conf_storno_access==0 && !global_access_flag) return Forbidden();
    
    NUMERIC(quantity,3);
    memcpy(&quantity.value, msg.quantity, sizeof(msg.quantity));

    int code=0;
    memcpy(&code, msg.code, sizeof(code));

    switch(msg.check_type) {
    case 1: oper_check_type=DOC_SALE; break;
    case 3: 
	if (global_conf_return_access==0 && !global_access_flag) return Forbidden();
	oper_check_type=DOC_RETURN;
	break;
    default: 
        gdk_threads_enter();
	    mw_showText("Неверный статус документа");
        gdk_threads_leave();
	return -10;
	}
    oper_seller=msg.seller;

    if (msg.storno==1 && global_conf_storno_access==0 && !global_access_flag) return Forbidden();

//    if (global_doc_state==DOC_CLOSED) global_doc_state=oper_check_type;
    gdk_threads_enter();
        mw_wheelOn();
    gdk_threads_leave();
    if (msg.storno) return CodeStorno(code, &quantity, buf);
    if (global_doc_state==DOC_CLOSED) {
//	global_doc_state=oper_check_type;
	mw_agentLock(TRUE);
	}
    return CodeRegistration(code, &quantity, buf);
    }

/************************ barcode_process() **********************************/
static int NumStorno(char * num_s, void * buf) {
//printf("storno: <%s>\n", num_s);
    if (num_s==NULL) oper_rc=mw_current_record;
    else {
	int num=atoi(num_s);
        oper_rc=receipt_getRecordByNum(num);
	}
    if (oper_rc==NULL || !num_cmp0(&oper_rc->quantity)) {;
        gdk_threads_enter();
	    mw_showText("Нет позиции для сторно");
        gdk_threads_leave();
	return -10;
	}
    num_dup(&oper_quantity, &oper_rc->quantity); num_negThis(&oper_quantity);
    num_dup(&oper_summ, &oper_rc->summ); num_negThis(&oper_summ);
    gdk_threads_enter();
	mw_showText("Сторно");
        mw_showCode(oper_rc->code);
	mw_showQuantity(&oper_quantity);
        mw_showPrice(&oper_rc->price);
	mw_showSum(&oper_summ);
    gdk_threads_leave();
    internal_kkm_lock=TRUE;
    StartThread(StornoThread);
#ifdef OPERATION_JOURNAL
    control_storno(oper_check_type, oper_rc->code, num_s, &oper_quantity, &oper_summ);
#endif
    struct {
	uint8_t err;
	uint8_t barcode_type;
	uint8_t name[40];
	uint8_t price[5];
	uint8_t depart;
	uint8_t tax[4];
	uint8_t quantity[5];
        } __attribute__ ((packed)) msg;
    bzero(&msg, sizeof(msg));
    msg.barcode_type=1;
    char str[41];
    size_t l=kkm_textToKKM(str, sizeof(str), oper_rc->shortcut);
    memset(msg.name, 0x20, sizeof(msg.name));
    memcpy(msg.name, str, l);
    memcpy(msg.price, &oper_rc->price.value, sizeof(msg.price));
    msg.depart=1;
    memcpy(msg.quantity, &oper_quantity.value, sizeof(msg.quantity));
    memcpy(buf, &msg, sizeof(msg));
    return sizeof(msg);
    }    

static int BarcodeStorno(char * barcode, num_t * quantity, void * buf) {
    oper_rc=receipt_findRecordByBarcode(barcode, quantity);
    if (oper_rc==NULL) {;
        gdk_threads_enter();
	    mw_showText("Нет позиции для сторно");
	    mw_wheelOff();
        gdk_threads_leave();
	return -10;
	}
    num_dup(&oper_quantity, quantity); num_negThis(&oper_quantity);
    num_init(&oper_summ, 2);
    num_mul(&oper_summ, &oper_rc->price, &oper_quantity);
    gdk_threads_enter();
	mw_showText("Сторно");
        mw_showCode(oper_rc->code);
	mw_showQuantity(&oper_quantity);
        mw_showPrice(&oper_rc->price);
        mw_showSum(&oper_summ);
    gdk_threads_leave();
    internal_kkm_lock=TRUE;
    StartThread(StornoThread);
#ifdef OPERATION_JOURNAL
    control_storno(oper_check_type, oper_rc->code, barcode, &oper_quantity, &oper_summ);
#endif
    struct {
	uint8_t err;
	uint8_t barcode_type;
	uint8_t name[40];
	uint8_t price[5];
	uint8_t depart;
	uint8_t tax[4];
	uint8_t quantity[5];
        } __attribute__ ((packed)) msg;
    bzero(&msg, sizeof(msg));
    char str[41];
    size_t l=kkm_textToKKM(str, sizeof(str), oper_rc->shortcut);
    memset(msg.name, 0x20, sizeof(msg.name));
    memcpy(msg.name, str, l);
    memcpy(msg.price, &oper_rc->price.value, sizeof(msg.price));
    msg.depart=1;
    memcpy(buf, &msg, sizeof(msg));
    return sizeof(msg);
    }

static int BarcodeRegistration(char * barcode, num_t * quantity, void * buf) {
//puts("* BarcodeRegistration()");
    struct recordset rset;
    recordset_init(&rset);
    int r=localdb_getWareByBarcode(&rset, barcode);
    if (GetTime(TIME_END)>OPERATION_TIMEOUT) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
    	    mw_showText("Товар недоступен");
	    mw_wheelOff();
        gdk_threads_leave();
        return -256;
        }
    if (r>1) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
	    mw_showText("Штрихкод не уникален");
	    mw_wheelOff();
        gdk_threads_leave();
	recordset_destroy(&rset);
	return -10;
	}
    struct ware_dbrecord * ware=NULL;
    if (r==1) ware=recordset_begin(&rset);
    if (ware==NULL) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
    	    mw_showText("Товар не найден");
	    mw_wheelOff();
        gdk_threads_leave();
	recordset_destroy(&rset);
	return -10;
	}
    oper_rc=receipt_newRecord();
    oper_rc->code=ware->ware_id;
    strcpy(oper_rc->longtitle, ware->longtitle);
    strcpy(oper_rc->shortcut, ware->shortcut);
    num_fromString(&oper_rc->price, ware->price);
    num_dup(&oper_rc->quantity, quantity);
    num_mul(&oper_rc->summ, &oper_rc->price, quantity);
    strcpy(oper_rc->barcode, barcode);
    gdk_threads_enter();
	mw_showText(ware->longtitle);
        mw_showCode(ware->ware_id);
	mw_showQuantity(quantity);
        mw_showPrice(&oper_rc->price);
	mw_showSum(&oper_rc->summ);
    gdk_threads_leave();
    internal_kkm_lock=TRUE;
    StartThread(RegistrationThread);
    struct {
	uint8_t err;
	uint8_t barcode_type;
	uint8_t name[40];
	uint8_t price[5];
	uint8_t depart;
	uint8_t tax[4];
	uint8_t quantity[5];
        } __attribute__ ((packed)) msg;
    bzero(&msg, sizeof(msg));
    char str[41];
    size_t l=kkm_textToKKM(str, sizeof(str), ware->shortcut);
    memset(msg.name, 0x20, sizeof(msg.name));
    memcpy(msg.name, str, l);
    memcpy(msg.price, &oper_rc->price.value, sizeof(msg.price));
    msg.depart=1;
    memcpy(buf, &msg, sizeof(msg));
    return sizeof(msg);
    }    

static int BarcodeProcess(void * buf) {
//puts("* BarcodeProcess()");
    GetTime(TIME_BEGIN);
    struct {
	uint8_t check_type;    
	uint8_t storno;
	uint8_t quantity[5];
	uint8_t seller;
	uint8_t barcode[14];
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));
//printf("bcode: <%s>\n", msg.barcode);
    if (msg.storno==1 && global_conf_storno_access==0 && !global_access_flag) return Forbidden();

    NUMERIC(quantity, 3);
    memcpy(&quantity.value, msg.quantity, sizeof(msg.quantity));

    switch(msg.check_type) {
    case 1: oper_check_type=DOC_SALE; break;
    case 3: 
	if (global_conf_return_access==0 && !global_access_flag) return Forbidden();
	oper_check_type=DOC_RETURN;
	break;
    default: 
        gdk_threads_enter();
	    mw_showText("Неверный статус документа");
        gdk_threads_leave();
	return -10;
	}

    oper_seller=msg.seller;
//    if (global_doc_state==DOC_CLOSED) global_doc_state=oper_check_type;
    gdk_threads_enter();
        mw_wheelOn();
    gdk_threads_leave();
//printf("barcode: <%s>\n", msg.barcode);
    if (msg.storno) {
	if (!strcmp(msg.barcode, "0000000000000")) {
	    return NumStorno(NULL, buf);
	    }
	if (strlen(msg.barcode)<5) {
	    return NumStorno(msg.barcode, buf);
	    }
	return BarcodeStorno(msg.barcode, &quantity, buf);
	}
    if (global_doc_state==DOC_CLOSED) {
//	global_doc_state=oper_check_type;
	mw_agentLock(TRUE);
	}
    return BarcodeRegistration(msg.barcode, &quantity, buf);
    }

/************************** cancel_process() ******************************/
void CancelThread(void) {
    int r=actions_processCancel();
    global_doc_state=DOC_CLOSED;
    receipt_clear();
    num_clear(&global_payment_summ);
    num_clear(&global_total_summ);
    if (r<0) log_db_fail("АННУЛИРОВАНИЕ");
    gdk_threads_enter();
	if (r<0) {
	    mw_showText("Ошибка обращения к БД");
    	    mw_showDocState(DOC_ERROR, global_doc_num+1);
	    }
	else {
    	    mw_showText("Чек аннулирован");
    	    mw_showDocState(DOC_CANCEL, global_doc_num+1);
	    }
	mw_clearTable();
	mw_showTotal(&global_total_summ);
	mw_accessDisable();
	if (global_session_state!=SESSION_ERROR) mw_stopOff();
    gdk_threads_leave();
    internal_kkm_lock=FALSE;
    }

static int CancelProcess(void * data) {
    if (global_conf_cancel_access==0 && !global_access_flag) return Forbidden();

    gdk_threads_enter();
	mw_showText("Аннулирование чека...");
    gdk_threads_leave();
    struct {
	uint8_t seller;
	uint8_t check_type;
	uint8_t date[3];
	uint8_t time[3];
	uint16_t n_doc;
	uint16_t n_check;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, data, sizeof(msg));
    oper_seller=msg.seller;
    switch(msg.check_type) {
    case 1: oper_check_type=DOC_SALE; break;
    case 3: oper_check_type=DOC_RETURN; break;
    default: 
        gdk_threads_enter();
	    mw_showText("Неверный статус документа");
        gdk_threads_leave();
	return -10;
	}
    commands_getTm(&oper_date_time, msg.date, msg.time);
    oper_doc_num=msg.n_doc;
    oper_check_num=msg.n_check;
    internal_kkm_lock=TRUE;
    StartThread(CancelThread);
#ifdef OPERATION_JOURNAL
    control_receipt("АННУЛИРОВАНИЕ", msg.n_doc, msg.n_check, oper_check_type);
#endif
    return 0;
    }

/************************** payment process *******************************/
void PaymentThread(void) {
    g_mutex_lock(internal_mutex);
    int r=actions_processPayment();
    g_mutex_unlock(internal_mutex);
    num_addThis(&global_payment_summ, &oper_summ);
    if (r<0) {
        log_db_fail("ОПЛАТА");
        global_doc_state=DOC_ERROR;
	};
    gdk_threads_enter();
	if (r<0) {
    	    mw_showText("Ошибка обращения к БД");
	    mw_showDocState(DOC_ERROR, global_doc_num+1);
	    mw_stopOn();
	    }
	mw_wheelOff();
    gdk_threads_leave();
    }

static int PaymentProcess(void * buf) {
    struct {
	uint8_t payment_type;
	uint8_t check_type;
	uint8_t mode;
	uint8_t sum[5];
	uint8_t seller;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));

    if (msg.mode!=0) {
        gdk_threads_enter();
	mw_showText("ККМ: неверная операция");
        gdk_threads_leave();
	return -10;
	}
    
    oper_payment_type=msg.payment_type;
    switch(msg.check_type) {
    case 1: oper_check_type=DOC_SALE; break;
    case 3: oper_check_type=DOC_RETURN; break;
    default: 
        gdk_threads_enter();
	    mw_showText("Неверный статус документа");
        gdk_threads_leave();
	return -10;
	}
    oper_seller=msg.seller;

    num_init(&oper_summ, 2);
    memcpy(&oper_summ.value, msg.sum, sizeof(msg.sum));

    char str[64];
    switch (oper_check_type) {
    case DOC_SALE: strcpy(str,"Оплата: "); break;
    case DOC_RETURN: strcpy(str,"Возврат: "); break;
	}
    num_toString(strchr(str,0), &oper_summ);
    gdk_threads_enter();
	mw_showText(str);
	mw_wheelOn();
    gdk_threads_leave();
    StartThread(PaymentThread);
#ifdef OPERATION_JOURNAL
    control_payment(&oper_summ, msg.payment_type);
#endif
    return 0;
    }

int summ_process(void *buf) {
    struct {
	uint8_t check_type;
	uint8_t mode;
	uint8_t sum[5];
	uint8_t seller;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));

    if (msg.mode!=0) {
	gdk_threads_enter();
	    mw_showText("ККМ: неверная операция");
        gdk_threads_leave();
	return -10;
	}
    
    NUMERIC(summ, 2);
    memcpy(&summ.value, msg.sum, sizeof(msg.sum));

    struct {
	uint8_t err;
	uint8_t payment_type;
	uint8_t sum[5];
	} __attribute__ ((packed)) answer;
    bzero(&answer, sizeof(answer));
    answer.payment_type=2;
    memcpy(answer.sum, msg.sum, sizeof(answer.sum));
    memcpy(buf, &answer, sizeof(answer));
    return sizeof(answer);
    }

/**************************** receipt process ****************************/    
void ReceiptThread(void) {
    char str[64];
    NUMERIC(return_summ, 2);
    g_mutex_lock(internal_mutex);
    int r=actions_processReceipt();
    g_mutex_unlock(internal_mutex);
    bzero(str,sizeof(str));
    num_sub(&return_summ, &global_payment_summ, &global_total_summ);
    switch (global_doc_state) {
    case DOC_RETURN:
        global_return_num=oper_check_num;
        strcpy(str,"Возврат: ");
        num_toString(strchr(str,0), &global_total_summ);
        break;
    case DOC_SALE:
        global_sale_num=oper_check_num;
        strcpy(str,"Сдача: ");
        num_toString(strchr(str,0), &return_summ);
        break;
        }
    global_doc_num=oper_doc_num;
    global_doc_state=DOC_CLOSED;
    global_session_state=SESSION_OPEN;
    receipt_clear();
    num_clear(&global_payment_summ);
    num_clear(&global_total_summ);

    gdk_threads_enter();
	mw_clearTable();
	if (r<0) {
	    log_db_fail("ЗАКРЫТИЕ ЧЕКА");
    	    mw_showText("Ошибка обращения к БД");
	    mw_showDocState(DOC_ERROR, global_doc_num);
	    }
	else {
    	    mw_showText(str);
	    mw_showDocState(DOC_CLOSED, 0);
	    }
        mw_showSession(SESSION_OPEN, global_session_num+1);
	mw_showTotal(&global_total_summ);
	mw_accessDisable();
	mw_wheelOff();
    gdk_threads_leave();
    internal_kkm_lock=FALSE;
    }

static int ReceiptProcess(void * buf) {
    struct {
	uint8_t seller;
	uint8_t check_type;
	uint8_t date[3];
	uint8_t time[3];
	uint16_t n_doc;
	uint16_t n_check;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));

    commands_getTm(&oper_date_time, msg.date, msg.time);
    oper_seller=msg.seller;
    oper_check_num=msg.n_check;
    oper_doc_num=msg.n_doc;
    switch(msg.check_type) {
    case 1: oper_check_type=DOC_SALE; break;
    case 3: oper_check_type=DOC_RETURN; break;
    default: 
        gdk_threads_enter();
	    mw_showText("Неверный статус документа");
        gdk_threads_leave();
	return -10;
	}
    internal_kkm_lock=TRUE;
    gdk_threads_enter();
	mw_wheelOn();
    gdk_threads_leave();
    StartThread(ReceiptThread);
#ifdef OPERATION_JOURNAL
    control_receipt("ЗАКРЫТИЕ ЧЕКА", msg.n_doc, msg.n_check, oper_check_type);
#endif
    struct {
	uint8_t err;
	uint8_t tax[5][4];
	} __attribute__ ((packed)) answer;
    bzero(&answer, sizeof(answer));
    memcpy(buf, &answer, sizeof(answer));
    return sizeof(answer);
    }

/**************************** session process ********************************/
static int SessionAction(void) {
    gdk_threads_enter();
	mw_wheelOn();
    gdk_threads_leave();
    int r=actions_processSession();
    global_doc_num=oper_doc_num;
    global_session_num=oper_check_num;
    global_session_state=SESSION_CLOSED;
    if (r<0) log_db_fail("ЗАКРЫТИЕ СМЕНЫ");
    gdk_threads_enter();
	if (r<0) mw_showText("Ошибка обращения к БД");
	mw_showSession(SESSION_CLOSED, 0);
	mw_accessDisable();
	mw_stopOff();
    gdk_threads_leave();
    usleep(COMMAND_INTERVAL);
    r=kkm_sync();
    if (r<0) {
	log_puts(0, "СИНХРОНИЗАЦИЯ: Ошибка обмена данными");
        gdk_threads_enter();
    	    mw_showText("Ошибка обмена данными");
    	    mw_wheelOff();
        gdk_threads_leave();
	return -1;
	}
    if (r==0) {
	log_puts(0, "СИНХРОНИЗАЦИЯ: Ошибка ККМ");
        gdk_threads_enter();
    	    mw_showText("Ошибка ККМ");
    	    mw_wheelOff();
        gdk_threads_leave();
	return 0;
	}
    gdk_threads_enter();
	mw_showText("Смена закрыта");    
	mw_wheelOff();
    gdk_threads_leave();
    return 0;
    }

static int SessionProcess(void * buf) {
    struct {
	uint8_t sum[6];
	uint8_t date[3];
	uint8_t time[3];
	uint8_t seller;
	uint16_t n_doc;
	uint16_t n_session;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));
    gdk_threads_enter();
	mw_showText("Закрытие смены...");
    gdk_threads_leave();
    commands_getTm(&oper_date_time, msg.date, msg.time);
    oper_seller=msg.seller;
    oper_doc_num=msg.n_doc;
    oper_check_num=msg.n_session-1;
    num_init(&oper_summ,2);
    memcpy(&oper_summ.value, msg.sum, sizeof(msg.sum));
#ifdef OPERATION_JOURNAL
    control_session(&oper_summ, msg.n_doc, msg.n_session);
    control_close();
#endif
    kkm_result_handler=SessionAction;
    return 0;
    }

/****************************************************************************/
static int KeyProcess(void * buf) {
    struct {
	uint8_t check_type;    
	uint8_t storno;
	uint8_t quantity[5];
	uint8_t price[5];
	uint8_t depart;
	uint8_t seller;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));
    switch (msg.depart) {
    case 4: receipt_downRecord(); return 0;
    case 5: receipt_upRecord(); return 0;
	}
    return -1;
    }

/****************************************************************************/
static int CodeError(void * buf) {
    struct {
	uint8_t check_type;
	uint8_t storno;
	uint8_t quantity[5];
	uint8_t code[5];
	uint8_t seller;
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));
    if (msg.storno==1) return -10;

    struct {
	uint8_t err;
        uint8_t name[40];
        uint8_t price[5];
        uint8_t depart;
        uint8_t tax[4];
        } __attribute__ ((packed)) answer;
    bzero(&answer, sizeof(answer));
    char str[41];
    size_t l=kkm_textToKKM(str, sizeof(str), "!!! ОШИБКА !!!");
    memset(answer.name, 0x20, sizeof(answer.name));
    memcpy(answer.name, str, l);
    answer.depart=1;
    memcpy(buf, &answer, sizeof(answer));
    return sizeof(answer);
    }

static int BarcodeError(void * buf) {
    struct {
	uint8_t check_type;    
	uint8_t storno;
	uint8_t quantity[5];
	uint8_t seller;
	uint8_t barcode[14];
	} __attribute__ ((packed)) msg;
    memcpy(&msg, buf, sizeof(msg));
    if (msg.storno==1) return -10;
    struct {
	uint8_t err;
	uint8_t barcode_type;
	uint8_t name[40];
	uint8_t price[5];
	uint8_t depart;
	uint8_t tax[4];
	uint8_t quantity[5];
        } __attribute__ ((packed)) answer;
    bzero(&answer, sizeof(answer));
    char str[41];
    size_t l=kkm_textToKKM(str, sizeof(str), "!!! ОШИБКА !!!");
    memset(answer.name, 0x20, sizeof(answer.name));
    memcpy(answer.name, str, l);
    answer.depart=1;
    memcpy(buf, &answer, sizeof(answer));
    return sizeof(answer);
    }

static int ErrorHandler(int prefix, void * buf) {
    switch(prefix) {
    case 0x81:	return CodeError(buf);
    case 0x82:	return BarcodeError(buf);
	}
    return -10;
    }

static int ReceiptHandler(int prefix, void * buf) {
    if (global_doc_state==DOC_ERROR) {
	gdk_threads_enter();
	    mw_showText("Необходимо аннулировать чек");
        gdk_threads_leave();
	return ErrorHandler(prefix, buf);
	}
    if (global_session_state==SESSION_ERROR) {
	gdk_threads_enter();
    	    mw_showText("Необходимо закрыть смену");
        gdk_threads_leave();
	return ErrorHandler(prefix, buf);
	}
    switch (prefix) {
    case 0x81: return CodeProcess(buf);
    case 0x82: return BarcodeProcess(buf);
    case 0x87: return PaymentProcess(buf);
    case 0x90: return ReceiptProcess(buf);
	}
    return Forbidden();
    }

int process_event(int prefix, void * buf) {
//printf("*** prefix: %02x ***\n", prefix);
    int r=-10;
    if (internal_kkm_lock) r=-2;
    else {
        switch (prefix) {
	case 0x50: return CashProcess(DOC_CASHIN, buf);
        case 0x51: return CashProcess(DOC_CASHOUT, buf);
	case 0x52: return SessionProcess(buf);
        case 0x91: return CancelProcess(buf);
	case 0x80:
	    if (KeyProcess(buf)==0) return -56;
	    break;
        default: return ReceiptHandler(prefix, buf);
	    }
	}
    return r;
    }
