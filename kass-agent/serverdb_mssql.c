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
#include <stdarg.h>
#include <string.h>

#include <sybdb.h>
#include <sqlfront.h>

#include "conf.h"
#include "localdb.h"
#include "serverdb.h"
#include "database_common.h"
#include "log.h"
#include "global.h"
#include "process.h"

#if defined WARE_PROCESS_LOCK || defined COMMIT_LOCK
#include <pthread.h>
extern pthread_mutex_t ware_mutex;
#endif

extern int * global_stop_flag;

#define dbintbind(_db, _column, _target)	dbbind(_db, _column, INTBIND, sizeof(DBINT), (BYTE*)&(_target))
#define dbstringbind(_db, _column, _target)	dbbind(_db, _column, NTBSTRINGBIND, sizeof(_target), (BYTE*)(_target))

static void internal_message(char * sql, char * err) {
    log_puts(0, sql);
    log_printf(1, "ОШИБКА SYBDB: %s", err);
    }

static int err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr) {
	if ((dbproc == NULL) || (DBDEAD(dbproc))) {
	    return (INT_CANCEL);
	    }
	if (dberr!=SYBECONN)
	    log_message(0, "DB-LIBRARY error: %s", dberrstr);
//	if (oserr != DBNOERR) {
//	    log_puts(0, "Operating-system error:");
//	    log_printf(1, "\t%s", oserrstr);
//	    }
	return (INT_CANCEL);
    }

static int msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line) {
    char text[1024];
    char str[64];
    strcpy(text, "DB-LIBRARY:");
    int empty=-1;
    if (severity>0) {
	if (strlen(srvname) > 0) {
    	    sprintf(str, " Server '%s'", srvname);
    	    strcat(text, str);
    	    empty=0;
    	    }
	if (strlen(procname) > 0) {
            sprintf(str, " Procedure '%s'", procname);
            if (!empty) strcat(text, ",");
            strcat(text, str);
            empty=0;
            }
        if (line > 0) {
            sprintf(str, "Line %d", line);
            if (!empty) strcat(text, ",");
            strcat(text, str);
            }
	log_puts(0, text);
	}

    if (severity>0 && msgno!=0) {
	strcpy(text, "\t");
	strcat(text, msgtext);
	log_puts(1, text);
	}
    return 0;
    }

static LOGINREC * local_dblogin;

static DBPROCESS * local_dbconn=NULL;

void serverdb_init(void) {
    dbinit();
    local_dblogin=dblogin();
    DBSETLUSER(local_dblogin, global_conf_srv_login);
    DBSETLPWD(local_dblogin, global_conf_srv_psw);

    dbmsghandle(msg_handler);
    dberrhandle(err_handler);
    }

int serverdb_connect(void) {
#ifdef DEBUG
    if (local_dblogin==NULL) {
	fputs("*** server_connect() ASSERT: not initialized\n", stderr);
	return -1;
	}	
    if (local_dbconn!=NULL) {
	fputs("*** server_connect() ASSERT: already connected\n", stderr);
	return 0;
	}	
#endif
    DBPROCESS * conn=dbopen(local_dblogin, global_conf_conn_alias);
    if (conn==NULL) return -1;
    RETCODE erc=dbuse(conn, global_conf_srv_base);
    if (erc==SUCCEED) {
	local_dbconn=conn;
	return 0;
	}
    dbclose(conn);
    return -1;
    }

void serverdb_disconnect(void) {
#ifdef DEBUG
    if (local_dbconn==NULL) {
	fputs("*** server_disconnect() ASSERT: is not connected\n", stderr);
	return;
	}	
#endif
    dbclose(local_dbconn);
    local_dbconn=NULL;
    }	

void serverdb_destroy(void) {
#ifdef DEBUG
    if (local_dbconn!=NULL) {
	fputs("*** server_destroy() ASSERT: is connected\n", stderr);
	return;
	}	
#endif
    dbloginfree(local_dblogin);
    dbexit();
    }

static int database_addString(char *trg, size_t size, char *src) {
    if (src==NULL) return database_addText(trg,size,"NULL");
    char * ptr=strchr(trg,0);
    char * end=trg+size;
    if (trg+2>=end) return -1;
    *ptr='\'';
    ptr++;
    while (*src!=0) {
	switch(*src) {
	case '\'':
	    if (ptr+2>=end) return -1;
	    *ptr='\'';
	    *(ptr+1)='\'';
	    ptr+=2;
	    break;
	default:
	    if (ptr+1>=end) return -1;
	    *ptr=*src;
	    ptr++;
	    break;
	    }
	src++;
	}
    if (ptr+1>=end) return -1;
    *ptr='\'';
    *(ptr+1)=0;
    ptr++;
    return 0;
    }

