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
#include <sqlite3.h>

#include "vararg.h"
#include "conf.h"
#include "localdb.h"
#include "serverdb.h"
#include "sqlite_common.h"
#include "log.h"
#include "global.h"

#define SQLITE_BUSY_TIMEOUT	60000

extern int global_action;

static void internal_message(char *text) {
    log_printf(0, "ОШИБКА SQLITE: %s", text);
    }

static sqlite3 * local_ware_db=NULL;
static sqlite3 * local_main_db=NULL;
static sqlite3 * local_volume_db=NULL;

int localdb_openMainDB(char * dbname) {
#ifdef DEBUG
    if (local_main_db!=NULL) {
	fputs("*** localdb_openMainDB ASSERT: database already open\n", stderr);
	return 0;
	}
#endif
    int r=sqlite3_open(dbname, &local_main_db);
    if (r!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(local_main_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(local_main_db, SQLITE_BUSY_TIMEOUT);
    return 0;
    }

int localdb_openWareDB(char * dbname) {
#ifdef DEBUG
    if (local_ware_db!=NULL) {
	fputs("*** localdb_openWareDB ASSERT: database already open\n", stderr);
	return 0;
	}
#endif
    if (sqlite3_open(dbname, &local_ware_db)!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(local_ware_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(local_ware_db, SQLITE_BUSY_TIMEOUT);
    return 0;
    }

void localdb_closeWareDB(void) {
    if (local_ware_db!=NULL) {
	sqlite3_close(local_ware_db);
	local_ware_db=NULL;
	}
    }

void localdb_closeMainDB(void) {
    if (local_main_db!=NULL) {
	sqlite3_close(local_main_db);
	local_main_db=NULL;
	}
    }

/****************************************************************************/
static int localdb_getTableStruct(struct recordset * rset, char * table, sqlite3 * db) {
    enum {
	ID_COLUMN=0,
	NAME_COLUMN,
	TYPE_COLUMN,
	ISNULL_COLUMN,
	DEFAULT_COLUMN
	};
    sqlite3_stmt * req=sqlite_prepare(db,
	AS_TEXT("pragma table_info("), AS_STRING(table), AS_TEXT(")"),
	END_ARGS);
    if (req==NULL) return -1;
    int result=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	char s[255];
	strcpy(s, sqlite3_column_text(req, NAME_COLUMN));
	strcat(s, " ");
	strcat(s, sqlite3_column_text(req, TYPE_COLUMN));
	char * rec=recordset_new(rset, strlen(s)+1);
	strcpy(rec, s);
	result++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(db);
	internal_message(errmsg);
	result=-1;
	}
    sqlite3_finalize(req);
    return result;
    }

int localdb_getMainTableStruct(struct recordset * rset, char * table) {
    return localdb_getTableStruct(rset, table, local_main_db);
    }

int localdb_getWareTableStruct(struct recordset * rset, char * table) {
    return localdb_getTableStruct(rset, table, local_ware_db);
    }

int localdb_getVolumeTableStruct(struct recordset * rset, char * table) {
    return localdb_getTableStruct(rset, table, local_volume_db);
    }

int localdb_createMainTables(void) {
    if (sqlite_execCmdText(local_main_db,
	"CREATE TABLE _global_seq "
	    "(sessions_pk integer NOT NULL,"
	    " volumes_pk integer NOT NULL,"
	    " receipts_pk integer NOT NULL,"
	    " registrations_pk integer NOT NULL,"
	    " ware_quantity_pk integer NOT NULL,"
	    " payments_pk integer NOT NULL,"
	    " counter_seq integer NOT NULL)")<0) return -1;

    if (sqlite_execCmdText(local_main_db,
	"CREATE TABLE volumes "
	    "(volume_id INTEGER PRIMARY KEY,"
	    " volume_name varchar(100),"
	    " n_kkm varchar(20),"
	    " counter integer)")<0) return -1;

    if (sqlite_execCmdText(local_main_db,
	"CREATE TABLE sessions "
	    "(session_id INTEGER PRIMARY KEY,"
	    " date_time timestamp,"
	    " n_kkm varchar(20),"
	    " seller integer, "
	    " n_session integer,"
	    " n_doc integer,"
	    " total_summ numeric(15,2),"
	    " counter integer)")<0) return -1;
    return 0;
    }    

int localdb_createWareTables(void) {
    if (sqlite_execCmdText(local_ware_db,
	"CREATE TABLE ware "
	    "(ware_id INTEGER PRIMARY KEY,"
	    " longtitle varchar(255),"
	    " shortcut varchar(255),"
	    " price numeric(10,2),"
	    " quantity numeric(10,3),"
	    " group_id integer,"
	    " erase boolean)")<0) return -1;

    if (sqlite_execCmdText(local_ware_db,
	"CREATE TABLE barcodes "
	    "(internal_id INTEGER PRIMARY KEY,"
	    " ware_id integer,"
	    " barcode varchar(20),"
	    " erase boolean)")<0) return -1;
    return 0;
    }

static int waredb_recordsCount(char * table) {
    sqlite3_stmt * req=sqlite_prepare(local_ware_db,
	AS_TEXT("SELECT COUNT(*) FROM "), AS_TEXT(table),
	END_ARGS);
    if (req==NULL) return -1;
    int v=-1;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_ware_db);
        internal_message(errmsg);
	}
    sqlite3_finalize(req);
    return v;
    }

static int localdb_findRecord(sqlite3 * db, char * table, char * column, int value) {
    sqlite3_stmt * req=sqlite_prepare(db,
	AS_TEXT("SELECT COUNT(*) FROM "), AS_TEXT(table),
	AS_TEXT(" WHERE "), AS_TEXT(column), AS_TEXT("="), AS_UINT(value),
	END_ARGS);
    if (req==NULL) return -1;
    int v=-1;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(db);
        internal_message(errmsg);
	}
    sqlite3_finalize(req);
    return v;
    }

