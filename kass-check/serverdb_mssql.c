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

extern int global_action;

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
	if (dberr != SYBESMSG) {
	    log_puts(0, "DB-LIBRARY error:");
	    log_printf(1, "\t%s", dberrstr);
	}
//	if (oserr != DBNOERR) {
//	    log_puts(0, "Operating-system error:");
//	    log_printf(1, "\t%s", oserrstr);
//	}
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
    RETCODE erc=dbsqlexec(conn);
    if(erc!=SUCCEED) return -1;
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
#ifdef DEBUG
	    fprintf(stderr,"*** sybdb_execCmd() ASSERT: invalid value type: %d\n",t);
#endif
	    va_end(ap);
	    log_message(INTERNAL_ERROR);
	    return -1;
	    }
	if (r) {
#ifdef DEBUG
	    fputs("*** sybdb_execCmd() ASSERT: text buffer overflow\n",stderr);
#endif
	    va_end(ap);
	    log_message(INTERNAL_ERROR);
	    return -1;
	    }
	t=va_arg(ap,int);
	}
    va_end(ap);
    return sybdb_execCmdText(conn, text);
    }

/*****************************************************************************/
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

int serverdb_getWareCounter(int terminal_id) {
    int r=sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT ware_counter FROM terminals "
		    "WHERE terminal_id="), AS_UINT(terminal_id),
	END_ARGS);
    if (r<0) return -1;
    RETCODE erc=dbresults(local_dbconn);
    if (erc!=SUCCEED) return -1;
    int result=0;
    DBINT v=0;
    dbintbind(local_dbconn, 1, v);
    if (dbnextrow(local_dbconn)==NO_MORE_ROWS) return 0;
    result=v;
    if (dbnextrow(local_dbconn)==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return -1;
    }

static void log_info(char * table, int ins, int upd) {
    if (ins || upd) 
	log_printf(0, "  * таблица `%s' локальной БД обновлена:", table);
    if (ins)
	log_printf(1, "    ** добавлено записей: %d", ins);
    if (upd)
	log_printf(1, "    ** обновлено записей: %d", upd);
    }

/****************************** groups *************************************/
int serverdb_groupsCount(int last_id) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT COUNT(*) FROM groups "
	    "WHERE group_id IS NOT NULL AND group_id>0 "
	    "AND counter<="), AS_UINT(last_id),
	END_ARGS)<0) return -1;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=0;
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

int serverdb_processGroups(int last_id) {
    enum {
	GROUP_ID_COLUMN=1,
	PARENT_ID_COLUMN,
	NAME_COLUMN
	};
    int err=SERVER_FAIL;
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT group_id, parent_id, name "
	        "FROM groups WHERE counter<="), AS_UINT(last_id),
	    AS_TEXT(" ORDER BY parent_id"),
	END_ARGS)<0) return err;
    if (dbresults(local_dbconn)!=SUCCEED) return err;
    struct {
        DBINT group_id;		int group_id_isnull;
        DBINT parent_id;	int parent_id_isnull;
	char name[255];		int name_isnull;
	} v;
    dbnullbind(local_dbconn, GROUP_ID_COLUMN, &v.group_id_isnull);
	dbintbind(local_dbconn, GROUP_ID_COLUMN, v.group_id);
    dbnullbind(local_dbconn, PARENT_ID_COLUMN, &v.parent_id_isnull);
	dbintbind(local_dbconn, PARENT_ID_COLUMN, v.parent_id);
    dbnullbind(local_dbconn, NAME_COLUMN, &v.name_isnull);
	dbstringbind(local_dbconn, NAME_COLUMN, v.name);
    int ins=0;
    struct group_dbrecord rec;
    while(err=BREAK_PROCESS, global_action) {
	err=WAREDB_FAIL;
	bzero(&v, sizeof(v));
	RETCODE erc=dbnextrow(local_dbconn);
	if (erc==NO_MORE_ROWS) {
	    log_info("groups", ins, 0);
	    return 0;
	    }
	if (erc!=MORE_ROWS) break;
	rec.group_id=(v.group_id_isnull)?-1:v.group_id;
	rec.parent_id=(v.parent_id_isnull)?-1:v.parent_id;
	rec.name=(v.name_isnull)?NULL:rec._name;
	    strcpy(rec._name, v.name);
	int n=localdb_findGroup(rec.group_id);
	if (n<0) break;
	if (n==0) {
	    int r=localdb_insertGroup(&rec);
	    if (r<0) break;
	    if (r>0) ins+=r;
	    }
	}
    dbcancel(local_dbconn);
    return err;
    }