static int sybdb_execCmdText(DBPROCESS * conn, char * text) {
#ifdef DEBUG	
    printf("T-SQL: <%s>\n",text);
#endif
    dbcmd(conn, text);
    if (dbsqlexec(conn)!=SUCCEED) return -1;
    int rs=dbretstatus(local_dbconn);
    if (dbcanquery(local_dbconn)!=SUCCEED) return -1;
    return rs;
    }

static int sybdb_execCmd(DBPROCESS * conn, int t, ...) {
    char text[2048];
    bzero(text,sizeof(text));
    va_list ap;
    va_start(ap,t);
    int r=0;
    while (t!=END_ARGS) {
	switch(t) {
        case TEXT_VALUE: r=database_addText(text,sizeof(text),va_arg(ap,char*)); break;
        case STRING_VALUE: r=database_addString(text,sizeof(text),va_arg(ap,char*)); break;
        case UINT_VALUE: r=database_addUInt(text,sizeof(text),va_arg(ap,int)); break;
	default:
	    va_end(ap);
	    log_printf(0, "*** sybdb_execCmd() ASSERT: invalid value type: %d",t);
	    return -1;
	    }
	if (r) {
	    va_end(ap);
	    log_puts(0, "*** sybdb_execCmd() ASSERT: text buffer overflow");
	    return -1;
	    }
	t=va_arg(ap,int);
	}
    va_end(ap);
    return sybdb_execCmdText(conn, text);
    }

/********************************** ware update ****************************/
int serverdb_updateWare(int first_id, int fd) {
    enum {
	WARE_ID_COLUMN=1,
	GROUP_ID_COLUMN,
	LONGTITLE_COLUMN,
	SHORTCUT_COLUMN,
	PRICE_COLUMN
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT ware_id, group_id, longtitle, shortcut, price "
		"FROM ware WHERE counter>="), AS_UINT(first_id),    
	END_ARGS)<0) return -1;

    RETCODE erc=dbresults(local_dbconn);
    if (erc!=SUCCEED) return -1;

    struct {
	DBINT ware_id; int ware_id_isnull;
	DBINT group_id; int group_id_isnull;
	DBCHAR longtitle[512]; int longtitle_isnull;
	DBCHAR shortcut[512]; int shortcut_isnull;
	DBCHAR price[16]; int price_isnull;
	} v;
    dbnullbind(local_dbconn, WARE_ID_COLUMN, &v.ware_id_isnull);
	dbintbind(local_dbconn, WARE_ID_COLUMN, v.ware_id);
    dbnullbind(local_dbconn, GROUP_ID_COLUMN, &v.group_id_isnull);
	dbintbind(local_dbconn, GROUP_ID_COLUMN, v.group_id);
    dbnullbind(local_dbconn, LONGTITLE_COLUMN, &v.longtitle_isnull);
        dbstringbind(local_dbconn, LONGTITLE_COLUMN, v.longtitle);
    dbnullbind(local_dbconn, SHORTCUT_COLUMN, &v.shortcut_isnull);
        dbstringbind(local_dbconn, SHORTCUT_COLUMN, v.shortcut);
    dbnullbind(local_dbconn, PRICE_COLUMN, &v.price_isnull);
        dbstringbind(local_dbconn, PRICE_COLUMN, v.price);
    int trans=0;
    struct ware_dbrecord rec;
    while(!*global_stop_flag) {
	bzero(&v ,sizeof(v));
	erc=dbnextrow(local_dbconn);
	if (erc==NO_MORE_ROWS) {
#ifndef WITHOUT_TRANSACTIONS
#ifdef COMMIT_LOCK
	    if (trans>0) {
		pthread_mutex_lock(&ware_mutex);
		char b=ST_LOCK; write(fd, &b, 1);
		int r=localdb_wareCommit();
		pthread_mutex_unlock(&ware_mutex);
		b=ST_ACTION; write(fd, &b, 1);
		if (r<0) break;
		}
#else 
	    if (trans>0 && localdb_wareCommit()<0) break;
#endif /* COMMIT_LOCK */
#endif /* WITHOUT_TRANSACTIONS */
	    return 0;
	    }
	if (erc!=MORE_ROWS) break;
	rec.ware_id=(v.ware_id_isnull)?-1:v.ware_id;
	rec.group_id=(v.group_id_isnull)?-1:v.group_id;
	rec.longtitle=(v.longtitle_isnull)?NULL:rec._longtitle;
	    strcpy(rec._longtitle, v.longtitle);
	rec.shortcut=(v.shortcut_isnull)?NULL:rec._shortcut;
	    strcpy(rec._shortcut, v.shortcut);
	rec.price=(v.price_isnull)?NULL:rec._price;
	    strcpy(rec._price, v.price);	
#ifdef WARE_PROCESS_LOCK
	pthread_mutex_lock(&ware_mutex);
#endif
#ifndef WITHOUT_TRANSACTIONS
	if (trans==0 && localdb_wareBegin()<0) break;
	trans++;
#endif
	int r=localdb_updateWare(&rec);
	if (r>=0) r=localdb_markWareBarcodes(rec.ware_id);
#ifdef WARE_PROCESS_LOCK
	pthread_mutex_unlock(&ware_mutex);
#endif
	if (r<0) break;
#ifdef WARE_PROCESS_INTERVAL
	usleep(WARE_PROCESS_INTERVAL*1000);
#endif
	}
