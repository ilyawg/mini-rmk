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
#include <time.h>

#include <sqlite3.h>

#include "vararg.h"
#include "conf.h"
#include "database_common.h"
#include "sqlite_common.h"
#include "localdb.h"
#include "numeric.h"

#define DEBUG

#define SQLITE_BUSY_TIMEOUT	60000

static void internal_message(char *text) {
#ifdef DEBUG
    fprintf(stderr, "ОШИБКА SQLITE: %s", text);
#endif
    log_printf(0, "ОШИБКА SQLITE: %s", text);
    }

static sqlite3 * internal_ware_db=NULL;
static sqlite3 * internal_main_db=NULL;
static sqlite3 * internal_volume_db=NULL;

void localdb_closeVolume(void) {
    if (internal_volume_db!=NULL) {
	sqlite3_close(internal_volume_db);
	internal_volume_db=NULL;
	}
    }

void localdb_closeMain(void) {
    if (internal_ware_db!=NULL) {
	sqlite3_close(internal_ware_db);
	internal_ware_db=NULL;
	}
    if (internal_main_db!=NULL) {
	sqlite3_close(internal_main_db);
	internal_main_db=NULL;
	}
    }

static int localdb_createTables(void) {
    if (sqlite_execCmdText(internal_volume_db,
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

    if (sqlite_execCmdText(internal_volume_db,
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

    if (sqlite_execCmdText(internal_volume_db,
	"CREATE TABLE ware_quantity("
	    "quantity_id INTEGER PRIMARY KEY, "
	    "registration_id integer, "
	    "seller integer, "
	    "check_type integer, "
	    "oper_type integer, "
	    "quantity numeric(10,3), "
	    "summ numeric(10,2), "
	    "counter integer, "
	    "CONSTRAINT registration_fk FOREIGN KEY (registration_id) "
		"REFERENCES registrations ON DELETE CASCADE)")<0) return -1;

    if (sqlite_execCmdText(internal_volume_db,
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

static int localdb_addVolume(char * kkm, int id, char * name) {
    return sqlite_execCmd(internal_main_db,
	AS_TEXT ("INSERT INTO volumes(volume_id, volume_name, n_kkm) VALUES ("),
	    AS_UINT(id), AS_TEXT(", "),
	    AS_STRING(name), AS_TEXT(", "),
	    AS_STRING(kkm), AS_TEXT(")"),
	END_ARGS);
    }

int localdb_createVolume(char * kkm, int id, char * name) {
    if (sqlite_execCmdText(internal_volume_db, "BEGIN")<0) 
	return -1;
    if (localdb_createTables()<0) {
	sqlite_execCmdText(internal_volume_db, "ROLLBACK");
	return -1;
	}
    if (sqlite_execCmdText(internal_volume_db, "COMMIT")<0)
	return -1;
    return localdb_addVolume(kkm, id, name);
    }

/****************************************************************************/
static int GetTableStruct(struct recordset * rset, char * table, sqlite3 * db) {
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
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	char name[255];
	char type[64];
	sqlite_getText(name, req, NAME_COLUMN);
	sqlite_getText(type, req, TYPE_COLUMN);
	strcat(name, " ");
	strcat(name, type);
	recordset_add(rset, name, strlen(name)+1);
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

static int CheckWareTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "ware", internal_ware_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char *s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "ware_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "longtitle varchar(255)")) result|=0x0002;
	else if (!strcmp(s, "shortcut varchar(255)")) result|=0x0004;
	else if (!strcmp(s, "price numeric(10,2)")) result|=0x0008;
	else if (!strcmp(s, "group_id integer")) result|=0x0010;
	else if (!strcmp(s, "erase boolean")) result|=0x0020;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x003F) return -1;
    return 0;
    }

static int CheckBarcodesTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "barcodes", internal_ware_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char *s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "internal_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "ware_id integer")) result|=0x0002;
	else if (!strcmp(s, "barcode varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "erase boolean")) result|=0x0008;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x000F) return -1;
    return 0;
    }

static int CheckGlobalSeq(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "_global_seq", internal_main_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "sessions_pk integer")) result|=0x0001;
	else if (!strcmp(s, "volumes_pk integer")) result|=0x0002;
	else if (!strcmp(s, "receipts_pk integer")) result|=0x0004;
	else if (!strcmp(s, "registrations_pk integer")) result|=0x0008;
	else if (!strcmp(s, "ware_quantity_pk integer")) result|=0x0010;
    	else if (!strcmp(s, "payments_pk integer")) result|=0x0020;
	else if (!strcmp(s, "counter_seq integer")) result|=0x0040;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x007F) return -1;
    return 0;
    }

static int CheckSessionsTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "sessions", internal_main_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "session_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "date_time timestamp")) result|=0x0002;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "seller integer")) result|=0x0008;
	else if (!strcmp(s, "n_session integer")) result|=0x0010;
	else if (!strcmp(s, "n_doc integer")) result|=0x0020;
	else if (!strcmp(s, "total_summ numeric(15,2)")) result|=0x0040;
    	else if (!strcmp(s, "counter integer")) result|=0x0080;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x00FF) return -1;
    return 0;
    }

static int CheckVolumesTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "volumes", internal_main_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "volume_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "volume_name varchar(100)")) result|=0x0002;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "counter integer")) result|=0x0008;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x000F) return -1;
    return 0;
    }

int localdb_openMain(void) {
#ifdef DEBUG
    if (internal_ware_db!=NULL || internal_main_db!=NULL) {
	fputs("*** localdb_open ASSERT: database already open\n",stderr);
	return 0;
	}
#endif //DEBUG

    char path[1024];
    strcpy(path, global_conf_db_dir);
    strcat(path, "/");
    char *name=strchr(path, 0);
    strcpy(name, "ware.ldb");
    int r=sqlite3_open(path, &internal_ware_db);
    if (r!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(internal_ware_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(internal_ware_db, SQLITE_BUSY_TIMEOUT);
    if (CheckWareTable()<0 || CheckBarcodesTable()<0) {
	sqlite3_close(internal_ware_db);
	internal_ware_db=NULL;
	return -1;
	}

    strcpy(name, "main.ldb");
    r=sqlite3_open(path, &internal_main_db);
    if (r!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(internal_main_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(internal_main_db, SQLITE_BUSY_TIMEOUT);
    if (CheckGlobalSeq()==0 && CheckSessionsTable()==0) return 0;
    sqlite3_close(internal_ware_db); internal_ware_db=NULL;
    sqlite3_close(internal_main_db); internal_main_db=NULL;
    return -1;
    }

/****************************************************************************/
static int CheckRegistrationsTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "registrations", internal_volume_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "registration_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "receipt_id integer")) result|=0x0002;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "n_session integer")) result|=0x0008;
	else if (!strcmp(s, "n_check integer")) result|=0x0010;
	else if (!strcmp(s, "n_doc integer")) result|=0x0020;
	else if (!strcmp(s, "check_type integer")) result|=0x0040;
	else if (!strcmp(s, "n_position integer")) result|=0x0080;
	else if (!strcmp(s, "ware_id integer")) result|=0x0100;
	else if (!strcmp(s, "barcode varchar(20)")) result|=0x0200;
	else if (!strcmp(s, "price numeric(10,2)")) result|=0x0400;
	else if (!strcmp(s, "counter integer")) result|=0x0800;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x0FFF) return -1;
    return 0;
    }

static int CheckQuantityTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "ware_quantity", internal_volume_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "quantity_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "registration_id integer")) result|=0x0002;
	else if (!strcmp(s, "seller integer")) result|=0x0004;
	else if (!strcmp(s, "check_type integer")) result|=0x0008;
	else if (!strcmp(s, "oper_type integer")) result|=0x0010;
	else if (!strcmp(s, "quantity numeric(10,3)")) result|=0x0020;
	else if (!strcmp(s, "summ numeric(10,2)")) result|=0x0040;
	else if (!strcmp(s, "counter integer")) result|=0x0080;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x00FF) return -1;
    return 0;
    }

static int CheckPaymentsTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "payments", internal_volume_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "payment_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "receipt_id integer")) result|=0x0002;
	else if (!strcmp(s, "seller integer")) result|=0x0004;
	else if (!strcmp(s, "check_type integer")) result|=0x0008;
	else if (!strcmp(s, "payment_type integer")) result|=0x0010;
	else if (!strcmp(s, "summ numeric(10,2)")) result|=0x0020;
	else if (!strcmp(s, "counter integer")) result|=0x0040;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x007F) return -1;
    return 0;
    }