static int localdb_recordsCount(sqlite3 * db, char * table, int counter) {
    sqlite3_stmt * req=sqlite_prepare(db,
	AS_TEXT("SELECT COUNT(*) FROM "), AS_TEXT(table),
	    AS_TEXT(" WHERE counter<="), AS_UINT(counter),
	END_ARGS);
    if (req==NULL) return -1;
    int v=-1;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(db);
        internal_message(errmsg);
	}
    sqlite3_finalize(req);
    return v;
    }

/*****************************************************************************/
int localdb_groupsCount(void) {
    return waredb_recordsCount("groups");
    }

int localdb_findGroup(int id) {
    return localdb_findRecord(local_ware_db, "groups", "group_id", id);
    }

int localdb_insertGroup(struct group_dbrecord * rec) {
    return sqlite_execCmd(local_ware_db,
	AS_TEXT("INSERT INTO groups(group_id, name, parent_id) "
		"VALUES ("), AS_UINT(rec->group_id), AS_TEXT(" ,"),
			     AS_STRING(rec->name), AS_TEXT(" ,"),
		             AS_UINT(rec->parent_id), AS_TEXT(")"),
	END_ARGS);
    }

/********************************** ware *************************************/
int localdb_wareCount(void) {
    return waredb_recordsCount("ware");
    }

int localdb_findWare(int id) {
    return localdb_findRecord(local_ware_db, "ware", "ware_id", id);
    }

int localdb_insertWare(struct ware_dbrecord * rec) {
    return sqlite_execCmd(local_ware_db,
	AS_TEXT("INSERT INTO ware(ware_id, group_id, longtitle, shortcut, price)"
		" VALUES ("), AS_UINT(rec->ware_id), AS_TEXT(" ,"),
		              AS_UINT(rec->group_id), AS_TEXT(" ,"),
		 	      AS_STRING(rec->longtitle), AS_TEXT(" ,"),
			      AS_STRING(rec->shortcut), AS_TEXT(" ,"),
		              AS_TEXT(rec->price), AS_TEXT(")"),
	END_ARGS);
    }

/******************************* barcodes ***********************************/
int localdb_barcodesCount(void) {
    return waredb_recordsCount("barcodes");
    }

int localdb_findBarcode(int ware_id, char * bcode) {
    sqlite3_stmt * req=sqlite_prepare(local_ware_db,
	AS_TEXT("SELECT COUNT(*) FROM barcodes "
		"WHERE ware_id="), AS_UINT(ware_id),
	    AS_TEXT(" AND barcode="), AS_STRING(bcode),
	END_ARGS);
    if (req==NULL) return -1;
    int v=-1;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_ware_db);
        internal_message(errmsg);
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_insertBarcode(struct barcode_dbrecord * rec) {
    return sqlite_execCmd(local_ware_db,
	AS_TEXT("INSERT INTO barcodes(ware_id, barcode) VALUES ("),
		AS_UINT(rec->ware_id), AS_TEXT(", "),
		AS_STRING(rec->barcode), AS_TEXT(")"),
	END_ARGS);
    }
/*****************************************************************************/

int localdb_getGlobalSeq(struct globalseq_dbrecord * rec) {
    enum {
	VOLUMES_PK_COLUMN=0,
	SESSIONS_PK_COLUMN,
	RECEIPTS_PK_COLUMN,
	REGISTRATIONS_PK_COLUMN,
	WARE_QUANTITY_PK_COLUMN,
	PAYMENTS_PK_COLUMN,
	COUNTER_SEQ_COLUMN
	};
    sqlite3_stmt * req=sqlite_prepareText(local_main_db,
	"SELECT volumes_pk, sessions_pk, receipts_pk, registrations_pk,"
	    " ware_quantity_pk, payments_pk, counter_seq FROM _global_seq");
    if (req==NULL) {
	return -1;
	}
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	rec->volumes_pk=sqlite3_column_int(req,VOLUMES_PK_COLUMN);
	rec->sessions_pk=sqlite3_column_int(req,SESSIONS_PK_COLUMN);
	rec->receipts_pk=sqlite3_column_int(req,RECEIPTS_PK_COLUMN);
	rec->registrations_pk=sqlite3_column_int(req,REGISTRATIONS_PK_COLUMN);
	rec->ware_quantity_pk=sqlite3_column_int(req,WARE_QUANTITY_PK_COLUMN);
	rec->payments_pk=sqlite3_column_int(req,PAYMENTS_PK_COLUMN);
	rec->counter_seq=sqlite3_column_int(req, COUNTER_SEQ_COLUMN);
	v=1;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_main_db);
        internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

static int localdb_setSequence(char * seq, int value) {
    return sqlite_execCmd(local_main_db,
	AS_TEXT("UPDATE _global_seq SET "),
	    AS_TEXT(seq), AS_TEXT("="), AS_UINT(value),
	END_ARGS);
    }