#ifndef WITHOUT_TRANSACTIONS
    if (trans>0) localdb_wareRollback();
#endif
    return -1;
    }

/******************************** barcode update ****************************/
int serverdb_updateBarcode(int first_id, int fd) {
    enum {
	WARE_ID_COLUMN=1,
	BARCODE_COLUMN
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT ware_id, barcode "
		"FROM barcodes WHERE ware_id IN "
		    "(SELECT ware_id FROM ware "
			"WHERE counter>="), AS_UINT(first_id), AS_TEXT(")"),
	END_ARGS)<0) return -1;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    struct {
        DBINT ware_id; int ware_id_isnull;
	DBCHAR barcode[20]; int barcode_isnull;
	} v;
    dbnullbind(local_dbconn, WARE_ID_COLUMN, &v.ware_id_isnull);
	dbintbind(local_dbconn, WARE_ID_COLUMN, v.ware_id);
    dbnullbind(local_dbconn, BARCODE_COLUMN, &v.barcode_isnull);
        dbstringbind(local_dbconn, BARCODE_COLUMN, v.barcode);
    int trans=0;
    struct barcode_dbrecord rec;
    while(!*global_stop_flag) {
	bzero(&v ,sizeof(v));
	RETCODE erc=dbnextrow(local_dbconn);
	if (erc==NO_MORE_ROWS) {
#ifndef WITHOUT_TRANSACTIONS
#ifdef COMMIT_LOCK
	    if (trans>0) {
		pthread_mutex_lock(&ware_mutex);
		char b=ST_LOCK; write(fd, &b, 1);
		int r=localdb_wareCommit();
		pthread_mutex_unlock(&ware_mutex);
		b=ST_ACTION;
		write(fd, &b, 1);
		if (r<0) break;
		}
#else 
	    if (trans>0 && localdb_wareCommit()<0) break;
#endif /* COMMIT_LOCK */
#endif /* WITHOUT_TRANSACTIONS */
	    return 0;
	    }
	if (erc!=MORE_ROWS) break;
	rec.ware_id=(v.ware_id_isnull)?-1:v.ware_id;
	rec.barcode=(v.barcode_isnull)?NULL:rec._barcode;
	    strcpy(rec._barcode, v.barcode);
#ifdef WARE_PROCESS_LOCK
	pthread_mutex_lock(&ware_mutex);
#endif
#ifndef WITHOUT_TRANSACTIONS
	if (trans==0 && localdb_wareBegin()<0) break;
	trans++;
#endif
	int r=localdb_updateBarcode(&rec);
#ifdef WARE_PROCESS_LOCK
	pthread_mutex_unlock(&ware_mutex);
#endif
	if (r<0) break;
#ifdef WARE_PROCESS_INTERVAL
	usleep(WARE_PROCESS_INTERVAL*1000);
#endif
	}
#ifndef WITHOUT_TRANSACTIONS
    if (trans>0) localdb_wareRollback();
#endif
    return -1;
    }

/****************************************************************************/
int serverdb_updateWareOperID(int terminal_id, int ware_counter) {
    return sybdb_execCmd(local_dbconn,
	AS_TEXT("UPDATE terminals SET ware_counter="), AS_UINT(ware_counter),
	    AS_TEXT(" WHERE terminal_id="), AS_UINT(terminal_id),
	END_ARGS);
    }

int serverdb_getOperationSequence(struct serverSeq_dbrecord * rec) {
    enum {
	GLOBAL_COUNTER_COLUMN=1,
	UPDATE_COUNTER_COLUMN,
	CLEAR_COUNTER_COLUMN,
	FULL_CLEAR_COUNTER_COLUMN
	};
    if (sybdb_execCmdText(local_dbconn,
	"SELECT global_counter, update_counter, clear_counter, "
	    "full_clear_counter FROM operation_sequence")<0) return -1;

    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    int r=-1;
    DBINT global_counter=0;
    DBINT update_counter=0;
    DBINT clear_counter=0;
    DBINT full_clear_counter=0;
    dbintbind(local_dbconn, GLOBAL_COUNTER_COLUMN, global_counter);
    dbintbind(local_dbconn, UPDATE_COUNTER_COLUMN, update_counter);
    dbintbind(local_dbconn, CLEAR_COUNTER_COLUMN, clear_counter);
    dbintbind(local_dbconn, FULL_CLEAR_COUNTER_COLUMN, full_clear_counter);
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	rec->global_counter=global_counter;
	rec->update_counter=update_counter;
	rec->clear_counter=clear_counter;
	rec->full_clear_counter=full_clear_counter;
	r=0;
	erc=dbnextrow(local_dbconn);
	}
    
    if (erc==NO_MORE_ROWS) return r;
    dbcancel(local_dbconn);
    return -1;
    }