/******************************** ware **************************************/
int serverdb_wareCount(int last_id) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT COUNT(*) FROM ware "
	    "WHERE counter<="), AS_UINT(last_id),
	    AS_TEXT(" AND ware_id>0 AND price IS NOT NULL"),
	END_ARGS)<0) return -1;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=0;
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

int serverdb_processWare(int last_id) {
    enum {
	WARE_ID_COLUMN=1,
	PARENT_ID_COLUMN,
	LONGTITLE_COLUMN,
	SHORTCUT_COLUMN,
	PRICE_COLUMN
	};
    int err=SERVER_FAIL;
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT ware_id, group_id, longtitle, shortcut, price "
		"FROM ware WHERE counter<="), AS_UINT(last_id),
	END_ARGS)<0) return err;
    if (dbresults(local_dbconn)!=SUCCEED) return err;
    struct {
	DBINT ware_id;		int ware_id_isnull;
        DBINT parent_id;	int parent_id_isnull;
	DBCHAR longtitle[255];	int longtitle_isnull;
	DBCHAR shortcut[255];	int shortcut_isnull;
	DBCHAR price[16];	int price_isnull;
	} v;
    dbnullbind(local_dbconn, WARE_ID_COLUMN, &v.ware_id_isnull);
        dbintbind(local_dbconn, WARE_ID_COLUMN, v.ware_id);
    dbnullbind(local_dbconn, PARENT_ID_COLUMN, &v.parent_id_isnull);
	dbintbind(local_dbconn, PARENT_ID_COLUMN, v.parent_id);
    dbnullbind(local_dbconn, LONGTITLE_COLUMN, &v.longtitle_isnull);
	dbstringbind(local_dbconn, LONGTITLE_COLUMN, v.longtitle);
    dbnullbind(local_dbconn, SHORTCUT_COLUMN, &v.shortcut_isnull);
        dbstringbind(local_dbconn, SHORTCUT_COLUMN, v.shortcut);
    dbnullbind(local_dbconn, PRICE_COLUMN, &v.price_isnull);
	dbstringbind(local_dbconn, PRICE_COLUMN, v.price);
    int ins=0;
    int trans=0;
    struct ware_dbrecord rec;
    while(err=BREAK_PROCESS, global_action) {
	err=WAREDB_FAIL;
	bzero(&v, sizeof(v));
	RETCODE erc=dbnextrow(local_dbconn);
	if (erc==NO_MORE_ROWS) {
	    if (trans>0 && localdb_wareCommit()<0) break;
	    log_info("ware", ins, 0);
	    return 0;
	    }
	if (erc!=MORE_ROWS) break;
	rec.ware_id=(v.ware_id_isnull)?-1:v.ware_id;
	rec.group_id=(v.parent_id_isnull)?-1:v.parent_id;
	rec.longtitle=(v.longtitle_isnull)?NULL:rec._longtitle;
	    strcpy(rec._longtitle, v.longtitle);
	rec.shortcut=(v.shortcut_isnull)?NULL:rec._shortcut;
	    strcpy(rec._shortcut, v.shortcut);
	rec.price=(v.price_isnull)?NULL:rec._price;
	    strcpy(rec._price, v.price);	
	int n=localdb_findWare(rec.ware_id);
	if (n<0) break;
	if (n==0) {
	    if (trans==0 && localdb_wareBegin()<0) break;
	    trans++;
	    int r=localdb_insertWare(&rec);
	    if (r<0) break;
	    if (r>0) ins+=r;
	    }
	}
    dbcancel(local_dbconn);
    if (trans>0) localdb_wareRollback();
    return err;
    }

/******************************** barcode ***********************************/
int serverdb_barcodesCount(int last_id) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT COUNT(*) FROM ware JOIN barcodes "
	    "ON ware.ware_id=barcodes.ware_id "
	    "WHERE ware.counter<="), AS_UINT(last_id),
	    AS_TEXT(" AND ware.ware_id>0 AND ware.price IS NOT NULL"),
	END_ARGS)<0) return -1;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=0;
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