int localdb_setVolumesSeq(int v) {
    return localdb_setSequence("volumes_pk", v);
    }
int localdb_setSessionsSeq(int v) {
    return localdb_setSequence("sessions_pk", v);
    }
int localdb_setReceiptsSeq(int v) {
    return localdb_setSequence("receipts_pk", v);
    }
int localdb_setRegistrationsSeq(int v) {
    return localdb_setSequence("registrations_pk", v);
    }
int localdb_setQuantitySeq(int v) {
    return localdb_setSequence("ware_quantity_pk", v);
    }
int localdb_setPaymentsSeq(int v) {
    return localdb_setSequence("payments_pk", v);
    }

int localdb_getCounterSeq(void) {
    sqlite3_stmt * req=sqlite_prepareText(local_main_db,
	"SELECT counter_seq FROM _global_seq");
    if (req==NULL) return MAINDB_FAIL;
    int v=DB_EMPTY;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_ware_db);
        internal_message(errmsg);
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_createGlobalSeq(struct globalseq_dbrecord * rec) {
    return sqlite_execCmd(local_main_db,
	AS_TEXT("INSERT INTO _global_seq (volumes_pk, sessions_pk,"
	    " receipts_pk, registrations_pk, ware_quantity_pk,"
	    " payments_pk, counter_seq) VALUES ("),
	    AS_UINT(rec->volumes_pk), AS_TEXT(", "),
	    AS_UINT(rec->sessions_pk), AS_TEXT(", "),
	    AS_UINT(rec->receipts_pk), AS_TEXT(", "),
	    AS_UINT(rec->registrations_pk), AS_TEXT(", "),
	    AS_UINT(rec->ware_quantity_pk), AS_TEXT(", "),
	    AS_UINT(rec->payments_pk), AS_TEXT(", "),
	    AS_UINT(rec->counter_seq), AS_TEXT(")"),
	END_ARGS);
    }

/****************************************************************************/
int localdb_findSession(int id) {
    return localdb_findRecord(local_main_db, "sessions", "session_id", id);
    }

int localdb_insertSession(struct session_dbrecord * rec) {
    return sqlite_execCmd(local_main_db,
//    int r=sqlite_execCmd(local_main_db,
	AS_TEXT("INSERT INTO sessions(session_id, date_time,"
		    " n_kkm, seller, n_session, n_doc,"
		    " total_summ, counter) VALUES ("),
	    AS_UINT(rec->session_id), AS_TEXT(", "),
	    AS_STRING(rec->date_time), AS_TEXT(", "),
	    AS_STRING(rec->n_kkm), AS_TEXT(", "),
	    AS_UINT(rec->seller), AS_TEXT(", "),
	    AS_UINT(rec->n_session), AS_TEXT(", "),
	    AS_UINT(rec->n_doc), AS_TEXT(", "),
	    AS_TEXT(rec->total_summ), AS_TEXT(", "),
	    AS_UINT(rec->counter), AS_TEXT(")"),
	END_ARGS);
//printf("*** localdb_insertSession(): r=%d\n",r);
//return r;
    }

int localdb_updateSession(struct session_dbrecord * rec) {
    return sqlite_execCmd(local_main_db,
	AS_TEXT("UPDATE sessions SET date_time="), AS_STRING(rec->date_time),
	    AS_TEXT(", n_kkm="), AS_STRING(rec->n_kkm), 
	    AS_TEXT(", seller="), AS_UINT(rec->seller), 
	    AS_TEXT(", n_session="), AS_UINT(rec->n_session), 
	    AS_TEXT(", n_doc="), AS_UINT(rec->n_doc), 
	    AS_TEXT(", total_summ="), AS_TEXT(rec->total_summ),
	    AS_TEXT(", counter="), AS_UINT(rec->counter),
	    AS_TEXT(" WHERE session_id="), AS_UINT(rec->session_id),
	    AS_TEXT(" AND counter<"), AS_UINT(rec->counter),
	END_ARGS);
    }

int localdb_getSessionsCount(int counter) {
    return localdb_recordsCount(local_main_db, "sessions", counter);
    }