/***************************************************************************/
int serverdb_addSession(int terminal_id, struct session_dbrecord * rec) {
    enum {
	DATE_TIME_COLUMN=1,
	KKM_NUM_COLUMN,
	SELLER_COLUMN,
	SESSION_NUM_COLUMN,
	DOC_NUM_COLUMN,
	TOTAL_SUMM_COLUMN,
	COUNTER_COLUMN
	};
    int r=sybdb_execCmd(local_dbconn,
	AS_TEXT("EXEC UpdateSessions "), AS_UINT(terminal_id),
	    AS_TEXT(", "), AS_UINT(rec->session_id), 
	    AS_TEXT(", "), AS_STRING(rec->date_time), 
	    AS_TEXT(", "), AS_STRING(rec->n_kkm),
	    AS_TEXT(", "), AS_UINT(rec->seller), 
	    AS_TEXT(", "), AS_UINT(rec->n_session), 
	    AS_TEXT(", "), AS_UINT(rec->n_doc), 
	    AS_TEXT(", "), AS_TEXT(rec->total_summ), 
	    AS_TEXT(", "), AS_UINT(rec->counter),
	END_ARGS);
    if (r<0) return -1;
    if (r>0) return 0;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    struct {
	DBCHAR date_time[24];	int date_time_isnull;
        DBCHAR n_kkm[21];	int n_kkm_isnull;
	DBINT seller;		int seller_isnull;
        DBINT n_session;	int n_session_isnull;
	DBINT n_doc;		int n_doc_isnull;
	DBCHAR total_summ[16];	int total_summ_isnull;
        DBINT counter;
	} v;
    dbnullbind(local_dbconn, DATE_TIME_COLUMN, &v.date_time_isnull);
	dbstringbind(local_dbconn, DATE_TIME_COLUMN, v.date_time);
    dbnullbind(local_dbconn, KKM_NUM_COLUMN, &v.n_kkm_isnull);
	dbstringbind(local_dbconn, KKM_NUM_COLUMN, v.n_kkm);
    dbnullbind(local_dbconn, SELLER_COLUMN, &v.seller_isnull);
        dbintbind(local_dbconn, SELLER_COLUMN, v.seller);
    dbnullbind(local_dbconn, SESSION_NUM_COLUMN, &v.n_session_isnull);
        dbintbind(local_dbconn, SESSION_NUM_COLUMN, v.n_session);
    dbnullbind(local_dbconn, DOC_NUM_COLUMN, &v.n_doc_isnull);
        dbintbind(local_dbconn, DOC_NUM_COLUMN, v.n_doc);
    dbnullbind(local_dbconn, TOTAL_SUMM_COLUMN, &v.total_summ_isnull);
	dbstringbind(local_dbconn, TOTAL_SUMM_COLUMN, v.total_summ);
    dbintbind(local_dbconn, COUNTER_COLUMN, v.counter);
    bzero(&v, sizeof(v));
    int result=0;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	int counter=rec->counter;
        rec->date_time=(v.date_time_isnull)?NULL:rec->_date_time;
	    strcpy(rec->_date_time, v.date_time);
        rec->n_kkm=(v.n_kkm_isnull)?NULL:rec->_n_kkm;
	    strcpy(rec->_n_kkm, v.n_kkm);
        rec->seller=(v.seller_isnull)?-1:v.seller;
	rec->n_session=(v.n_session_isnull)?-1:v.n_session;
        rec->n_doc=(v.n_doc_isnull)?-1:v.n_doc;
	rec->total_summ=(v.total_summ_isnull)?NULL:rec->_total_summ;
            strcpy(rec->_total_summ, v.total_summ);
	rec->counter=v.counter;
	if (rec->counter>counter) result=localdb_updateSession(rec);
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return -1;
    }

