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

extern int * global_stop_flag;

static void internal_message(char *text) {
    log_message(0, "ОШИБКА SQLITE: %s", text);
    }

static sqlite3 * local_ware_db=NULL;
static sqlite3 * local_main_db=NULL;
static sqlite3 * local_volume_db=NULL;

int localdb_openMain(void) {
#ifdef DEBUG
    if (local_ware_db!=NULL || local_main_db!=NULL) {
	fputs("*** localdb_open ASSERT: database already open", stderr);
	return 0;
	}
#endif
    char dbname[1024];
    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/ware.ldb");
    int r=sqlite3_open(dbname, &local_ware_db);
    if (r!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(local_ware_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(local_ware_db, 30000);

    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/main.ldb");
    r=sqlite3_open(dbname, &local_main_db);
    if (r!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(local_main_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(local_main_db, 30000);

    return 0;
    }

void localdb_closeVolume(void) {
    if (local_volume_db!=NULL) {
	sqlite3_close(local_volume_db);
	local_volume_db=NULL;
	}
    }

void localdb_closeMain(void) {
    if (local_ware_db!=NULL) {
	sqlite3_close(local_ware_db);
	local_ware_db=NULL;
	}
    if (local_main_db!=NULL) {
	sqlite3_close(local_main_db);
	local_main_db=NULL;
	}
    }

/****************************************************************************/
int localdb_updateWare(struct ware_dbrecord * rec) {
    int r=sqlite_execCmd(local_ware_db,
	AS_TEXT("UPDATE ware SET group_id="), AS_UINT(rec->group_id),
		AS_TEXT(", longtitle="), AS_STRING(rec->longtitle),
		AS_TEXT(", shortcut="), AS_STRING(rec->shortcut),
		AS_TEXT(", price="), AS_TEXT(rec->price),
		AS_TEXT(", erase='f' WHERE ware_id="), AS_UINT(rec->ware_id),
	END_ARGS);
    if (r<0) return r;
    if (r>0) return 0;
    return sqlite_execCmd(local_ware_db,
	AS_TEXT("INSERT INTO ware(ware_id, group_id, longtitle,"
		" shortcut, price) VALUES ("),
	    AS_UINT(rec->ware_id), AS_TEXT(", "),
	    AS_UINT(rec->group_id), AS_TEXT(", "),
	    AS_STRING(rec->longtitle), AS_TEXT(", "),
	    AS_STRING(rec->shortcut), AS_TEXT(", "),
	    AS_TEXT(rec->price), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_updateBarcode(struct barcode_dbrecord * rec) {
    int r=sqlite_execCmd(local_ware_db,
	AS_TEXT("UPDATE barcodes SET erase='f' "
		"WHERE ware_id="), AS_UINT(rec->ware_id),
	    AS_TEXT(" AND barcode="), AS_STRING(rec->barcode),
	END_ARGS);
    if (r<0) return -1;
    if (r>0) return 0;
    return sqlite_execCmd(local_ware_db,
	AS_TEXT("INSERT INTO barcodes(ware_id, barcode) VALUES ("),
		AS_UINT(rec->ware_id), AS_TEXT(", "),
		AS_STRING(rec->barcode), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_markBarcode(int id) {
    return sqlite_execCmd(local_ware_db,
	AS_TEXT("UPDATE barcodes SET erase='t' WHERE "
		    "internal_id="), AS_UINT(id),
	END_ARGS);
    }

int localdb_markWareBarcodes(int code) {
    return sqlite_execCmd(local_ware_db,
	AS_TEXT("UPDATE barcodes SET erase='t' WHERE "
		    "ware_id="), AS_UINT(code),
	END_ARGS);
    }

int localdb_markAll(void) {
    int r=sqlite_execCmdText(local_ware_db,
	"UPDATE barcodes SET erase='t'");
    if (r>=0) r=sqlite_execCmdText(local_ware_db,
	"UPDATE ware SET erase='t'");
    if (r>=0) return 0;
    return -1;
    }

int localdb_clearMarked(void) {
    int r=sqlite_execCmdText(local_ware_db,
	"DELETE FROM barcodes WHERE erase='t'");
    if (r>=0) r=sqlite_execCmdText(local_ware_db, 
	"DELETE FROM ware WHERE erase='t'");
    if (r>=0) return 0;
    return -1;
    }

int localdb_clearAll(void) {
puts("* localdb_clearAll()");
    int r=sqlite_execCmdText(local_ware_db, 
	"DELETE FROM barcodes");
    if (r>=0) r=sqlite_execCmdText(local_ware_db,
	"DELETE FROM ware");
    if (r>=0) return 0;
    return -1;
    }

/****************************************************************************/
int localdb_getCounter(void) {
    sqlite3_stmt * req=sqlite_prepareText(local_main_db,
	"SELECT MAX(counter_seq) FROM _global_seq");
    if (req==NULL) {
	return -1;
	}
    int v=-1;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_main_db);
	internal_message(errmsg);
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_processSessions(struct recordset * rset, int first_id, int last_id) {
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
		    " n_session, n_doc, total_summ, counter"
		" FROM sessions WHERE counter BETWEEN "),
		AS_UINT(first_id), AS_TEXT(" AND "), AS_UINT(last_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (*global_stop_flag) {
	    sqlite3_finalize(req);
	    return -1;
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

int localdb_processReceipts(struct recordset * rset, int first_id, int last_id) {
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
		    " oper_type, counter"
		" FROM receipts WHERE counter BETWEEN "),
		AS_UINT(first_id), AS_TEXT(" AND "), AS_UINT(last_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (*global_stop_flag) {
	    sqlite3_finalize(req);
	    return -1;
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
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_processRegistrations(struct recordset * rset, int first_id, int last_id) {
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
		" FROM registrations WHERE counter BETWEEN "),
		AS_UINT(first_id), AS_TEXT(" AND "), AS_UINT(last_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (*global_stop_flag) {
	    sqlite3_finalize(req);
	    return -1;
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
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_processQuantity(struct recordset * rset, int first_id, int last_id) {
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
		" FROM ware_quantity WHERE counter BETWEEN "),
		AS_UINT(first_id), AS_TEXT(" AND "), AS_UINT(last_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (*global_stop_flag) {
	    sqlite3_finalize(req);
	    return -1;
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
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_processPayments(struct recordset * rset, int first_id, int last_id) {
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
		    " payment_type, summ, counter"
		" FROM payments WHERE counter BETWEEN "),
		AS_UINT(first_id), AS_TEXT(" AND "), AS_UINT(last_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	if (*global_stop_flag) {
	    sqlite3_finalize(req);
	    return -1;
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
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_openVolumeName(char * name) {
#ifdef DEBUG
    if (local_volume_db!=NULL) {
	fputs("*** localdb_openVolume ASSERT: database already open\n",stderr);
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

int localdb_getVolumes(struct recordset * rset, int first_id, int last_id) {
    enum {
	VOLUME_ID_COLUMN=0,
	VOLUME_NAME_COLUMN,
	KKM_NUM_COLUMN,
	COUNTER_COLUMN
	};

    sqlite3_stmt * req=sqlite_prepare(local_main_db,
	AS_TEXT("SELECT volume_id, volume_name, n_kkm, counter"
		" FROM volumes WHERE counter BETWEEN "),
		AS_UINT(first_id), AS_TEXT(" AND "), AS_UINT(last_id),
	END_ARGS);
    if (req==NULL) return -1;
    int result=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	struct volume_dbrecord * rec=recordset_new(rset, sizeof(struct volume_dbrecord));
	rec->volume_id=(sqlite3_column_type(req, VOLUME_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, VOLUME_ID_COLUMN);
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
	result++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(local_main_db);
	internal_message(errmsg);
	result=-1;
	}
    sqlite3_finalize(req);
    return result;
    }
/*****************************************************************************/
static int DeleteRecord(sqlite3 * db, char * table, char * column, int v) {
    return sqlite_execCmd(db,
	AS_TEXT("DELETE FROM "), AS_TEXT(table),
	AS_TEXT(" WHERE "), AS_TEXT(column), AS_TEXT("="), AS_UINT(v),
	END_ARGS);
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

int localdb_deleteReceipt(int id) {
    return DeleteRecord(local_volume_db, "receipts", "receipt_id", id);
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

int localdb_deleteRegistration(int id) {
    return DeleteRecord(local_volume_db, "registrations", "registration_id", id);
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

int localdb_deleteQuantity(int id) {
    return DeleteRecord(local_volume_db, "ware_quantity", "quantity_id", id);
    }

int localdb_updatePayment(struct payment_dbrecord * rec) {
    return sqlite_execCmd(local_volume_db,
	AS_TEXT("UPDATE payments SET receipt_id="), AS_UINT(rec->receipt_id),
	    AS_TEXT(", seller="), AS_UINT(rec->seller), 
	    AS_TEXT(", check_type="), AS_UINT(rec->check_type), 
	    AS_TEXT(", payment_type="), AS_UINT(rec->payment_type), 
	    AS_TEXT(", summ="), AS_TEXT(rec->summ), 
	    AS_TEXT(", counter="), AS_UINT(rec->counter), 
	    AS_TEXT(" WHERE payment_id="), AS_UINT(rec->payment_id),
	    AS_TEXT(" AND counter<"), AS_UINT(rec->counter),
	END_ARGS);
    }

int localdb_deletePayment(int id) {
    return DeleteRecord(local_volume_db, "payments", "payment_id", id);
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