int serverdb_processBarcodes(int last_id) {
    enum {
	WARE_ID_COLUMN=1,
	BARCODE_COLUMN
	};
    int err=SERVER_FAIL;
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT ware.ware_id, barcodes.barcode "
		"FROM barcodes JOIN ware"
		    " ON barcodes.ware_id=ware.ware_id "
		"WHERE ware.counter<="), AS_UINT(last_id),
	END_ARGS)<0) return err;
    if (dbresults(local_dbconn)!=SUCCEED) return err;
    struct {
        DBINT ware_id; int ware_id_isnull;
	DBCHAR barcode[20]; int barcode_isnull;
	} v;
    dbnullbind(local_dbconn, WARE_ID_COLUMN, &v.ware_id_isnull);
	dbintbind(local_dbconn, WARE_ID_COLUMN, v.ware_id);
    dbnullbind(local_dbconn, BARCODE_COLUMN, &v.barcode_isnull);
        dbstringbind(local_dbconn, BARCODE_COLUMN, v.barcode);
    int ins=0;
    int trans=0;
    struct barcode_dbrecord rec;
    while(err=BREAK_PROCESS, global_action) {
	err=WAREDB_FAIL;
	bzero(&v, sizeof(v));
	RETCODE erc=dbnextrow(local_dbconn);
	if (erc==NO_MORE_ROWS) {
	    if (trans>0 && localdb_wareCommit()<0) break;
	    log_info("barcodes", ins, 0);
	    return 0;
	    }
	if (erc!=MORE_ROWS) break;
	rec.ware_id=(v.ware_id_isnull)?-1:v.ware_id;
	rec.barcode=(v.barcode_isnull)?NULL:rec._barcode;
	    strcpy(rec._barcode, v.barcode);
	int n=localdb_findBarcode(rec.ware_id, rec.barcode);
	if (n<0) break;
	if (n==0) {
	    if (trans==0 && localdb_wareBegin()<0) break;
	    trans++;
	    int r=localdb_insertBarcode(&rec);
	    if (r<0) break;
	    if (r>0) ins+=r;
	    }
	}
    dbcancel(local_dbconn);
    if (trans>0) localdb_wareRollback();
    return err;
    }

/*****************************************************************************/
static int serverdb_getMainSeq(char * table, char * column, int terminal_id, int counter) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT MAX("), AS_TEXT(column), AS_TEXT(") FROM "), AS_TEXT(table),
	    AS_TEXT(" WHERE terminal_id="), AS_UINT(terminal_id),
	    AS_TEXT(" AND counter<="), AS_UINT(counter),
	END_ARGS)<0) return -1;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=0;
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

int serverdb_getVolumesSeq(int terminal_id, int counter) {
    return serverdb_getMainSeq("receipts", "volume_id", terminal_id, counter);
    }

int serverdb_getSessionsSeq(int terminal_id, int counter) {
    return serverdb_getMainSeq("sessions", "session_id", terminal_id, counter);
    }
    
int serverdb_getReceiptsSeq(int terminal_id, int counter) {
    return serverdb_getMainSeq("receipts", "receipt_id", terminal_id, counter);
    }

int serverdb_getRegistrationsSeq(int terminal_id, int counter) {
    return serverdb_getMainSeq("registrations", "registration_id", terminal_id, counter);
    }

int serverdb_getQuantitySeq(int terminal_id, int counter) {
    return serverdb_getMainSeq("ware_quantity", "quantity_id", terminal_id, counter);
    }

int serverdb_getPaymentsSeq(int terminal_id, int counter) {
    return serverdb_getMainSeq("payments", "payment_id", terminal_id, counter);
    }