int serverdb_addReceipt(int terminal_id, int volume_id, struct receipt_dbrecord * rec) {
    enum {
	SESSION_ID_COLUMN=1,
	DATE_TIME_COLUMN,
	KKM_NUM_COLUMN,
	SELLER_COLUMN,
	SESSION_NUM_COLUMN,
	CHECK_NUM_COLUMN,
	DOC_NUM_COLUMN,
	CHECK_TYPE_COLUMN,
	OPER_TYPE_COLUMN,
	COUNTER_COLUMN,
	VOLUME_ID_COLUMN
	};
    int r=sybdb_execCmd(local_dbconn,
	AS_TEXT("EXEC UpdateReceipts "), AS_UINT(terminal_id), 
	    AS_TEXT(", "), AS_UINT(rec->receipt_id), 
	    AS_TEXT(", "), AS_UINT(rec->session_id), 
	    AS_TEXT(", "), AS_STRING(rec->date_time), 
	    AS_TEXT(", "), AS_STRING(rec->n_kkm), 
	    AS_TEXT(", "), AS_UINT(rec->seller), 
	    AS_TEXT(", "), AS_UINT(rec->n_session), 
	    AS_TEXT(", "), AS_UINT(rec->n_check), 
	    AS_TEXT(", "), AS_UINT(rec->n_doc), 
	    AS_TEXT(", "), AS_UINT(rec->check_type), 
	    AS_TEXT(", "), AS_UINT(rec->oper_type), 
	    AS_TEXT(", "), AS_UINT(rec->counter), 
	    AS_TEXT(", "), AS_UINT(volume_id), 
	END_ARGS);
    if (r<0) return -1;
    if (r>0) return 0;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    struct {
	DBINT session_id;	int session_id_isnull;
        DBCHAR date_time[24];	int date_time_isnull;
	DBCHAR n_kkm[21];	int n_kkm_isnull;
	DBINT seller;		int seller_isnull;
	DBINT n_session;	int n_session_isnull;
	DBINT n_check;		int n_check_isnull;
	DBINT n_doc;		int n_doc_isnull;
        DBINT check_type;	int check_type_isnull;
        DBINT oper_type;	int oper_type_isnull;
	DBINT counter;
	DBINT volume_id;
	} v;
puts ("*** 1 ***");
    dbnullbind(local_dbconn, SESSION_ID_COLUMN, &v.session_id_isnull);
	dbintbind(local_dbconn, SESSION_ID_COLUMN, v.session_id);
    dbnullbind(local_dbconn, DATE_TIME_COLUMN, &v.date_time_isnull);
	dbstringbind(local_dbconn, DATE_TIME_COLUMN, v.date_time);
    dbnullbind(local_dbconn, KKM_NUM_COLUMN, &v.n_kkm_isnull);
	dbstringbind(local_dbconn, KKM_NUM_COLUMN, v.n_kkm);
    dbnullbind(local_dbconn, SELLER_COLUMN, &v.seller_isnull);
	dbintbind(local_dbconn, SELLER_COLUMN, v.seller);
    dbnullbind(local_dbconn, SESSION_NUM_COLUMN, &v.n_session_isnull);
	dbintbind(local_dbconn, SESSION_NUM_COLUMN, v.n_session);
    dbnullbind(local_dbconn, CHECK_NUM_COLUMN, &v.n_check_isnull);
	dbintbind(local_dbconn, CHECK_NUM_COLUMN, v.n_check);
    dbnullbind(local_dbconn, DOC_NUM_COLUMN, &v.n_doc_isnull);
	dbintbind(local_dbconn, DOC_NUM_COLUMN, v.n_doc);
    dbnullbind(local_dbconn, CHECK_TYPE_COLUMN, &v.check_type_isnull);
	dbintbind(local_dbconn, CHECK_TYPE_COLUMN, v.check_type);
    dbnullbind(local_dbconn, OPER_TYPE_COLUMN, &v.oper_type_isnull);
	dbintbind(local_dbconn, OPER_TYPE_COLUMN, v.oper_type);
    dbintbind(local_dbconn, COUNTER_COLUMN, v.counter);
    dbintbind(local_dbconn, VOLUME_ID_COLUMN, v.volume_id);
puts ("*** 2 ***");
    int result=0;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	int counter=rec->counter;
        rec->session_id=(v.session_id_isnull)?-1:v.session_id;
	rec->date_time=(v.date_time_isnull)?NULL:rec->_date_time;
	    strcpy(rec->_date_time, v.date_time);
	rec->n_kkm=(v.n_kkm_isnull)?NULL:rec->_n_kkm;
	    strcpy(rec->_n_kkm, v.n_kkm);
	rec->seller=(v.seller_isnull)?-1:v.seller;
        rec->n_session=(v.n_session_isnull)?-1:v.n_session;
	rec->n_check=(v.n_check_isnull)?-1:v.n_check;
        rec->n_doc=(v.n_doc_isnull)?-1:v.n_doc;
	rec->check_type=(v.check_type_isnull)?-1:v.check_type;
        rec->oper_type=(v.oper_type_isnull)?-1:v.oper_type;
	rec->counter=v.counter;
	if (v.volume_id!=volume_id) result=localdb_deleteReceipt(rec->receipt_id);
	else if (rec->counter>counter) result=localdb_updateReceipt(rec);
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return -1;
    }

