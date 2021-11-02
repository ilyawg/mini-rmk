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
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/kd.h>
#include <gtk/gtk.h>

#include "conf.h"
#include "global.h"
#include "process.h"
#include "actions.h"
#include "kkm.h"
#include "protocol.h"
#include "commands.h"
#include "receipt.h"
#include "main_window.h"
#include "receipt_window.h"
#include "cash_window.h"
#include "localdb.h"
#include "global.h"
#include "../numeric.h"
#include "../recordset.h"

static int internal_code;
static char internal_barcode[20];
static num_t internal_summ;
static num_t internal_quantity;

static int StartThread(void (*thread_func)(void)) {
    mw_wheelOn();
    scanner_disable();
    GError * err=NULL;
    GThread * tid=g_thread_create((GThreadFunc)thread_func, NULL, FALSE, &err);
    if (tid==NULL) {
	if (err!=NULL)
	    fprintf(stderr,"* g_thread_create failed: %s\n", err->message);
	else fputs("* g_thread_create failed: unknown error\n",stderr);
	mw_showText("Операция не выполнена");
	mw_wheelOff();
	scanner_enable(TRUE);
	return -1;
	}
    return 0;
    }

static void log_status(const char *operation) {
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

static void LogDBFail(const char * operation) {
    log_status(operation);
    log_puts(1, "* Ошибка обращения к БД");
    }

static void LogKKMError(const char * operation, int err) {
    if (err==-256) log_printf(0, "ККМ: <%s> - Нарушение связи", operation);
    else log_printf(0, "ККМ: <%s> ОШИБКА %d", operation, -err);
    }
    
static void LogOperationFail(const char * operation) {
    log_status(operation);
    log_puts(1, "* операция не выполнена");
    }

/************************************************************************************************/
static void XReportThread(void) {
    const char * oper_name="ОТЧЕТ БЕЗ ГАШЕНИЯ";
    int r=commands_xreport();
    if (r<0) {
	LogKKMError(oper_name, r);
	LogOperationFail(oper_name);
	kkm_error(r, global_session_state!=SESSION_ERROR);
	}
    else {
	gdk_threads_enter();
    	    mw_wheelOff();
	    mw_clearText();
	    mw_unlock();
	gdk_threads_leave();
	if (global_session_state!=SESSION_ERROR) scanner_enable(FALSE);
	}
    }
	
/*************************************************************************************************/
static void CashInThread(void) {
    const char * oper_name="ВНЕСЕНИЕ";
    char str[64];
    int r=commands_cash(CMD_CASHIN, &internal_summ);
    if (r<0) {
	LogKKMError(oper_name, r);
	LogOperationFail(oper_name);
	kkm_error(r, global_session_state!=SESSION_ERROR);
	}
    else {
	strcpy(str,"Внесено: ");
	num_toString(strchr(str,0), &internal_summ);
	global_doc_num=r;
	gdk_threads_enter();
	    mw_showText(str);
    	    mw_showDocState(DOC_CLOSED, 0);
    	    mw_wheelOff();
	    mw_unlock();
	gdk_threads_leave();
	if (global_session_state!=SESSION_ERROR) scanner_enable(FALSE);
	}
    }

static void CashOutThread(void) {
    const char * oper_name="ВЫПЛАТА";
    char str[64];
    int r=commands_cash(CMD_CASHOUT, &internal_summ);
    if (r<0) {
	LogKKMError(oper_name, r);
	LogOperationFail(oper_name);
	kkm_error(r, global_session_state!=SESSION_ERROR);
	}
    else {
	strcpy(str,"Изъято: ");
	num_toString(strchr(str,0), &internal_summ);
	global_doc_num=r;
	gdk_threads_enter();
	    mw_showText(str);
    	    mw_showDocState(DOC_CLOSED, 0);
    	    mw_wheelOff();
	    mw_unlock();
	gdk_threads_leave();
	if (global_session_state!=SESSION_ERROR) scanner_enable(FALSE);
	}
    }

/*************************************************************************************************/
static void CancelThread(void) {
    const char * oper_name="АННУЛИРОВАНИЕ";
    gdk_threads_enter();
	mw_showText("Аннулирование чека...");
    gdk_threads_leave();
    struct tm ts;
    int n_session, n_doc, check_type, mode;
    int r=commands_getState(KKM_DATE, &ts,
			    KKM_SESSION_NUM, &n_session,
			    KKM_DOC_NUM, &n_doc,
			    KKM_MODE, &mode,
			    END_ARGS);

    if (r<0) LogKKMError("ЗАПРОС СОСТОЯНИЯ", r);
    else {
	r=commands_cancel();
	if (r<0) LogKKMError(oper_name, r);
	}
    if (r<0) {
	LogOperationFail(oper_name);
	kkm_error(r, global_doc_state!=DOC_ERROR && global_session_state!=SESSION_ERROR);
	}
    else {
	r=actions_processCancel(&ts, n_session+1, global_check_num+1, n_doc+1, global_doc_state);
	global_doc_state=DOC_CLOSED;
        receipt_clear();
	if (r<0) LogDBFail(oper_name);
        gdk_threads_enter();
    	    mw_wheelOff();
	    if (r<0) {
		mw_showText("Ошибка обращения к БД");
    		mw_showDocState(DOC_ERROR, global_doc_num+1);
		}
	    else {
    		mw_showText("Чек аннулирован");
    		mw_showDocState(DOC_CANCEL, global_doc_num+1);
		}
    	    mw_clearTable();
    	    mw_showTotal(NULL);
	    if (global_session_state!=SESSION_ERROR) mw_stopOff();
	    mw_unlock();
	gdk_threads_leave();
	if (global_session_state!=SESSION_ERROR) scanner_enable(FALSE);
	}
    }
	
/***********************************************************************************************/
static void TotalThread(void) {
    const char * oper_name="ИТОГ";
    char text[32];
    strcpy(text, "К оплате: ");
    num_init(&internal_summ, 2);
    int r=commands_total(&internal_summ);
    if (r<0) {
	LogKKMError(oper_name, r);
	LogOperationFail(oper_name);
	kkm_error(r, FALSE);
	}
    else {
	if (global_receipt_summ!=NULL && num_cmp(&internal_summ, global_receipt_summ)!=0) {
	    global_doc_state=DOC_ERROR;
	    gdk_threads_enter();
    		mw_wheelOff();
		mw_showText("Необходимо аннулировать чек");
		mw_showDocState(DOC_ERROR, global_doc_num);
		mw_stopOn();
	    gdk_threads_leave();
	    }
	else {
	    num_toString(strchr(text, 0), &internal_summ);
	    gdk_threads_enter();
    		mw_wheelOff();
		mw_showText(text);
	    gdk_threads_leave();
	    }
	g_idle_add((GSourceFunc)receiptWindow_create, &internal_summ);
	}
    }

/***********************************************************************************************/
static void CashThread(void) {
    const char * oper_name="СУММА ЯЩИКА";
    num_init(&internal_summ, 2);
    int r=commands_moneyReg(241, &internal_summ);
    if (r<0) {
	LogKKMError(oper_name, r);
	LogOperationFail(oper_name);
	kkm_error(r, FALSE);
	}
    else {
        gdk_threads_enter();
    	    mw_wheelOff();
	gdk_threads_leave();
	g_idle_add((GSourceFunc)cashWindow_create, &internal_summ);
	}
    }

/************************************************************************************************/
static void ReceiptThread(void) {
    const char * oper_name="ЗАКРЫТИЕ ЧЕКА";
    char str[64];
    NUMERIC(summ, 2);
    num_cpy(&summ, &internal_summ);
    struct tm ts;
    int n_session, n_doc, check_type;
    int r=commands_getState(KKM_DATE, &ts,
			    KKM_SESSION_NUM, &n_session,
			    KKM_DOC_NUM, &n_doc,
			    END_ARGS);

    if (r<0) LogKKMError("ЗАПРОС СОСТОЯНИЯ", r);
    else {
	r=commands_closeReceipt(&summ);
	if (r<0) LogKKMError(oper_name, r);
	}
    if (r<0) {
	LogOperationFail(oper_name);
	kkm_error(r, TRUE);
	}
    else {
        r=actions_processReceipt(&ts, n_session+1, global_check_num+1, n_doc+1, global_doc_state, &internal_summ);

	bzero(str,sizeof(str));
	switch (global_doc_state) {
	case DOC_RETURN:
    	    strcpy(str,"Возврат: ");
    	    num_toString(strchr(str,0), global_receipt_summ);
    	    break;
	case DOC_SALE:
    	    strcpy(str,"Сдача: ");
    	    num_toString(strchr(str,0), &summ);
    	    break;
    	    }

	global_doc_state=DOC_CLOSED;
	global_session_num=n_session;
	global_session_state=SESSION_OPEN;
	receipt_clear();
	gdk_threads_enter();
	    mw_clearTable();
	    if (r<0) {
		LogDBFail(oper_name);
    		mw_showText("Ошибка обращения к БД");
		mw_showDocState(DOC_ERROR, global_doc_num);
		}
	    else {
    		mw_showText(str);
		mw_showDocState(DOC_CLOSED, 0);
		}
    	    mw_showSession(SESSION_OPEN, global_session_num+1);
	    mw_showTotal(NULL);
	    mw_wheelOff();
	    mw_unlock();
	gdk_threads_leave();
	scanner_enable(FALSE);
	}
    }

/***********************************************************************************************/
static void SessionThread(void) {
    const char * oper_name="ЗАКРЫТИЕ СМЕНЫ";
    NUMERIC(summ, 2);
    int n_session;
    int n_doc;
    struct tm ts;
    int r=commands_getState(KKM_SESSION_NUM, &n_session,
			    KKM_DOC_NUM, &n_doc,
			    KKM_DATE, &ts,
			    END_ARGS);
    if (r<0) LogKKMError("ЗАПРОС СОСТОЯНИЯ", r);
    else {
	r=commands_moneyReg(193, &summ);
	if (r<0) LogKKMError("СМЕННЫЙ ИТОГ", r);
	else {
	    r=commands_zreport();
	    if (r<0) LogKKMError("ОТЧЕТ С ГАШЕНИЕМ", r);
	    }
	}
    if (r<0) {
	LogOperationFail(oper_name);
	kkm_error(r, global_session_state!=SESSION_ERROR);
	}
    else {
	r=actions_processSession(&ts, n_session+1, n_doc+1, &summ);
	global_doc_num=n_doc+1;
	global_session_state=SESSION_CLOSED;
        if (r<0) LogDBFail(oper_name);
        gdk_threads_enter();
    	    mw_wheelOff();
	    if (r<0) mw_showText("Ошибка обращения к БД");
	    else mw_showText("Смена закрыта");    
	    mw_showSession(SESSION_CLOSED, 0);
	    mw_stopOff();
	    mw_unlock();
	gdk_threads_leave();
	scanner_enable(FALSE);
	}
    }

/***********************************************************************************************/
static void ReturnThread(void) {
    const char * oper_name="ВОЗВРАТ";
    int n_doc, n_check;
    mw_agentLock(TRUE);
    int r=commands_operReg(152);
    if (r<0) LogKKMError("НОМЕР ДОКУМЕНТА", r);
    else {
	n_doc=r;
	r=commands_operReg(150);
	if (r<0) LogKKMError("НОМЕР ЧЕКА", r);
	else {
	    n_check=r;
	    r=commands_open(KKM_DOC_RETURN);
	    if (r==-55) r=commands_registration(CMD_RETURN, NULL, NULL, NULL);
	    if (r<0) LogKKMError(oper_name, r);
	    }
	}
    if (r<0) {
	LogOperationFail(oper_name);
	kkm_error(r, TRUE);
	mw_agentLock(FALSE);
	return;
	}
    global_doc_num=r;
    global_check_num=r;
    global_doc_state=DOC_RETURN;
    gdk_threads_enter();
	mw_wheelOff();
	mw_showDocState(global_doc_state, global_doc_num+1);
	mw_unlock();
    gdk_threads_leave();
    scanner_enable(FALSE);
    }
	    
/***********************************************************************************************/
static void StornoThread(void) {
//printf("storno: <%s>\n", num_s);
    const char * oper_name="СТОРНО";
    num_t quantity, summ;
    struct record_receipt * rc=mw_current_record;
    if (rc==NULL || num_cmp0(&rc->quantity)==0) {;
        gdk_threads_enter();
    	    mw_wheelOff();
	    mw_showText("Нет позиции для сторно");
	    mw_unlock();
        gdk_threads_leave();
	scanner_enable(TRUE);
	return;
	}
    num_dup(&quantity, &rc->quantity); num_negThis(&quantity);
    num_dup(&summ, &rc->summ); num_negThis(&summ);
    gdk_threads_enter();
	mw_showText("Сторно");
        mw_showCode(rc->code);
	mw_showQuantity(&quantity);
        mw_showPrice(&rc->price);
	mw_showSum(&summ);
    gdk_threads_leave();
    char str[51];
    size_t l=kkm_textToKKM(str, sizeof(str), rc->shortcut);
    int r=commands_registration(CMD_STORNO,
			        &rc->price,
			        &rc->quantity,
			        str);
    if (r<0) {
	LogKKMError(oper_name, r);
	LogOperationFail(oper_name);
	kkm_error(r, TRUE);
	}
    else {
	r=actions_processStorno(rc->id, &quantity, &summ, global_doc_state);
        num_addThis(&rc->quantity, &quantity);
	num_addThis(&rc->summ, &summ);
	num_addThis(global_receipt_summ, &summ);
	if (r<0) {
	    LogDBFail(oper_name);
    	    global_doc_state=DOC_ERROR;
	    }
	gdk_threads_enter();
	    if (r<0) {
		mw_showDocState(DOC_ERROR, global_doc_num+1);
		mw_showText("Ошибка обращения к БД");
		mw_stopOn();
		}
    	    mw_showRecord(rc);
	    mw_showTotal(global_receipt_summ);
	    mw_wheelOff();
	    mw_unlock();
	gdk_threads_leave();
	if (global_doc_state!=DOC_ERROR) scanner_enable(FALSE);
	}
    }

/************************************************************************************************/
static void ExecRegistration(struct record_receipt * rc) {
    const char * oper_name="РЕГИСТРАЦИЯ";
    char str[51];
    size_t l=kkm_textToKKM(str, sizeof(str), rc->shortcut);
    int r, n_doc, n_check;
    if (global_doc_state==DOC_CLOSED) {
	r=commands_operReg(152);
	if (r<0) LogKKMError("НОМЕР ДОКУМЕНТА", r);
	else {
	    n_doc=r;
	    r=commands_operReg(148);
	    if (r<0) LogKKMError("НОМЕР ЧЕКА", r);
	    else n_check=r;
	    }
	if (r<0) {
	    LogOperationFail(oper_name);
	    kkm_error(r, TRUE);
	    mw_agentLock(FALSE);
	    return;
	    }
	}
    r=commands_registration((global_doc_state==DOC_RETURN)?CMD_RETURN:CMD_SALE,
			    &rc->price,
			    &rc->quantity,
			    str);
    if (r<0) {
	LogKKMError(oper_name, r);
	LogOperationFail(oper_name);
	kkm_error(r, TRUE);
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
	return;
	}

    if (global_doc_state==DOC_CLOSED) {
	global_doc_num=n_doc;
	global_check_num=n_check;
    	global_doc_state=DOC_SALE;
	}
    r=actions_processRegistration(rc,
				  global_session_num+1,
				  global_check_num+1,
				  global_doc_num+1,
				  global_doc_state);
    if (r<0) {
        LogDBFail(oper_name);
	global_doc_state=DOC_ERROR;
	}
    num_addThis(global_receipt_summ, &rc->summ);
    gdk_threads_enter();
	if (r<0) {
	    mw_showText("Ошибка обращения к БД");
	    mw_stopOn();
	    }
	rc->path=mw_newRecord();
	mw_showDocState(global_doc_state, global_doc_num+1);
	mw_showRecord(rc);
	mw_showTotal(global_receipt_summ);
	mw_wheelOff();
    	mw_unlock();
	gdk_threads_leave();
	g_idle_add((GSourceFunc)mw_setCursor, rc);
	receipt_addRecord(rc);
	if (global_doc_state!=DOC_ERROR) scanner_enable(FALSE);
    }

static void BarcodeThread(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (global_doc_state==DOC_CLOSED) mw_agentLock(TRUE);
    int r=localdb_getWareByBarcode(&rset, internal_barcode);
    if (r>1) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
	    mw_showText("Штрихкод не уникален");
	    mw_wheelOff();
    	    mw_unlock();
        gdk_threads_leave();
	recordset_destroy(&rset);
	scanner_enable(TRUE);
	return;
	}

    struct ware_dbrecord * ware=NULL;
    if (r==1) ware=recordset_begin(&rset);
    if (ware==NULL) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
    	    mw_showText("Товар не найден");
	    mw_wheelOff();
    	    mw_unlock();
        gdk_threads_leave();
	recordset_destroy(&rset);
	scanner_enable(TRUE);
	return;
	}
    struct record_receipt * rc=receipt_newRecord();
    rc->code=ware->ware_id;
    strcpy(rc->longtitle, ware->longtitle);
    strcpy(rc->shortcut, ware->shortcut);
    num_fromString(&rc->price, ware->price);
    num_dup(&rc->quantity, &internal_quantity);
    num_mul(&rc->summ, &rc->price, &rc->quantity);
    strcpy(rc->barcode, internal_barcode);
    recordset_destroy(&rset);

    gdk_threads_enter();
	mw_showText(rc->longtitle);
        mw_showCode(rc->code);
	mw_showQuantity(&internal_quantity);
        mw_showPrice(&rc->price);
	mw_showSum(&rc->summ);
    gdk_threads_leave();
    ExecRegistration(rc);
    }    