/****************************************************************************/
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
    if (r<0) return SERVER_FAIL;
    if (r>0) return SERVERDB_UPDATE;
    if (dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
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
    int result=NO_ACTION;
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
	if (rec->counter>counter) {
	    int n=localdb_updateSession(rec);
	    if (n<0) result=MAINDB_FAIL;
	    else if (n>0) result=LOCALDB_UPDATE;
	    }
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

int serverdb_processSessions(int terminal_id, int max_counter) {
    enum {
	SESSION_ID_COLUMN=1,
	DATE_TIME_COLUMN,
	KKM_NUM_COLUMN,
	SELLER_COLUMN,
	SESSION_NUM_COLUMN,
	DOC_NUM_COLUMN,
	TOTAL_SUMM_COLUMN,
	COUNTER_COLUMN
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT session_id, CONVERT(varchar, date_time, 20),"
	    " n_kkm, seller, n_session, n_doc, total_summ, counter"
	    " FROM sessions WHERE terminal_id="), AS_UINT(terminal_id),
		AS_TEXT(" AND counter<="), AS_UINT(max_counter),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    struct {
        DBINT session_id;
	DBCHAR date_time[24];	int date_time_isnull;
        DBCHAR n_kkm[24];	int n_kkm_isnull;
	DBINT seller;		int seller_isnull;
	DBINT n_session;	int n_session_isnull;
	DBINT n_doc;		int n_doc_isnull;
	DBCHAR total_summ[16];	int total_summ_isnull;
        DBINT counter;
	} v;
    dbintbind(local_dbconn, SESSION_ID_COLUMN, v.session_id);
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
    int upd=0, ins=0;
    struct session_dbrecord rec;
    bzero(&v, sizeof(v));
    RETCODE erc=dbnextrow(local_dbconn);
    while(erc==MORE_ROWS) {
	if (!global_action) {
	    dbcancel(local_dbconn); return BREAK_PROCESS;
	    }
	if (erc!=MORE_ROWS) break;
	rec.session_id=v.session_id;
        rec.date_time=(v.date_time_isnull)?NULL:rec._date_time;
	    strcpy(rec._date_time, v.date_time);
	rec.n_kkm=(v.n_kkm_isnull)?NULL:rec._n_kkm;
	    strcpy(rec._n_kkm, v.n_kkm);
	rec.seller=(v.seller_isnull)?-1:v.seller;
	rec.n_session=(v.n_session_isnull)?-1:v.n_session;
	rec.n_doc=(v.n_doc_isnull)?-1:v.n_doc;
	rec.total_summ=(v.total_summ_isnull)?NULL:rec._total_summ;
	    strcpy(rec._total_summ, v.total_summ);
        rec.counter=v.counter;
	int r=localdb_findSession(rec.session_id);
//printf("*** localdb_findSession(): r=%d\n", r);
	if (r==0) {
	    r=localdb_insertSession(&rec);
	    if (r>0) ins+=r;
	    }
	else if (r>0) {
	    r=localdb_updateSession(&rec);
	    if (r>0) upd+=r;
	    }
	if (r<0) {
	    dbcancel(local_dbconn); 
	    return MAINDB_FAIL;
	    }
	bzero(&v, sizeof(v));
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) {
        log_info("sessions", ins, upd);
        return 0;
        }
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

/****************************** receipts ***********************************/
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
	VOLUME_ID_COLUMN,
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
    if (r<0) return SERVER_FAIL;
    if (r>0) return SERVERDB_UPDATE;
    if (dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
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
    dbnullbind(local_dbconn, OPER_TYPE_COLUMN, &v.oper_type_isnull);
	dbintbind(local_dbconn, OPER_TYPE_COLUMN, v.oper_type);
    dbnullbind(local_dbconn, CHECK_TYPE_COLUMN, &v.check_type_isnull);
	dbintbind(local_dbconn, CHECK_TYPE_COLUMN, v.check_type);
    dbintbind(local_dbconn, COUNTER_COLUMN, v.counter);
    dbintbind(local_dbconn, VOLUME_ID_COLUMN, v.volume_id);
    int result=NO_ACTION;
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
	if (v.volume_id!=volume_id) {
	    int r=localdb_deleteReceipt(rec->receipt_id);
	    if (r<0) result=VOLUME_FAIL;
	    else if (r>0) result=LOCALDB_DELETE;
	    }
	else if (rec->counter>counter) {
	    int r=localdb_updateReceipt(rec);
	    if (r<0) result=VOLUME_FAIL;
	    else if (r>0) result=LOCALDB_UPDATE;
	    }
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

int serverdb_processReceipts(int terminal_id, int volume_id, int max_counter) {
    enum {
	RECEIPT_ID_COLUMN=1,
	SESSION_ID_COLUMN,
	DATE_TIME_COLUMN,
	KKM_NUM_COLUMN,
	SELLER_COLUMN,
	SESSION_NUM_COLUMN,
	CHECK_NUM_COLUMN,
	DOC_NUM_COLUMN,
	CHECK_TYPE_COLUMN,
	OPER_TYPE_COLUMN,
	COUNTER_COLUMN,
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT receipt_id, session_id, "
	    "CONVERT(varchar, date_time, 20),"
	    " n_kkm, seller, n_session, n_check, n_doc,"
	     " check_type, oper_type, counter"
	    " FROM receipts WHERE terminal_id="), AS_UINT(terminal_id),
		AS_TEXT(" AND volume_id="), AS_UINT(volume_id),
		AS_TEXT(" AND counter<="), AS_UINT(max_counter),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    struct {
        DBINT receipt_id;
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
	} v;
    dbintbind(local_dbconn, RECEIPT_ID_COLUMN, v.receipt_id);
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
    int ins=0, upd=0;
    struct receipt_dbrecord rec;
    bzero(&v, sizeof(v));
    RETCODE erc=dbnextrow(local_dbconn);
    while(erc==MORE_ROWS) {
	if (!global_action) {
	    dbcancel(local_dbconn); return BREAK_PROCESS;
	    }
	rec.receipt_id=v.receipt_id;
        rec.session_id=(v.session_id_isnull)?-1:v.session_id;
	rec.date_time=(v.date_time_isnull)?NULL:rec._date_time;
	    strcpy(rec._date_time, v.date_time);
	rec.n_kkm=(v.n_kkm_isnull)?NULL:rec._n_kkm;
	    strcpy(rec._n_kkm, v.n_kkm);
	rec.seller=(v.seller_isnull)?-1:v.seller;
	rec.n_session=(v.n_session_isnull)?-1:v.n_session;
	rec.n_check=(v.n_check_isnull)?-1:v.n_check;
	rec.n_doc=(v.n_doc_isnull)?-1:v.n_doc;
        rec.check_type=(v.check_type_isnull)?-1:v.check_type;
	rec.oper_type=(v.oper_type_isnull)?-1:v.oper_type;
        rec.counter=v.counter;
	int r=localdb_findReceipt(rec.receipt_id);
	if (r==0) {
	    r=localdb_insertReceipt(&rec);
	    if (r>0) ins+=r;
	    }
	else if (r>0) {
	    int r=localdb_updateReceipt(&rec);
	    if (r>0) upd+=r;
	    }
	if (r<0) {
	    dbcancel(local_dbconn);
	    return VOLUME_FAIL;
	    }    
	bzero(&v, sizeof(v));
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) {
        log_info("receipts", ins, upd);
        return 0;
        }
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

/********************************* registrations *****************************/
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
    if (r<0) return SERVER_FAIL;
    if (r>0) return SERVERDB_UPDATE;
    if (dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
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
    int result=NO_ACTION;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	result=NO_ACTION;
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
	if (v.volume_id!=volume_id) {
	    int r=localdb_deleteRegistration(rec->registration_id);
	    if (r<0) result=VOLUME_FAIL;
	    else if (r>0) result=LOCALDB_DELETE;
	    }
	else if (rec->counter>counter) {
	    int r=localdb_updateRegistration(rec);
	    if (r<0) result=VOLUME_FAIL;
	    else if (r>0) result=LOCALDB_UPDATE;
	    }
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

int serverdb_processRegistrations(int terminal_id, int volume_id, int max_counter) {
    enum {
	REGISTRATION_ID_COLUMN=1,
	RECEIPT_ID_COLUMN,
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
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT registration_id, receipt_id, n_kkm, n_session,"
	    " n_check, n_doc, check_type, n_position, ware_id, barcode,"
	    " price, counter"
	    " FROM registrations WHERE terminal_id="), AS_UINT(terminal_id),
		AS_TEXT(" AND volume_id="), AS_UINT(volume_id),
		AS_TEXT(" AND counter<="), AS_UINT(max_counter),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    struct {
	DBINT registration_id;
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
	} v;
    dbintbind(local_dbconn, REGISTRATION_ID_COLUMN, v.registration_id);
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
    int ins=0, upd=0;
    struct registration_dbrecord rec;
    bzero(&v, sizeof(v));
    RETCODE erc=dbnextrow(local_dbconn);
    while (erc==MORE_ROWS) {
	if (!global_action) {
	    dbcancel(local_dbconn); return BREAK_PROCESS;
	    }
	rec.registration_id=v.registration_id;
        rec.receipt_id=(v.receipt_id_isnull)?-1:v.receipt_id;
	rec.n_kkm=(v.n_kkm_isnull)?NULL:rec._n_kkm;
	    strcpy(rec._n_kkm, v.n_kkm);
	rec.n_session=(v.n_session_isnull)?-1:v.n_session;
	rec.n_check=(v.n_check_isnull)?-1:v.n_check;
	rec.n_doc=(v.n_doc_isnull)?-1:v.n_doc;
	rec.check_type=(v.check_type_isnull)?-1:v.check_type;
        rec.n_position=(v.n_position_isnull)?-1:v.n_position;
	rec.ware_id=(v.ware_id_isnull)?-1:v.ware_id;
	rec.barcode=(v.barcode_isnull)?NULL:rec._barcode;
	    strcpy(rec._barcode, v.barcode);
	rec.price=(v.price_isnull)?NULL:rec._price;
	    strcpy(rec._price, v.price);
	rec.counter=v.counter;
	int r=localdb_findRegistration(rec.registration_id);
	if (r==0) {
	    r=localdb_insertRegistration(&rec);
	    if (r>0) ins+=r;
	    }
	else if (r>0) {
	    r=localdb_updateRegistration(&rec);
	    if (r>0) upd+=r;
	    }
	if (r<0) {
	    dbcancel(local_dbconn); 
	    return VOLUME_FAIL;
	    }
	bzero(&v, sizeof(v));
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) {
	log_info("registrations", ins, upd);
	return 0;
	}
    dbcancel(local_dbconn); 
    return SERVER_FAIL;
    }

/******************************** ware_quantity ******************************/
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
    if (r<0) return SERVER_FAIL;
    if (r>0) return SERVERDB_UPDATE;
    if (dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    struct {
        DBINT registration_id;
        DBINT seller;		int seller_isnull;
        DBINT check_type;	int check_type_isnull;
        DBINT oper_type;	int oper_type_isnull;
	DBCHAR quantity[16];	int quantity_isnull;
	DBCHAR summ[16];	int summ_isnull;
	DBINT counter;
	DBINT volume_id;
	} v;
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
    int result=NO_ACTION;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
        result=NO_ACTION;
	rec->registration_id=v.registration_id;
        rec->seller=(v.seller_isnull)?-1:v.seller;
        rec->check_type=(v.check_type_isnull)?-1:v.check_type;
        rec->oper_type=(v.oper_type_isnull)?-1:v.oper_type;
        rec->quantity=(v.quantity_isnull)?NULL:rec->_quantity;
    	    strcpy(rec->_quantity, v.quantity);
	rec->summ=(v.summ_isnull)?NULL:rec->_summ;
	    strcpy(rec->_summ, v.summ);
	rec->counter=v.counter;
	if (v.volume_id!=volume_id) {
	    int r=localdb_deleteQuantity(rec->quantity_id);
	    if (r<0) result=VOLUME_FAIL;
	    else if (r>0) result=LOCALDB_DELETE;
	    }
	else if (rec->counter>counter) {
	    int r=localdb_updateQuantity(rec);
	    if (r<0) result=VOLUME_FAIL;
	    else if (r>0) result=LOCALDB_UPDATE;
	    }
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

int serverdb_processQuantity(int terminal_id, int volume_id, int max_counter) {
    enum {
	QUANTITY_ID_COLUMN=1,
	REGISTRATION_ID_COLUMN,
	SELLER_COLUMN,
	CHECK_TYPE_COLUMN,
	OPER_TYPE_COLUMN,
	QUANTITY_COLUMN,
	SUMM_COLUMN,
	COUNTER_COLUMN
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT quantity_id, registration_id, seller,"
		    " check_type, oper_type, quantity, summ, counter"
	    " FROM ware_quantity WHERE terminal_id="), AS_UINT(terminal_id),
		AS_TEXT(" AND volume_id="), AS_UINT(volume_id),
		AS_TEXT(" AND counter<="), AS_UINT(max_counter),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    struct {
	DBINT quantity_id;
        DBINT registration_id;	int registration_id_isnull;
        DBINT seller;		int seller_isnull;
        DBINT check_type;	int check_type_isnull;
        DBINT oper_type;	int oper_type_isnull;
	DBCHAR quantity[16];	int quantity_isnull;
	DBCHAR summ[16];	int summ_isnull;
	DBINT counter;
	} v;
    dbintbind(local_dbconn, QUANTITY_ID_COLUMN, v.quantity_id);
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
    int ins=0, upd=0;
    struct quantity_dbrecord rec;
    bzero(&v, sizeof(v));
    RETCODE erc=dbnextrow(local_dbconn);
    while(erc==MORE_ROWS) {
	if (!global_action) {
	    dbcancel(local_dbconn); return BREAK_PROCESS;
	    }
	rec.quantity_id=v.quantity_id;
	rec.registration_id=(v.registration_id_isnull)?-1:v.registration_id;
        rec.seller=(v.seller_isnull)?-1:v.seller;
	rec.check_type=(v.check_type_isnull)?-1:v.check_type;
        rec.oper_type=(v.oper_type_isnull)?-1:v.oper_type;
	rec.quantity=(v.quantity_isnull)?NULL:rec._quantity;
    	    strcpy(rec._quantity, v.quantity);
        rec.summ=(v.summ_isnull)?NULL:rec._summ;
	    strcpy(rec._summ, v.summ);
	rec.counter=v.counter;
	int r=localdb_findQuantity(rec.quantity_id);
	if (r==0) {
	    r=localdb_insertQuantity(&rec);
	    if (r>0) ins+=r;
	    }
	else if (r>0) {
	    r=localdb_updateQuantity(&rec);
	    if (r>0) upd+=r;
	    }
	if (r<0) {
	    dbcancel(local_dbconn);
    	    return VOLUME_FAIL;
	    }
	bzero(&v, sizeof(v));
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) {
	log_info("ware_quantity", ins, upd);
	return 0;
	}
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

/******************************* payment *************************************/
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
    if (r<0) return SERVER_FAIL;
    if (r>0) return SERVERDB_UPDATE;
    if (dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
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
    int result=NO_ACTION;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	result=NO_ACTION;
	rec->receipt_id=(v.receipt_id_isnull)?-1:v.receipt_id;
        rec->seller=(v.seller_isnull)?-1:v.seller;
	rec->check_type=(v.check_type_isnull)?-1:v.check_type;
        rec->payment_type=(v.payment_type_isnull)?-1:v.payment_type;
	rec->summ=(v.summ_isnull)?NULL:rec->_summ;
	    strcpy(rec->_summ, v.summ);
	rec->counter=v.counter;
	if (v.volume_id!=volume_id) {
	    int r=localdb_deletePayment(rec->payment_id);
	    if (r<0) result=VOLUME_FAIL;
	    else if (r>0) result=LOCALDB_DELETE;
	    }
	else if (rec->counter>counter) {
	    int r=localdb_updatePayment(rec);
	    if (r<0) result=VOLUME_FAIL;
	    if (r>0) result=LOCALDB_UPDATE;
	    }
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

int serverdb_processPayments(int terminal_id, int volume_id, int max_counter) {
    enum {
	PAYMENT_ID_COLUMN=1,
	RECEIPT_ID_COLUMN,
	SELLER_COLUMN,
	CHECK_TYPE_COLUMN,
	PAYMENT_TYPE_COLUMN,
	SUMM_COLUMN,
	COUNTER_COLUMN
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT payment_id, receipt_id, seller, check_type,"
	    " payment_type, summ, counter"
	    " FROM payments WHERE terminal_id="), AS_UINT(terminal_id),
		AS_TEXT(" AND volume_id="), AS_UINT(volume_id),
		AS_TEXT(" AND counter<="), AS_UINT(max_counter),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    struct {
	DBINT payment_id;
	DBINT receipt_id;	int receipt_id_isnull;	
	DBINT seller;		int seller_isnull;
        DBINT check_type;	int check_type_isnull;
        DBINT payment_type;	int payment_type_isnull;
	DBCHAR summ[16];	int summ_isnull;
	DBINT counter;
	} v;
    dbintbind(local_dbconn, PAYMENT_ID_COLUMN, v.payment_id);
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
    int ins=0, upd=0;
    struct payment_dbrecord rec;
    bzero(&v, sizeof(v));
    RETCODE erc=dbnextrow(local_dbconn);
    while(erc==MORE_ROWS) {
	if (!global_action) {
	    dbcancel(local_dbconn); return BREAK_PROCESS;
	    }
	rec.payment_id=v.payment_id;
	rec.receipt_id=(v.receipt_id_isnull)?-1:v.receipt_id;
        rec.seller=(v.seller_isnull)?-1:v.seller;
	rec.check_type=(v.check_type_isnull)?-1:v.check_type;
        rec.payment_type=(v.payment_type_isnull)?-1:v.payment_type;
	rec.summ=(v.summ_isnull)?NULL:rec._summ;
	    strcpy(rec._summ, v.summ);
	rec.counter=v.counter;
	int r=localdb_findPayment(rec.payment_id);
	if (r==0) {
	    r=localdb_insertPayment(&rec);
	    if (r>0) ins+=r;
	    }
	else if (r>0) {
	    r=localdb_updatePayment(&rec);
	    if (r>0) upd+=r;
	    }
	if (r<0) {
	    dbcancel(local_dbconn);
	    return VOLUME_FAIL;
	    }
	bzero(&v, sizeof(v));
        erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) {
	log_info("payments", ins, upd);
	return 0;
	}
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

/*****************************************************************************/

static int serverdb_getRecordsCount(char * table, int terminal_id, int counter) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT COUNT(*) FROM "), AS_TEXT(table),
	    AS_TEXT(" WHERE terminal_id="), AS_UINT(terminal_id),
	    AS_TEXT(" AND counter<="), AS_UINT(counter),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=0;
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

int serverdb_getSessionsCount(int terminal_id, int counter) {
    return serverdb_getRecordsCount("sessions", terminal_id, counter);
    }

int serverdb_getSessionCounter(int terminal_id) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT session_counter FROM terminals "
		    "WHERE terminal_id="), AS_UINT(terminal_id),
	END_ARGS)<0) return -1;
    if (dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=0;
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

static int serverdb_getVolumeRecordsCount(char * table, int terminal_id, int volume_id, int counter) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT COUNT(*) FROM "), AS_TEXT(table),
	    AS_TEXT(" WHERE terminal_id="), AS_UINT(terminal_id),
	    AS_TEXT(" AND volume_id="), AS_UINT(volume_id),
	    AS_TEXT(" AND counter<="), AS_UINT(counter),
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

int serverdb_getReceiptsCount(int terminal_id, int volume_id, int counter) {
    return serverdb_getVolumeRecordsCount("receipts", terminal_id, volume_id, counter);
    }
int serverdb_getRegistrationsCount(int terminal_id, int volume_id, int counter) {
    return serverdb_getVolumeRecordsCount("registrations", terminal_id, volume_id, counter);
    }
int serverdb_getQuantityCount(int terminal_id, int volume_id, int counter) {
    return serverdb_getVolumeRecordsCount("ware_quantity", terminal_id, volume_id, counter);
    }
int serverdb_getPaymentsCount(int terminal_id, int volume_id, int counter) {
    return serverdb_getVolumeRecordsCount("payments", terminal_id, volume_id, counter);
    }

int serverdb_getVolumes(struct recordset * rset, int terminal_id, int counter) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT volume_id FROM receipts"
		" WHERE terminal_id="), AS_UINT(terminal_id),
	    AS_TEXT(" AND counter<="), AS_UINT(counter),
	    AS_TEXT("  GROUP BY volume_id ORDER BY -volume_id"),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    DBINT v=0;
    dbintbind(local_dbconn, 1, v);
    int result=0;
    RETCODE erc=dbnextrow(local_dbconn);
    while(erc==MORE_ROWS) {
	if (!global_action) {
	    dbcancel(local_dbconn);
	    return BREAK_PROCESS;
	    }
	int * rec=recordset_new(rset, sizeof(int));
	*rec=v;
	result++;
	v=0;
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

int serverdb_getVolumeRecord(struct volumes_dbrecord * rec, int terminal_id, int counter) {
    enum {
	KKM_NUM_COLUMN=1,
	COUNTER_COLUMN
	};
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT n_kkm, MAX(counter) FROM receipts"
		" WHERE terminal_id="), AS_UINT(terminal_id),
	    AS_TEXT(" AND volume_id="), AS_UINT(rec->volume_id),
	    AS_TEXT(" AND counter<="), AS_UINT(counter),
	    AS_TEXT(" GROUP BY n_kkm"),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return SERVER_FAIL;
    struct {
	DBCHAR n_kkm[41];	int n_kkm_isnull;
	DBINT counter;		int counter_isnull;
	} v;
    dbnullbind(local_dbconn, KKM_NUM_COLUMN, &v.n_kkm_isnull);
	dbstringbind(local_dbconn, KKM_NUM_COLUMN, v.n_kkm);
    dbnullbind(local_dbconn, COUNTER_COLUMN, &v.counter_isnull);
	dbintbind(local_dbconn, COUNTER_COLUMN, v.counter);
    int result=-1;
    RETCODE erc=dbnextrow(local_dbconn);
    if (erc==MORE_ROWS) {
	rec->n_kkm=(v.n_kkm_isnull)?NULL:rec->_n_kkm;
	    strcpy(rec->_n_kkm, v.n_kkm);
        rec->counter=(v.counter_isnull)?-1:v.counter;
	result=0;	
	erc=dbnextrow(local_dbconn);
	}
    if (erc==NO_MORE_ROWS) return result;
    dbcancel(local_dbconn);
    return SERVER_FAIL;
    }

int serverdb_updateSessionOperID(int terminal_id, int counter) {
    return sybdb_execCmd(local_dbconn,
	AS_TEXT("UPDATE terminals SET session_counter="), AS_UINT(counter),
	    AS_TEXT(" WHERE terminal_id="), AS_UINT(terminal_id),
	END_ARGS);
    }

int serverdb_getVolumesCount(int terminal_id, int counter) {
    if (sybdb_execCmd(local_dbconn,
	AS_TEXT("SELECT COUNT(DISTINCT volume_id) FROM receipts"
	    " WHERE terminal_id="), AS_UINT(terminal_id),
	    AS_TEXT(" AND counter<="), AS_UINT(counter),
	END_ARGS)<0 || dbresults(local_dbconn)!=SUCCEED) return -1;
    int result=0;
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