int serverdb_addRegistration(int terminal_id, int volume_id, struct registration_dbrecord * rec) {
    enum {
	RECEIPT_ID_COLUMN=1,
	KKM_NUM_COLUMN,
	SESSION_NUM_COLUMN,
	CHECK_NUM_COLUMN,
	DOC_NUM_COLUMN,
	CHECK_TYPE_COLUMN,
	POSITION_NUM_COLUMN,
	WARE_ID_COLUMN,
	BARCODE_COLUMN,
	PRICE_COLUMN,
	COUNTER_COLUMN,
	VOLUME_ID_COLUMN
	};
    int r=sybdb_execCmd(local_dbconn,
	AS_TEXT("EXEC UpdateRegistrations "), AS_UINT(terminal_id), 
	    AS_TEXT(", "), AS_UINT(rec->registration_id), 
	    AS_TEXT(", "), AS_UINT(rec->receipt_id), 
	    AS_TEXT(", "), AS_STRING(rec->n_kkm), 
	    AS_TEXT(", "), AS_UINT(rec->n_session), 
	    AS_TEXT(", "), AS_UINT(rec->n_check), 
	    AS_TEXT(", "), AS_UINT(rec->n_doc), 
	    AS_TEXT(", "), AS_UINT(rec->check_type), 
	    AS_TEXT(", "), AS_UINT(rec->n_position), 
	    AS_TEXT(", "), AS_UINT(rec->ware_id), 
	    AS_TEXT(", "), AS_STRING(rec->barcode), 
	    AS_TEXT(", "), AS_TEXT(rec->price), 
	    AS_TEXT(", "), AS_UINT(rec->counter), 
	    AS_TEXT(", "), AS_UINT(volume_id), 
	END_ARGS);
    if (r<0) return -1;
    if (r>0) return 0;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    struct {
        DBINT receipt_id;	int receipt_id_isnull;
	DBCHAR n_kkm[21];	int n_kkm_isnull;
	DBINT n_session;	int n_session_isnull;
	DBINT n_check;		int n_check_isnull;
	DBINT n_doc;		int n_doc_isnull;
	DBINT check_type;	int check_type_isnull;
	DBINT n_position;       int n_position_isnull;
	DBINT ware_id;          int ware_id_isnull;
        DBCHAR barcode[24];	int barcode_isnull;
        DBCHAR price[16];	int price_isnull;
	DBINT counter;
	DBINT volume_id;
	} v;
    dbnullbind(local_dbconn, RECEIPT_ID_COLUMN, &v.receipt_id_isnull);
	dbintbind(local_dbconn, RECEIPT_ID_COLUMN, v.receipt_id);
    dbnullbind(local_dbconn, KKM_NUM_COLUMN, &v.n_kkm_isnull);
	dbstringbind(local_dbconn, KKM_NUM_COLUMN, v.n_kkm);
    dbnullbind(local_dbconn, SESSION_NUM_COLUMN, &v.n_session_isnull);
	dbintbind(local_dbconn, SESSION_NUM_COLUMN, v.n_session);
    dbnullbind(local_dbconn, CHECK_NUM_COLUMN, &v.n_check_isnull);
	dbintbind(local_dbconn, CHECK_NUM_COLUMN, v.n_check);
    dbnullbind(local_dbconn, DOC_NUM_COLUMN, &v.n_doc_isnull);
	dbintbind(local_dbconn, DOC_NUM_COLUMN, v.n_doc);
    dbnullbind(local_dbconn, CHECK_TYPE_COLUMN, &v.check_type_isnull);
	dbintbind(local_dbconn, CHECK_TYPE_COLUMN, v.check_type);
    dbnullbind(local_dbconn, POSITION_NUM_COLUMN, &v.n_position_isnull);
	dbintbind(local_dbconn, POSITION_NUM_COLUMN, v.n_position);
    dbnullbind(local_dbconn, WARE_ID_COLUMN, &v.ware_id_isnull);
	dbintbind(local_dbconn, WARE_ID_COLUMN, v.ware_id);
    dbnullbind(local_dbconn, BARCODE_COLUMN, &v.barcode_isnull);
	dbstringbind(local_dbconn, BARCODE_COLUMN, v.barcode);
    dbnullbind(local_dbconn, PRICE_COLUMN, &v.price_isnull);
	dbstringbind(local_dbconn, PRICE_COLUMN, v.price);
    dbintbind(local_dbconn, COUNTER_COLUMN, v.counter);
    dbintbind(local_dbconn, VOLUME_ID_COLUMN, v.volume_id);
    int counter=rec->counter;
    int result=0;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
        rec->receipt_id=(v.receipt_id_isnull)?-1:v.receipt_id;
	rec->n_kkm=(v.n_kkm_isnull)?NULL:rec->_n_kkm;
	    strcpy(rec->_n_kkm, v.n_kkm);
        rec->n_session=(v.n_session_isnull)?-1:v.n_session;
	rec->n_check=(v.n_check_isnull)?-1:v.n_check;
        rec->n_doc=(v.n_doc_isnull)?-1:v.n_doc;
	rec->check_type=(v.check_type_isnull)?-1:v.check_type;
        rec->n_position=(v.n_position_isnull)?-1:v.n_position;
	rec->ware_id=(v.ware_id)?-1:v.ware_id;
        rec->barcode=(v.barcode_isnull)?NULL:rec->_barcode;
	    strcpy(rec->_barcode, v.barcode);
        rec->price=(v.price_isnull)?NULL:rec->_price;
	    strcpy(rec->_price, v.price);
        rec->counter=v.counter;
        if (v.volume_id!=volume_id) result=localdb_deleteRegistration(rec->registration_id);
        else if (rec->counter>counter) result=localdb_updateRegistration(rec);
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return -1;
    }