static void CodeThread(void) {
    struct ware_dbrecord ware;
    ware.ware_id=internal_code;
    if (global_doc_state==DOC_CLOSED) mw_agentLock(TRUE);
    int r=localdb_getWare(&ware);
    if (r<=0) {
	if (global_doc_state==DOC_CLOSED) mw_agentLock(FALSE);
        gdk_threads_enter();
    	    mw_showText("Неверный код товара");
	    mw_wheelOff();
    	    mw_unlock();
        gdk_threads_leave();
	scanner_enable(TRUE);
        return;
        }

    struct record_receipt * rc=receipt_newRecord();
    rc->code=ware.ware_id;
    strcpy(rc->longtitle, ware.longtitle);
    strcpy(rc->shortcut, ware.shortcut);
    num_fromString(&rc->price, ware.price);
    num_dup(&rc->quantity, &internal_quantity);
    num_mul(&rc->summ, &rc->price, &internal_quantity);
    gdk_threads_enter();
	mw_showText(ware.longtitle);
        mw_showCode(ware.ware_id);
	mw_showQuantity(&internal_quantity);
        mw_showPrice(&rc->price);
	mw_showSum(&rc->summ);
    gdk_threads_leave();
    ExecRegistration(rc);
    }

/************************************************************************************************/

static int event3(int id, va_list ap) {
    if (global_doc_state==DOC_ERROR) {
	mw_showText("Необходимо аннулировать чек");
	return -1;
	}
    if (global_session_state==SESSION_ERROR) {
    	mw_showText("Необходимо закрыть смену");
	return -1;
	}

    switch (id) {

    case RECEIPT_EVENT:
	num_init(&internal_summ, 2);
	num_cpy(&internal_summ, va_arg(ap, num_t*));
	return StartThread(ReceiptThread);

    case RETURN_EVENT:
	if (global_conf_return_access==0 && !global_access_flag) break;
	mw_showText("Возврат");
	return StartThread(ReturnThread);

    case CODE_EVENT:
	internal_code=va_arg(ap, int);
	num_dup(&internal_quantity, va_arg(ap, num_t*));
	return StartThread(CodeThread);

    case BARCODE_EVENT:
	mw_lock();
	strcpy(internal_barcode, va_arg(ap, char*));
	num_dup(&internal_quantity, va_arg(ap, num_t*));
	return StartThread(BarcodeThread);

    case STORNO_EVENT:
	if (global_conf_storno_access==0 && !global_access_flag) break;
	mw_lock();
	return StartThread(StornoThread);
	}
    if (global_doc_state!=DOC_ERROR && global_session_state!=SESSION_ERROR) scanner_enable(TRUE);
    mw_showText("Операция запрещена");
    mw_unlock();
    return -1;
    }    