int localdb_getSessions(struct recordset * rset, int counter) {
    enum {
	SESSION_ID_COLUMN=0,
	DATE_TIME_COLUMN,
	KKM_NUM_COLUMN,
	SELLER_COLUMN,
	SESSION_NUM_COLUMN,
	DOC_NUM_COLUMN,
	TOTAL_SUMM_COLUMN,
	COUNTER_COLUMN
	};

    sqlite3_stmt * req=sqlite_prepare(local_main_db,
	AS_TEXT("SELECT session_id, date_time, n_kkm, seller,"
		" n_session, n_doc, total_summ, counter "
		"FROM sessions WHERE counter<="), AS_UINT(counter),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (!global_action) {
	    sqlite3_finalize(req);
	    return BREAK_PROCESS;
	    }
	struct session_dbrecord * rec=recordset_new(rset, sizeof(struct session_dbrecord));
	rec->session_id=(sqlite3_column_type(req, SESSION_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SESSION_ID_COLUMN);
	if (sqlite3_column_type(req, DATE_TIME_COLUMN)==SQLITE_NULL) rec->date_time=NULL;
	else {
	    strcpy(rec->_date_time, sqlite3_column_text(req, DATE_TIME_COLUMN));
    	    rec->date_time=rec->_date_time;
	    }
	if (sqlite3_column_type(req, KKM_NUM_COLUMN)==SQLITE_NULL) rec->n_kkm=NULL;
	else {
	    strcpy(rec->_n_kkm, sqlite3_column_text(req, KKM_NUM_COLUMN));
	    rec->n_kkm=rec->_n_kkm;
	    }
        rec->seller=(sqlite3_column_type(req, SELLER_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SELLER_COLUMN);
        rec->n_session=(sqlite3_column_type(req, SESSION_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SESSION_NUM_COLUMN);
        rec->n_doc=(sqlite3_column_type(req, DOC_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, DOC_NUM_COLUMN);
	if (sqlite3_column_type(req, TOTAL_SUMM_COLUMN)==SQLITE_NULL) rec->total_summ=NULL;
	else {
	    strcpy(rec->_total_summ, sqlite3_column_text(req,TOTAL_SUMM_COLUMN));
            rec->total_summ=rec->_total_summ;
	    }
	rec->counter=sqlite3_column_int(req,COUNTER_COLUMN);
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_main_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

/***************************************************************************/
int localdb_openVolumeName(char * name) {
#ifdef DEBUG
printf("* openVolumeName() <%s>\n",name);
    if (local_volume_db!=NULL) {
	fputs("*** localdb_openVolumeName ASSERT: database already open\n",stderr);
	return 0;
	}
#endif
    if (sqlite3_open(name, &local_volume_db)!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(local_volume_db, SQLITE_BUSY_TIMEOUT);
    return 0;
    }    

void localdb_closeVolume(void) {
    if (local_volume_db!=NULL) {
	sqlite3_close(local_volume_db);
	local_volume_db=NULL;
	}
    }

int localdb_getVolume(struct volumes_dbrecord * rec) {
    enum {
	VOLUME_NAME_COLUMN=0,
	KKM_NUM_COLUMN,
	COUNTER_COLUMN
	};
    sqlite3_stmt * req=sqlite_prepare(local_main_db,
	AS_TEXT("SELECT volume_name, n_kkm, counter FROM volumes "
		"WHERE volume_id="), AS_UINT(rec->volume_id), 
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	if (sqlite3_column_type(req, VOLUME_NAME_COLUMN)==SQLITE_NULL) rec->volume_name=NULL;
	else {
	    strcpy(rec->_volume_name, sqlite3_column_text(req, VOLUME_NAME_COLUMN));
	    rec->volume_name=rec->_volume_name;
	    }
	if (sqlite3_column_type(req, KKM_NUM_COLUMN)==SQLITE_NULL) rec->n_kkm=NULL;
	else {
	    strcpy(rec->_n_kkm, sqlite3_column_text(req, KKM_NUM_COLUMN));
	    rec->n_kkm=rec->_n_kkm;
	    }
	rec->counter=sqlite3_column_int(req, COUNTER_COLUMN);
	v=1;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_main_db);
        internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }    

int localdb_createVolume(void) {
    if (sqlite_execCmdText(local_volume_db,
	"CREATE TABLE receipts ("
	    "receipt_id INTEGER PRIMARY KEY, "
	    "session_id integer, "
	    "date_time timestamp, "
	    "n_kkm varchar(20), "
	    "seller integer, "
	    "n_session integer, "
	    "n_check integer, "
	    "n_doc integer, "
	    "check_type integer, "
	    "oper_type integer, "
	    "counter integer)")<0) return -1;

    if (sqlite_execCmdText(local_volume_db,
	"CREATE TABLE registrations ("
	    "registration_id INTEGER PRIMARY KEY, "
	    "receipt_id integer, "
	    "n_kkm varchar(20), "
	    "n_session integer, "
	    "n_check integer, "
	    "n_doc integer, "
	    "check_type integer, "
	    "n_position integer, "
	    "ware_id integer, "
	    "barcode varchar(20), "
	    "price numeric(10,2), "
	    "counter integer)")<0) return -1;

    if (sqlite_execCmdText(local_volume_db,
	"CREATE TABLE ware_quantity("
	    "quantity_id INTEGER PRIMARY KEY, "
	    "registration_id integer, "
	    "seller integer, "
	    "check_type integer, "
	    "oper_type integer, "
	    "quantity numeric(10,3), "
	    "summ numeric(10,2), "
	    "counter integer)")<0) return -1;

    if (sqlite_execCmdText(local_volume_db,
	"CREATE TABLE payments ("
	    "payment_id INTEGER PRIMARY KEY, "
	    "receipt_id integer, "
	    "seller integer, "
	    "check_type integer, "
	    "payment_type integer, "
	    "summ numeric(10,2), "
	    "counter integer)")<0) return -1;
    return 0;
    }    

static int volume_deleteRecord(char * table, char * column, int v) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("DELETE FROM "), AS_TEXT(table),
	AS_TEXT(" WHERE "), AS_TEXT(column), AS_TEXT("="), AS_UINT(v),
	END_ARGS);
    }
/********************************* receipts *********************************/
int localdb_findReceipt(int id) {
    return localdb_findRecord(local_volume_db, "receipts", "receipt_id", id);
    }

int localdb_deleteReceipt(int id) {
    return volume_deleteRecord("receipts", "receipt_id", id);
    }

int localdb_getReceiptsCount(int counter) {
    return localdb_recordsCount(local_volume_db, "receipts", counter);
    }

int localdb_getReceipts(struct recordset * rset, int counter) {
    enum {
	RECEIPT_ID_COLUMN=0,
	SESSION_ID_COLUMN,
	DATE_TIME_COLUMN,
	KKM_NUM_COLUMN,
	SELLER_COLUMN,
	SESSION_NUM_COLUMN,
	CHECK_NUM_COLUMN,
	DOC_NUM_COLUMN,
	CHECK_TYPE_COLUMN,
	OPER_TYPE_COLUMN,
	COUNTER_COLUMN
	};

    sqlite3_stmt * req=sqlite_prepare(local_volume_db,
	AS_TEXT("SELECT receipt_id, session_id, date_time, n_kkm,"
		    " seller, n_session, n_check, n_doc, check_type,"
		    " oper_type, counter "
		"FROM receipts WHERE counter<="), AS_UINT(counter),
	END_ARGS);
    if (req==NULL) return -1;
    int err=BREAK_PROCESS;
    struct receipt_dbrecord rec;
    while (global_action) {
	int r=sqlite3_step(req);
	if (r==SQLITE_DONE) {
	    err=0;
	    break;
	    }
	if (r!=SQLITE_ROW) {
	    char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	    internal_message(errmsg);
	    err=-1;
	    break;
	    }
	struct receipt_dbrecord * rec=recordset_new(rset, sizeof(struct receipt_dbrecord));
	rec->receipt_id=(sqlite3_column_type(req, RECEIPT_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, RECEIPT_ID_COLUMN);
	rec->session_id=(sqlite3_column_type(req, SESSION_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SESSION_ID_COLUMN);
	if (sqlite3_column_type(req, DATE_TIME_COLUMN)==SQLITE_NULL) rec->date_time=NULL;
	else {
	    strcpy(rec->_date_time, sqlite3_column_text(req, DATE_TIME_COLUMN));
	    rec->date_time=rec->_date_time;
	    }
	if (sqlite3_column_type(req, KKM_NUM_COLUMN)==SQLITE_NULL) rec->n_kkm=NULL;
	else {
	    strcpy(rec->_n_kkm, sqlite3_column_text(req, KKM_NUM_COLUMN));
	    rec->n_kkm=rec->_n_kkm;
	    }
        rec->seller=(sqlite3_column_type(req, SELLER_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SELLER_COLUMN);
        rec->n_session=(sqlite3_column_type(req, SESSION_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SESSION_NUM_COLUMN);
        rec->n_check=(sqlite3_column_type(req, CHECK_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, CHECK_NUM_COLUMN);
        rec->n_doc=(sqlite3_column_type(req, DOC_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, DOC_NUM_COLUMN);
        rec->check_type=(sqlite3_column_type(req, CHECK_TYPE_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, CHECK_TYPE_COLUMN);
        rec->oper_type=(sqlite3_column_type(req, OPER_TYPE_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, OPER_TYPE_COLUMN);
	rec->counter=sqlite3_column_int(req,COUNTER_COLUMN);
	}
    sqlite3_finalize(req);
    return err;
    }

int localdb_insertReceipt(struct receipt_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("INSERT INTO receipts(receipt_id, session_id,"
		" date_time, n_kkm, seller, n_session, n_check,"
		" n_doc, check_type, oper_type, counter) VALUES ("),
	    AS_UINT(rec->receipt_id), AS_TEXT(", "),
	    AS_UINT(rec->session_id), AS_TEXT(", "),
	    AS_STRING(rec->date_time), AS_TEXT(", "),
	    AS_STRING(rec->n_kkm), AS_TEXT(", "),
	    AS_UINT(rec->seller), AS_TEXT(", "),
	    AS_UINT(rec->n_session), AS_TEXT(", "),
	    AS_UINT(rec->n_check), AS_TEXT(", "),
	    AS_UINT(rec->n_doc), AS_TEXT(", "),
	    AS_UINT(rec->check_type), AS_TEXT(", "),
	    AS_UINT(rec->oper_type), AS_TEXT(", "),
	    AS_UINT(rec->counter), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_updateReceipt(struct receipt_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("UPDATE receipts SET session_id="), AS_UINT(rec->session_id),
	    AS_TEXT(", date_time="), AS_STRING(rec->date_time),
	    AS_TEXT(", n_kkm="), AS_STRING(rec->n_kkm), 
	    AS_TEXT(", seller="), AS_UINT(rec->seller), 
	    AS_TEXT(", n_session="), AS_UINT(rec->n_session), 
	    AS_TEXT(", n_check="), AS_UINT(rec->n_check), 
	    AS_TEXT(", n_doc="), AS_UINT(rec->n_doc), 
	    AS_TEXT(", check_type="), AS_UINT(rec->check_type), 
	    AS_TEXT(", oper_type="), AS_UINT(rec->oper_type), 
	    AS_TEXT(", counter="), AS_UINT(rec->counter), 
	    AS_TEXT(" WHERE receipt_id="), AS_UINT(rec->receipt_id),
	    AS_TEXT(" AND counter<"), AS_UINT(rec->counter),
	END_ARGS);
    }

/**************************** registrations ********************************/
int localdb_findRegistration(int id) {
    return localdb_findRecord(local_volume_db, "registrations", "registration_id", id);
    }

int localdb_deleteRegistration(int id) {
    return volume_deleteRecord("registrations", "registration_id", id);
    }

int localdb_getRegistrationsCount(int counter) {
    return localdb_recordsCount(local_volume_db, "registrations", counter);
    }

int localdb_getRegistrations(struct recordset * rset, int counter) {
    enum {
	REGISTRATION_ID_COLUMN=0,
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
	COUNTER_COLUMN
	};

    sqlite3_stmt * req=sqlite_prepare(local_volume_db,
	AS_TEXT("SELECT registration_id, receipt_id, n_kkm, n_session,"
		    " n_check, n_doc, check_type, n_position, ware_id,"
		    " barcode, price, counter"
		" FROM registrations WHERE counter<="), AS_UINT(counter),
	END_ARGS);
    if (req==NULL) return -1;
    int err=BREAK_PROCESS;
    struct registration_dbrecord rec;
    while (global_action) {
	int r=sqlite3_step(req);
	if (r==SQLITE_DONE) {
	    err=0; break;
	    }
	if (r!=SQLITE_ROW) {
	    char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	    internal_message(errmsg);
	    err=-1; break;
	    }
	struct registration_dbrecord * rec=recordset_new(rset, sizeof(struct registration_dbrecord));
	rec->registration_id=(sqlite3_column_type(req, REGISTRATION_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, REGISTRATION_ID_COLUMN);
	rec->receipt_id=(sqlite3_column_type(req, RECEIPT_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, RECEIPT_ID_COLUMN);
	if (sqlite3_column_type(req, KKM_NUM_COLUMN)==SQLITE_NULL) rec->n_kkm=NULL;
	else {
	    strcpy(rec->_n_kkm, sqlite3_column_text(req, KKM_NUM_COLUMN));
	    rec->n_kkm=rec->_n_kkm;
	    }
        rec->n_session=(sqlite3_column_type(req, SESSION_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SESSION_NUM_COLUMN);
        rec->n_check=(sqlite3_column_type(req, CHECK_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, CHECK_NUM_COLUMN);
        rec->n_doc=(sqlite3_column_type(req, DOC_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, DOC_NUM_COLUMN);
        rec->check_type=(sqlite3_column_type(req, CHECK_TYPE_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, CHECK_TYPE_COLUMN);
        rec->n_position=(sqlite3_column_type(req, POSITION_NUM_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, POSITION_NUM_COLUMN);
	rec->ware_id=(sqlite3_column_type(req, WARE_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, WARE_ID_COLUMN);
	if (sqlite3_column_type(req, BARCODE_COLUMN)==SQLITE_NULL) rec->barcode=NULL;
	else {
	    strcpy(rec->_barcode, sqlite3_column_text(req, BARCODE_COLUMN));
	    rec->barcode=rec->_barcode;
	    }
	if (sqlite3_column_type(req, PRICE_COLUMN)==SQLITE_NULL) rec->price=NULL;
	else {
	    strcpy(rec->_price, sqlite3_column_text(req, PRICE_COLUMN));
	    rec->price=rec->_price;
	    }
	rec->counter=sqlite3_column_int(req,COUNTER_COLUMN);
	}
    sqlite3_finalize(req);
    return err;
    }

int localdb_insertRegistration(struct registration_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("INSERT INTO registrations(registration_id, receipt_id,"
		" n_kkm, n_session, n_check, n_doc, check_type,"
		" n_position, ware_id, barcode, price, counter) VALUES ("),
	    AS_UINT(rec->registration_id), AS_TEXT(", "),
	    AS_UINT(rec->receipt_id), AS_TEXT(", "),
	    AS_STRING(rec->n_kkm), AS_TEXT(", "),
	    AS_UINT(rec->n_session), AS_TEXT(", "),
	    AS_UINT(rec->n_check), AS_TEXT(", "),
	    AS_UINT(rec->n_doc), AS_TEXT(", "),
	    AS_UINT(rec->check_type), AS_TEXT(", "),
	    AS_UINT(rec->n_position), AS_TEXT(", "),
	    AS_UINT(rec->ware_id), AS_TEXT(", "),
	    AS_STRING(rec->barcode), AS_TEXT(", "),
	    AS_TEXT(rec->price), AS_TEXT(", "),
	    AS_UINT(rec->counter), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_updateRegistration(struct registration_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("UPDATE registrations SET receipt_id="), AS_UINT(rec->receipt_id),
	    AS_TEXT(", n_kkm="), AS_STRING(rec->n_kkm), 
	    AS_TEXT(", n_session="), AS_UINT(rec->n_session), 
	    AS_TEXT(", n_check="), AS_UINT(rec->n_check), 
	    AS_TEXT(", n_doc="), AS_UINT(rec->n_doc), 
	    AS_TEXT(", check_type="), AS_UINT(rec->check_type), 
	    AS_TEXT(", n_position="), AS_UINT(rec->n_position), 
	    AS_TEXT(", ware_id="), AS_UINT(rec->ware_id),
	    AS_TEXT(", barcode="), AS_STRING(rec->barcode),
	    AS_TEXT(", price="), AS_TEXT(rec->price),
	    AS_TEXT(", counter="), AS_UINT(rec->counter), 
	    AS_TEXT(" WHERE registration_id="), AS_UINT(rec->registration_id),
	    AS_TEXT(" AND counter<"), AS_UINT(rec->counter),
	END_ARGS);
    }

/**************************** quantity ********************************/
int localdb_findQuantity(int id) {
    return localdb_findRecord(local_volume_db, "ware_quantity", "quantity_id", id);
    }

int localdb_deleteQuantity(int id) {
    return volume_deleteRecord("ware_quantity", "quantity_id", id);
    }

int localdb_getQuantityCount(int counter) {
    return localdb_recordsCount(local_volume_db, "ware_quantity", counter);
    }

int localdb_getQuantity(struct recordset * rset, int counter) {
    enum {
	QUANTITY_ID_COLUMN=0,
	REGISTRATION_ID_COLUMN,
	SELLER_COLUMN,
	CHECK_TYPE_COLUMN,
	OPER_TYPE_COLUMN,
	QUANTITY_COLUMN,
	SUMM_COLUMN,
	COUNTER_COLUMN
	};

    sqlite3_stmt * req=sqlite_prepare(local_volume_db,
	AS_TEXT("SELECT quantity_id, registration_id, seller, check_type,"
		" oper_type, quantity, summ, counter"
		" FROM ware_quantity WHERE counter<="), AS_UINT(counter),
	END_ARGS);
    if (req==NULL) return -1;
    int r=0;
    int err=BREAK_PROCESS;
    while (global_action) {
	int r=sqlite3_step(req);
	if (r==SQLITE_DONE) {
	    err=0; break;
	    }
	if (r!=SQLITE_ROW) {
	    char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	    internal_message(errmsg);
	    err=-1; break;
	    }
	struct quantity_dbrecord * rec=recordset_new(rset, sizeof(struct quantity_dbrecord));
	rec->quantity_id=(sqlite3_column_type(req, QUANTITY_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, QUANTITY_ID_COLUMN);
	rec->registration_id=(sqlite3_column_type(req, REGISTRATION_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, REGISTRATION_ID_COLUMN);
        rec->seller=(sqlite3_column_type(req, SELLER_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SELLER_COLUMN);
        rec->check_type=(sqlite3_column_type(req, CHECK_TYPE_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, CHECK_TYPE_COLUMN);
        rec->oper_type=(sqlite3_column_type(req, OPER_TYPE_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, OPER_TYPE_COLUMN);
	if (sqlite3_column_type(req, QUANTITY_COLUMN)==SQLITE_NULL) rec->quantity=NULL;
	else {
	    strcpy(rec->_quantity, sqlite3_column_text(req, QUANTITY_COLUMN));
	    rec->quantity=rec->_quantity;
	    }
	if (sqlite3_column_type(req, SUMM_COLUMN)==SQLITE_NULL) rec->summ=NULL;
	else {
	    strcpy(rec->_summ, sqlite3_column_text(req, SUMM_COLUMN));
	    rec->summ=rec->_summ;
	    }
	rec->counter=sqlite3_column_int(req, COUNTER_COLUMN);
	}
    sqlite3_finalize(req);
    return err;
    }

int localdb_insertQuantity(struct quantity_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("INSERT INTO ware_quantity(quantity_id, registration_id,"
	    " seller, check_type, oper_type, quantity, summ,"
	    " counter) VALUES ("),
	    AS_UINT(rec->quantity_id), AS_TEXT(", "),
	    AS_UINT(rec->registration_id), AS_TEXT(", "),
	    AS_UINT(rec->seller), AS_TEXT(", "),
	    AS_UINT(rec->check_type), AS_TEXT(", "),
	    AS_UINT(rec->oper_type), AS_TEXT(", "),
	    AS_TEXT(rec->quantity), AS_TEXT(", "),
	    AS_TEXT(rec->summ), AS_TEXT(", "),
	    AS_UINT(rec->counter), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_updateQuantity(struct quantity_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("UPDATE ware_quantity SET registration_id="), AS_UINT(rec->registration_id),
	    AS_TEXT(", seller="), AS_UINT(rec->seller), 
	    AS_TEXT(", check_type="), AS_UINT(rec->check_type), 
	    AS_TEXT(", oper_type="), AS_UINT(rec->oper_type), 
	    AS_TEXT(", quantity="), AS_TEXT(rec->quantity), 
	    AS_TEXT(", summ="), AS_TEXT(rec->summ), 
	    AS_TEXT(", counter="), AS_UINT(rec->counter), 
	    AS_TEXT(" WHERE quantity_id="), AS_UINT(rec->quantity_id),
	    AS_TEXT(" AND counter<"), AS_UINT(rec->counter),
	END_ARGS);
    }

/******************************* payments ***********************************/
int localdb_findPayment(int id) {
    return localdb_findRecord(local_volume_db, "payments", "payment_id", id);
    }

int localdb_deletePayment(int id) {
    return volume_deleteRecord("payments", "payment_id", id);
    }

int localdb_getPaymentsCount(int counter) {
    return localdb_recordsCount(local_volume_db, "payments", counter);
    }

int localdb_getPayments(struct recordset * rset, int counter) {
    enum {
	PAYMENT_ID_COLUMN=0,
	RECEIPT_ID_COLUMN,
	SELLER_COLUMN,
	CHECK_TYPE_COLUMN,
	PAYMENT_TYPE_COLUMN,
	SUMM_COLUMN,
	COUNTER_COLUMN
	};

    sqlite3_stmt * req=sqlite_prepare(local_volume_db,
	AS_TEXT("SELECT payment_id, receipt_id, seller, check_type,"
		" payment_type, summ, counter "
		"FROM payments WHERE counter<="), AS_UINT(counter),
	END_ARGS);
    if (req==NULL) return -1;
    int result=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (!global_action) {
	    sqlite3_finalize(req);
	    return BREAK_PROCESS;
	    }
	struct payment_dbrecord * rec=recordset_new(rset, sizeof(struct payment_dbrecord));
	rec->payment_id=(sqlite3_column_type(req, PAYMENT_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, PAYMENT_ID_COLUMN);
	rec->receipt_id=(sqlite3_column_type(req, RECEIPT_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, RECEIPT_ID_COLUMN);
        rec->seller=(sqlite3_column_type(req, SELLER_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, SELLER_COLUMN);
        rec->check_type=(sqlite3_column_type(req, CHECK_TYPE_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, CHECK_TYPE_COLUMN);
        rec->payment_type=(sqlite3_column_type(req, PAYMENT_TYPE_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, PAYMENT_TYPE_COLUMN);
	if (sqlite3_column_type(req, SUMM_COLUMN)==SQLITE_NULL) rec->summ=NULL;
	else {
	    strcpy(rec->_summ, sqlite3_column_text(req, SUMM_COLUMN));
	    rec->summ=rec->_summ;
	    }
	rec->counter=sqlite3_column_int(req, COUNTER_COLUMN);
	result++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	internal_message(errmsg);
	result=VOLUME_FAIL;
	}
    sqlite3_finalize(req);
    return result;
    }

int localdb_insertPayment(struct payment_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("INSERT INTO payments(payment_id, receipt_id, seller,"
	    " check_type, payment_type, summ, counter) VALUES ("),
	    AS_UINT(rec->payment_id), AS_TEXT(", "),
	    AS_UINT(rec->receipt_id), AS_TEXT(", "),
	    AS_UINT(rec->seller), AS_TEXT(", "),
	    AS_UINT(rec->check_type), AS_TEXT(", "),
	    AS_UINT(rec->payment_type), AS_TEXT(", "),
	    AS_TEXT(rec->summ), AS_TEXT(", "),
	    AS_UINT(rec->counter), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_updatePayment(struct payment_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("UPDATE payments SET receipt_id="), AS_UINT(rec->receipt_id),
	    AS_TEXT(", seller="), AS_UINT(rec->payment_type), 
	    AS_TEXT(", check_type="), AS_UINT(rec->payment_type), 
	    AS_TEXT(", payment_type="), AS_UINT(rec->payment_type), 
	    AS_TEXT(", summ="), AS_TEXT(rec->summ), 
	    AS_TEXT(", counter="), AS_UINT(rec->counter), 
	    AS_TEXT(" WHERE payment_id="), AS_UINT(rec->payment_id),
	    AS_TEXT(" AND counter<"), AS_UINT(rec->counter),
	END_ARGS);
    }

int localdb_setVolumeName(int id, char * name) {
    return sqlite_execCmd(local_main_db,
	AS_TEXT("UPDATE volumes SET volume_name="), AS_STRING(name),
	    AS_TEXT(" WHERE volume_id="), AS_UINT(id),
	END_ARGS);
    }

static int localdb_updateVolume(struct volumes_dbrecord * rec) {
    return sqlite_execCmd(local_main_db,
	AS_TEXT ("UPDATE volumes SET volume_name="), AS_STRING(rec->volume_name), 
	    AS_TEXT(", n_kkm="), AS_STRING(rec->n_kkm),
	    AS_TEXT(" WHERE volume_id="), AS_UINT(rec->volume_id),
	END_ARGS);
    }

static int localdb_insertVolume(struct volumes_dbrecord * rec) {
    return sqlite_execCmd(local_main_db,
	AS_TEXT ("INSERT INTO volumes(volume_id, volume_name, n_kkm) VALUES ("),
	    AS_UINT(rec->volume_id), AS_TEXT(", "),
	    AS_STRING(rec->volume_name), AS_TEXT(", "),
	    AS_STRING(rec->n_kkm), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_addVolume(struct volumes_dbrecord * rec) {
    int r=localdb_updateVolume(rec);
    if (r<0) return -1;
    if (r>0) return 0;
    return localdb_insertVolume(rec);
    }

int localdb_getVolumesCount(int counter) {
    return localdb_recordsCount(local_main_db, "volumes", counter);
    }

int localdb_getVolumes(struct recordset * rset, int counter) {
    sqlite3_stmt * req=sqlite_prepare(local_main_db,
	AS_TEXT("SELECT volume_id FROM volumes"
	    " WHERE counter IS NULL OR counter<="), AS_UINT(counter), 
	    AS_TEXT(" ORDER BY -volume_id"),
	END_ARGS);
    if (req==NULL) return -1;
    int result=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (!global_action) {
	    sqlite3_finalize(req);
	    return BREAK_PROCESS;
	    }
	int * rec=recordset_new(rset, sizeof(int));
	*rec=(sqlite3_column_type(req, 0)==SQLITE_NULL)?-1:sqlite3_column_int(req, 0);
	result++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	internal_message(errmsg);
	result=-1;
	}
    sqlite3_finalize(req);
    return result;
    }

int localdb_wareBegin(void) {
    return sqlite_execCmdText(local_ware_db, "BEGIN");
    }

int localdb_wareCommit(void) {
    return sqlite_execCmdText(local_ware_db, "COMMIT");
    }

int localdb_wareRollback(void) {
    return sqlite_execCmdText(local_ware_db, "ROLLBACK");
    }