static int CheckReceiptsTable(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (GetTableStruct(&rset, "receipts", internal_volume_db)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "receipt_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "session_id integer")) result|=0x0002;
	else if (!strcmp(s, "date_time timestamp")) result|=0x0004;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0008;
	else if (!strcmp(s, "seller integer")) result|=0x0010;
	else if (!strcmp(s, "n_session integer")) result|=0x0020;
	else if (!strcmp(s, "n_check integer")) result|=0x0040;
	else if (!strcmp(s, "n_doc integer")) result|=0x0080;
	else if (!strcmp(s, "check_type integer")) result|=0x0100;
	else if (!strcmp(s, "oper_type integer")) result|=0x0200;
	else if (!strcmp(s, "counter integer")) result|=0x0400;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x07FF) return -1;
    return 0;
    }

int localdb_openVolume(char * name) {
#ifdef DEBUG
printf("* openVolumeName() <%s>\n",name);
    if (internal_volume_db!=NULL) {
	fputs("*** localdb_openVolumeName ASSERT: database already open\n",stderr);
	return 0;
	}
#endif
    if (sqlite3_open(name, &internal_volume_db)!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    sqlite3_busy_timeout(internal_volume_db, SQLITE_BUSY_TIMEOUT);
    return 0;
    }
    
int localdb_checkVolume(void) {
    if (CheckRegistrationsTable()==0
	&& CheckQuantityTable()==0
	&& CheckPaymentsTable()==0
	&& CheckReceiptsTable()==0) return 0;
    sqlite3_close(internal_volume_db);
    internal_volume_db=NULL;
    return -1;
    }

/****************************************************************************/
int localdb_getVolume(int session_id) {
    sqlite3_stmt * req=sqlite_prepare(internal_main_db,
	AS_TEXT("SELECT volume_id FROM sessions "
		"WHERE session_id="), AS_UINT(session_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_main_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_updateCounter(int counter) {
    return sqlite_execCmd(internal_main_db,
	AS_TEXT("UPDATE _global_seq SET counter_seq="),
	    AS_UINT(counter),
	END_ARGS);
    }

int localdb_updateVolume(int id, int counter) {
    return sqlite_execCmd(internal_main_db,
	AS_TEXT("UPDATE volumes SET counter="), AS_UINT(counter),
	    AS_TEXT(" WHERE volume_id="), AS_UINT(id),
	END_ARGS);
    }

/****************************************************************************/
int localdb_getWare(struct ware_dbrecord * rec) {
    enum {
	LONGTITLE_COLUMN=0,
	SHORTCUT_COLUMN,
	PRICE_COLUMN,
	};
    sqlite3_stmt * req=sqlite_prepare(internal_ware_db,
	AS_TEXT("SELECT longtitle, shortcut, price FROM ware "
		"WHERE ware_id="), AS_UINT(rec->ware_id), END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	sqlite_getText(rec->longtitle, req, LONGTITLE_COLUMN);
	sqlite_getText(rec->shortcut, req, SHORTCUT_COLUMN);
	sqlite_getNum(rec->price, req, PRICE_COLUMN);
	v=1;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_ware_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_getWareByBarcode(struct recordset * rset, char * barcode) {
    enum {
	WARE_ID_COLUMN=0,
	LONGTITLE_COLUMN,
	SHORTCUT_COLUMN,
	PRICE_COLUMN,
	};
    sqlite3_stmt * req=sqlite_prepare(internal_ware_db,
	AS_TEXT("SELECT ware_id, longtitle, shortcut, price"
	    " FROM ware WHERE ware_id IN"
		" (SELECT ware_id FROM barcodes"
		" WHERE barcode="), AS_STRING(barcode), AS_TEXT(")"),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    while (r==SQLITE_ROW) {
	struct ware_dbrecord * rec=recordset_new(rset, sizeof(struct ware_dbrecord));
	rec->ware_id=sqlite3_column_int(req, WARE_ID_COLUMN);
	sqlite_getText(rec->longtitle, req, LONGTITLE_COLUMN);
	sqlite_getText(rec->shortcut, req, SHORTCUT_COLUMN);
	sqlite_getNum(rec->price, req, PRICE_COLUMN);
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_ware_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

/****************************************************************************/
int localdb_addRegistration(struct registration_dbrecord * rec) {
    return sqlite_execCmd(internal_volume_db,
	AS_TEXT("INSERT INTO registrations(registration_id, receipt_id,"
		" n_kkm, n_session, n_check, n_doc, check_type, n_position,"
		" ware_id, barcode, price, counter) VALUES ( "),
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

int localdb_addQuantity(struct quantity_dbrecord * rec) {
    return sqlite_execCmd(internal_volume_db,
	AS_TEXT("INSERT INTO ware_quantity(quantity_id,"
		" registration_id, seller, check_type, oper_type,"
		" quantity, summ, counter) VALUES ("),
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

int localdb_addPayment(struct payment_dbrecord * rec) {
    return sqlite_execCmd(internal_volume_db,
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

int localdb_addReceipt(struct receipt_dbrecord * rec) {
    return sqlite_execCmd(internal_volume_db,
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

int localdb_addSession(struct session_dbrecord * rec) {
    return sqlite_execCmd(internal_main_db,
	AS_TEXT("INSERT INTO sessions(session_id, date_time, n_kkm,"
		" seller, n_session, n_doc, total_summ, counter) VALUES ("),
	    AS_UINT(rec->session_id), AS_TEXT(", "),
	    AS_STRING(rec->date_time), AS_TEXT(", "),
	    AS_STRING(rec->n_kkm), AS_TEXT(", "),
	    AS_UINT(rec->seller), AS_TEXT(", "),
	    AS_UINT(rec->n_session), AS_TEXT(", "),
	    AS_UINT(rec->n_doc), AS_TEXT(", "),
	    AS_TEXT(rec->total_summ), AS_TEXT(", "),
	    AS_UINT(rec->counter), AS_TEXT(")"),
	END_ARGS);
    }

/****************************** restore volume ******************************/
int localdb_getVolumeID(char * kkm) {
    sqlite3_stmt * req=sqlite_prepare(internal_main_db,
	AS_TEXT("SELECT MAX(volume_id) FROM volumes"
	    " WHERE n_kkm="), AS_STRING(kkm),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req, 0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_main_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_getVolumeName(char * name, int id) {
    sqlite3_stmt * req=sqlite_prepare(internal_main_db,
	AS_TEXT("SELECT volume_name FROM volumes "
		"WHERE volume_id="), AS_UINT(id), 
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	strcpy(name, sqlite3_column_text(req,0));
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_main_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_volumeSize(void) {
    sqlite3_stmt * req=sqlite_prepareText(internal_volume_db,
	"SELECT COUNT(*) FROM receipts");
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req, 0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

/****************************** restore session ******************************/
int localdb_getLastSession(char * kkm) {
    sqlite3_stmt * req=sqlite_prepare(internal_volume_db,
	AS_TEXT("SELECT MAX(session_id) FROM receipts"
	    " WHERE n_kkm="), AS_STRING(kkm),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_getSessionStatus(int id) {
    sqlite3_stmt * req=sqlite_prepare(internal_main_db,
	AS_TEXT("SELECT COUNT(*) FROM sessions WHERE session_id="), AS_UINT(id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_main_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_getSessionNum(int id) {
    sqlite3_stmt * req=sqlite_prepare(internal_volume_db,
	AS_TEXT("SELECT MAX(n_session) FROM receipts WHERE session_id="), AS_UINT(id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

/****************************** restore receipt ******************************/
int localdb_getLastReceipt(char * kkm) {
    sqlite3_stmt * req=sqlite_prepare(internal_volume_db,
	AS_TEXT("SELECT MAX(receipt_id) FROM registrations"
	    " WHERE n_kkm="), AS_STRING(kkm),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_getReceiptStatus(int id) {
    sqlite3_stmt * req=sqlite_prepare(internal_volume_db,
	AS_TEXT("SELECT COUNT(*) FROM receipts WHERE receipt_id="), AS_UINT(id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_getDocNum(int id) {
    sqlite3_stmt * req=sqlite_prepare(internal_volume_db,
	AS_TEXT("SELECT MAX(n_doc) FROM registrations WHERE receipt_id="), AS_UINT(id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	v=sqlite3_column_int(req,0);
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

/****************************************************************************/
int localdb_getReceiptRecords(struct recordset * rset, int receipt_id) {
    enum {
	REGISTRATION_ID_COLUMN=0,
	KKM_NUM_COLUMN,
	SESSION_NUM_COLUMN,
	CHECK_NUM_COLUMN,
	DOC_NUM_COLUMN,
	CHECK_TYPE_COLUMN,
	POSITION_NUM_COLUMN,
	WARE_ID_COLUMN,
	BARCODE_COLUMN,
	PRICE_COLUMN,
	QUANTITY_COLUMN,
	SUMM_COLUMN
	};
    sqlite3_stmt * req=sqlite_prepare(internal_volume_db,
	AS_TEXT("SELECT registrations.registration_id,"
		      " registrations.n_kkm,"
		      " registrations.n_session,"
		      " registrations.n_check,"
		      " registrations.n_doc,"
		      " registrations.check_type,"
		      " registrations.n_position,"
		      " registrations.ware_id,"
		      " registrations.barcode,"
		      " registrations.price,"
		      " ware_quantity.quantity,"
		      " ware_quantity.summ"
	    " FROM registrations JOIN"
		" (SELECT registration_id, SUM(quantity) AS quantity,"
		" SUM(summ) AS summ FROM ware_quantity"
		" GROUP BY registration_id) AS ware_quantity"
	    " ON registrations.registration_id=ware_quantity.registration_id"
	    " WHERE registrations.receipt_id="), AS_UINT(receipt_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r;
    while (r=sqlite3_step(req), r==SQLITE_ROW) {
	struct registration_dbrecord * rec=recordset_new(rset, sizeof(struct registration_dbrecord));
	rec->registration_id=(sqlite3_column_type(req, REGISTRATION_ID_COLUMN)==SQLITE_NULL)?-1:sqlite3_column_int(req, REGISTRATION_ID_COLUMN);
	rec->receipt_id=receipt_id;
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
	v++;
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

int localdb_getPaymentSum(char * sum, int receipt_id) {
    sqlite3_stmt * req=sqlite_prepare(internal_volume_db,
	AS_TEXT("SELECT SUM(summ) FROM payments"
	    " WHERE receipt_id="), AS_UINT(receipt_id),
	END_ARGS);
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	sqlite_getNum(sum, req, 0);
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_volume_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

/*****************************************************************************/
int localdb_saveSeq(struct seq_dbrecord * rec) {
    return sqlite_execCmd(internal_main_db,
	AS_TEXT("UPDATE _global_seq SET volumes_pk="), AS_UINT(rec->volumes_pk),
	    AS_TEXT(", receipts_pk="), AS_UINT(rec->receipts_pk),
	    AS_TEXT(", registrations_pk="), AS_UINT(rec->registrations_pk),
	    AS_TEXT(", ware_quantity_pk="), AS_UINT(rec->quantity_pk),
	    AS_TEXT(", payments_pk="), AS_UINT(rec->payments_pk),
	    AS_TEXT(", sessions_pk="), AS_UINT(rec->sessions_pk),
	    AS_TEXT(", counter_seq="), AS_UINT(rec->counter_seq),
	END_ARGS);
    }

int localdb_getSeq(struct seq_dbrecord * rec) {
    enum {
	VOLUMES_PK_COLUMN=0,
	SESSIONS_PK_COLUMN,
	RECEIPTS_PK_COLUMN,
	REGISTRATIONS_PK_COLUMN,
	WARE_QUANTITY_PK_COLUMN,
	PAYMENTS_PK_COLUMN,
	COUNTER_SEQ_COLUMN
	};
    sqlite3_stmt * req=sqlite_prepareText(internal_main_db,
	"SELECT volumes_pk, sessions_pk, receipts_pk, registrations_pk,"
	    " ware_quantity_pk, payments_pk, counter_seq FROM _global_seq");
    if (req==NULL) return -1;
    int v=0;
    int r=sqlite3_step(req);
    if (r==SQLITE_ROW) {
	rec->volumes_pk=sqlite3_column_int(req,VOLUMES_PK_COLUMN);
	rec->sessions_pk=sqlite3_column_int(req,SESSIONS_PK_COLUMN);
	rec->receipts_pk=sqlite3_column_int(req,RECEIPTS_PK_COLUMN);
	rec->registrations_pk=sqlite3_column_int(req,REGISTRATIONS_PK_COLUMN);
	rec->quantity_pk=sqlite3_column_int(req,WARE_QUANTITY_PK_COLUMN);
	rec->payments_pk=sqlite3_column_int(req,PAYMENTS_PK_COLUMN);
	rec->counter_seq=sqlite3_column_int(req, COUNTER_SEQ_COLUMN);
	v++;
	r=sqlite3_step(req);
	}
    if (r!=SQLITE_DONE) {
	char * errmsg=(char*)sqlite3_errmsg(internal_main_db);
	internal_message(errmsg);
	v=-1;
	}
    sqlite3_finalize(req);
    return v;
    }

void localdb_setWareTimeout(int ms) {
puts("* localdb_setWareTimeout()");
    sqlite3_busy_timeout(internal_ware_db, ms);
    }