static int event2(int id, va_list ap) {
    switch (id) {
    case XREPORT_EVENT:	return StartThread(XReportThread);

    case CASHIN_EVENT:
	num_dup(&internal_summ, va_arg(ap, num_t*));
	return StartThread(CashInThread);

    case CASHOUT_EVENT:
	num_dup(&internal_summ, va_arg(ap, num_t*));
	mw_showText("Выплата...");
	return StartThread(CashOutThread);


    case CANCEL_EVENT:
	if (global_conf_cancel_access==0 && !global_access_flag) break;
	return StartThread(CancelThread);

    case SESSION_EVENT:
	mw_showText("Закрытие смены...");
	return StartThread(SessionThread);

    default:	return event3(id, ap);
	}
    if (global_doc_state!=DOC_ERROR && global_session_state!=SESSION_ERROR) scanner_enable(TRUE);
    mw_showText("Операция запрещена");
    mw_unlock();
    return -1;
    }

static int event1(int id, va_list ap) {
    switch (id) {
    case CASH_EVENT:	
	mw_lock();
	return StartThread(CashThread);

    case TOTAL_EVENT:
	mw_lock();
	return StartThread(TotalThread);

    case SERVICE_OPEN_EVENT:
	mw_lock();
	mw_clearText();
	scanner_disable();
	serviceWindow_create();
	return 0;

    case WARE_OPEN_EVENT:
	mw_lock();
	mw_clearText();
//	scanner_disable();
	wareWindow_create();
	return 0;

    case WARE_CLOSE_EVENT:
	mw_unlock();
	return 0;
	
    case RECEIPT_CLOSE_EVENT:
    case SERVICE_CLOSE_EVENT:
    case CASH_CLOSE_EVENT:
	mw_unlock();
	if (global_doc_state!=DOC_ERROR && global_session_state!=SESSION_ERROR) scanner_enable(TRUE);
	return 0;
	}

    int r=event2(id, ap);
    if (r<0) return -1;
    mw_accessDisable();
    return 0;
    }

int process_event(int id, ...) {
    va_list ap;
    va_start(ap, id);
    int r=event1(id, ap);
    va_end(ap);
    return r;
    }