int serverdb_addQuantity(int terminal_id, int volume_id, struct quantity_dbrecord * rec) {
    enum {
	REGISTRATION_ID_COLUMN=1,
	SELLER_COLUMN,
	CHECK_TYPE_COLUMN,
	OPER_TYPE_COLUMN,
	QUANTITY_COLUMN,
	SUMM_COLUMN,
	COUNTER_COLUMN,
	VOLUME_ID_COLUMN
	};
    int r=sybdb_execCmd(local_dbconn,
	AS_TEXT("EXEC UpdateQuantity "), AS_UINT(terminal_id), 
	    AS_TEXT(", "), AS_UINT(rec->quantity_id),
	    AS_TEXT(", "), AS_UINT(rec->registration_id), 
	    AS_TEXT(", "), AS_UINT(rec->seller), 
	    AS_TEXT(", "), AS_UINT(rec->check_type), 
	    AS_TEXT(", "), AS_UINT(rec->oper_type), 
	    AS_TEXT(", "), AS_TEXT(rec->quantity), 
	    AS_TEXT(", "), AS_TEXT(rec->summ), 
	    AS_TEXT(", "), AS_UINT(rec->counter), 
	    AS_TEXT(", "), AS_UINT(volume_id), 
	END_ARGS);
    if (r<0) return -1;
    if (r>0) return 0;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    struct {
        DBINT registration_id;	int registration_id_isnull;
        DBINT seller;		int seller_isnull;
        DBINT check_type;	int check_type_isnull;
        DBINT oper_type;	int oper_type_isnull;
	DBCHAR quantity[16];	int quantity_isnull;
	DBCHAR summ[16];	int summ_isnull;
	DBINT counter;
	DBINT volume_id;
	} v;
    dbnullbind(local_dbconn, REGISTRATION_ID_COLUMN, &v.registration_id_isnull);
	dbintbind(local_dbconn, REGISTRATION_ID_COLUMN, v.registration_id);
    dbnullbind(local_dbconn, SELLER_COLUMN, &v.seller_isnull);
	dbintbind(local_dbconn, SELLER_COLUMN, v.seller);
    dbnullbind(local_dbconn, CHECK_TYPE_COLUMN, &v.check_type_isnull);
	dbintbind(local_dbconn, CHECK_TYPE_COLUMN, v.check_type);
    dbnullbind(local_dbconn, OPER_TYPE_COLUMN, &v.oper_type_isnull);
	dbintbind(local_dbconn, OPER_TYPE_COLUMN, v.oper_type);
    dbnullbind(local_dbconn, QUANTITY_COLUMN, &v.quantity_isnull);
        dbstringbind(local_dbconn, QUANTITY_COLUMN, v.quantity);
    dbnullbind(local_dbconn, SUMM_COLUMN, &v.summ_isnull);
	dbstringbind(local_dbconn, SUMM_COLUMN, v.summ);
    dbintbind(local_dbconn, COUNTER_COLUMN, v.counter);
    dbintbind(local_dbconn, VOLUME_ID_COLUMN, v.volume_id);
    int counter=rec->counter;
    int result=0;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	rec->registration_id=(v.registration_id_isnull)?-1:v.registration_id;
        rec->seller=(v.seller_isnull)?-1:v.seller;
	rec->check_type=(v.check_type_isnull)?-1:v.check_type;
        rec->oper_type=(v.oper_type_isnull)?-1:v.oper_type;
	rec->quantity=(v.quantity_isnull)?NULL:rec->_quantity;
    	    strcpy(rec->_quantity, v.quantity);
        rec->summ=(v.summ_isnull)?NULL:rec->_summ;
	    strcpy(rec->_summ, v.summ);
	rec->counter=v.counter;
        if (v.volume_id!=volume_id) result=localdb_deleteQuantity(rec->quantity_id);
        else if (rec->counter>counter) result=localdb_updateQuantity(rec);
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return -1;
    }

int serverdb_addPayment(int terminal_id, int volume_id, struct payment_dbrecord * rec) {
    enum {
	RECEIPT_ID_COLUMN=1,
	SELLER_COLUMN,
	CHECK_TYPE_COLUMN,
	PAYMENT_TYPE_COLUMN,
	SUMM_COLUMN,
	COUNTER_COLUMN,
	VOLUME_ID_COLUMN
	};
    int r=sybdb_execCmd(local_dbconn,
	AS_TEXT("EXEC UpdatePayments "), AS_UINT(terminal_id), 
	    AS_TEXT(", "), AS_UINT(rec->payment_id), 
	    AS_TEXT(", "), AS_UINT(rec->receipt_id), 
	    AS_TEXT(", "), AS_UINT(rec->seller), 
	    AS_TEXT(", "), AS_UINT(rec->check_type), 
	    AS_TEXT(", "), AS_UINT(rec->payment_type), 
	    AS_TEXT(", "), AS_TEXT(rec->summ), 
	    AS_TEXT(", "), AS_UINT(rec->counter), 
	    AS_TEXT(", "), AS_UINT(volume_id), 
	END_ARGS);
    if (r<0) return -1;
    if (r>0) return 0;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    struct {
	DBINT receipt_id;	int receipt_id_isnull;	
	DBINT seller;		int seller_isnull;
        DBINT check_type;	int check_type_isnull;
        DBINT payment_type;	int payment_type_isnull;
	DBCHAR summ[16];	int summ_isnull;
	DBINT counter;
	DBINT volume_id;
	} v;
    dbnullbind(local_dbconn, RECEIPT_ID_COLUMN, &v.receipt_id_isnull);
	dbintbind(local_dbconn, RECEIPT_ID_COLUMN, v.receipt_id);
    dbnullbind(local_dbconn, SELLER_COLUMN, &v.seller_isnull);
	dbintbind(local_dbconn, SELLER_COLUMN, v.seller);
    dbnullbind(local_dbconn, CHECK_TYPE_COLUMN, &v.check_type_isnull);
	dbintbind(local_dbconn, CHECK_TYPE_COLUMN, v.check_type);
    dbnullbind(local_dbconn, PAYMENT_TYPE_COLUMN, &v.payment_type_isnull);
	dbintbind(local_dbconn, PAYMENT_TYPE_COLUMN, v.payment_type);
    dbnullbind(local_dbconn, SUMM_COLUMN, &v.summ_isnull);
	dbstringbind(local_dbconn, SUMM_COLUMN, v.summ);
    dbintbind(local_dbconn, COUNTER_COLUMN, v.counter);
    dbintbind(local_dbconn, VOLUME_ID_COLUMN, v.volume_id);
    int counter=rec->counter;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	rec->receipt_id=(v.receipt_id_isnull)?-1:v.receipt_id;
        rec->seller=(v.seller_isnull)?-1:v.seller;
	rec->check_type=(v.check_type_isnull)?-1:v.check_type;
        rec->payment_type=(v.payment_type_isnull)?-1:v.payment_type;
	rec->summ=(v.summ_isnull)?NULL:rec->_summ;
	    strcpy(rec->_summ, v.summ);
	rec->counter=v.counter;
	if (v.volume_id!=volume_id) r=localdb_deletePayment(rec->payment_id);
	else if (rec->counter>counter) r=localdb_updatePayment(rec);
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return r;
    dbcancel(local_dbconn);
    return -1;
    }

int serverdb_updateSessionOperID(int terminal_id, int counter) {
    return sybdb_execCmd(local_dbconn,
	AS_TEXT("UPDATE terminals SET session_counter="), AS_UINT(counter),
	    AS_TEXT(" WHERE terminal_id="), AS_UINT(terminal_id),
	END_ARGS);
    }

int serverdb_getTerminalID(char * name) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT MAX(terminal_id) FROM terminals "
		"WHERE name="), AS_STRING(name),    
	END_ARGS)<0) return -1;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=-1;
    DBINT v=0;
    dbintbind(local_dbconn, 1, v);
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	result=v;
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return -1;
    }

int serverdb_getTerminalValues(struct terminals_dbrecord * rec) {
    enum {
	NAME_COLUMN=1,
	WARE_COUNTER_COLUMN,
	SESSION_COUNTER_COLUMN
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT name, ware_counter, session_counter FROM terminals "
		    "WHERE terminal_id="), AS_UINT(rec->terminal_id),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return -1;
    struct {
	DBCHAR name[41];	int name_isnull;
	DBINT ware_counter;	int ware_counter_isnull;
	DBINT session_counter;	int session_counter_isnull;
	} v;
    dbnullbind(local_dbconn, NAME_COLUMN, &v.name_isnull);
	dbstringbind(local_dbconn, NAME_COLUMN, v.name);
    dbnullbind(local_dbconn, WARE_COUNTER_COLUMN, &v.ware_counter_isnull);
	dbintbind(local_dbconn, WARE_COUNTER_COLUMN, v.ware_counter);
    dbnullbind(local_dbconn, SESSION_COUNTER_COLUMN, &v.session_counter_isnull);
	dbintbind(local_dbconn, SESSION_COUNTER_COLUMN, v.session_counter);
    int result=-1;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	rec->name=(v.name_isnull)?NULL:rec->_name;
	    strcpy(rec->_name, v.name);
        rec->ware_counter=(v.ware_counter_isnull)?-1:v.ware_counter;
        rec->session_counter=(v.session_counter_isnull)?-1:v.session_counter;
	result=0;
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    return -1;
    }
