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

#ifndef SQLITE_SYNC_H
#define SQLITE_SYNC_H

#include "database_common.h"

int localdb_openWareDB(char * dbname);
void localdb_closeWareDB(void);
int localdb_openMainDB(char * dbname);
void localdb_closeMainDB(void);
void localdb_closeAll(void);
/******************************************************************************/
int localdb_getMainTableStruct(struct recordset * rset, char * table);
int localdb_getWareTableStruct(struct recordset * rset, char * table);
int localdb_getVolumeTableStruct(struct recordset * rset, char * table);


int localdb_checkMainDB(void);
int localdb_createMainDB(void);
int localdb_createMainTables(void);
int localdb_checkWareDB(void);
int localdb_createWareDB(void);
int localdb_createWareTables(void);

/******************************** groups ************************************/
struct group_dbrecord {
    int group_id;
    char * name;	char _name[200];
    int parent_id;
    };
int localdb_findGroup(int id);
int localdb_insertGroup(struct group_dbrecord * rec);
int localdb_groupsCount(void);

/******************************** ware ************************************/
struct ware_dbrecord {
    int ware_id;
    int group_id;
    char * longtitle;	char _longtitle[512];
    char * shortcut;	char _shortcut[512];
    char * price;	char _price[16];
    };    
int localdb_findWare(int id);
int localdb_insertWare(struct ware_dbrecord * rec);
int localdb_wareCount(void);

/******************************* barcodes ***********************************/
struct barcode_dbrecord {
    int ware_id;
    char * barcode;	char _barcode[21];
    };
int localdb_findBarcode(int ware_id, char * bcode);
int localdb_updateBarcode(struct barcode_dbrecord * rec);
int localdb_barcodesCount(void);

/*****************************************************************************/
struct globalseq_dbrecord {
    int volumes_pk;
    int sessions_pk;
    int receipts_pk;
    int registrations_pk;
    int ware_quantity_pk;
    int payments_pk;
    int counter_seq;
    };
int localdb_getGlobalSeq(struct globalseq_dbrecord * rec);
int localdb_setVolumesSeq(int v);
int localdb_setSessionsSeq(int v);
int localdb_setZReportsSeq(int v);
int localdb_setReceiptsSeq(int v);
int localdb_setTotalsSeq(int v);
int localdb_setStatusSeq(int v);
int localdb_setRegistrationsSeq(int v);
int localdb_setQuantitySeq(int v);
int localdb_setStornoSeq(int v);
int localdb_setPaymentsSeq(int v);
int localdb_setCashSeq(int v);
int localdb_createGlobalSeq(struct globalseq_dbrecord * rec);

int localdb_getCounterSeq(void);

/****************************************************************************/
struct session_dbrecord {
    int session_id;
    char * date_time;	char _date_time[24];
    char * n_kkm;	char _n_kkm[21];
    int seller;
    int n_session;
    int n_doc;
    char * total_summ;	char _total_summ[16];
    int counter;
    };
int localdb_findSession(int id);
int localdb_insertSession(struct session_dbrecord * rec);
int localdb_updateSession(struct session_dbrecord * rec);

/****************************************************************************/
int localdb_openVolumeName(char * name);
void localdb_closeVolume(void);
struct volumes_dbrecord {
    int volume_id;
    char * volume_name;	char _volume_name[100];
    char * n_kkm;	char _n_kkm[21];
    int counter;
    };
int localdb_getVolume(struct volumes_dbrecord * rec);
int localdb_createVolume(void);

struct receipt_dbrecord {
    int receipt_id;
    int session_id;
    char * date_time;	char _date_time[24];
    char * n_kkm;	char _n_kkm[21];
    int seller;
    int n_session;
    int n_check;
    int n_doc;
    int check_type;
    int oper_type;
    int counter;
    };
int localdb_getReceiptsCount(int counter);
int localdb_getReceipts(struct recordset * rset, int counter);
int localdb_findReceipt(int id);
int localdb_insertReceipt(struct receipt_dbrecord * rec);
int localdb_updateReceipt(struct receipt_dbrecord * rec);
int localdb_deleteReceipt(int id);

struct registration_dbrecord {
    int registration_id;
    int receipt_id;
    char * n_kkm;	char _n_kkm[21];
    int n_session;
    int n_check;
    int n_doc;
    int check_type;
    int n_position;
    int ware_id;
    char * barcode;	char _barcode[21];
    char * price;	char _price[16];
    char * quantity;	char _quantity[16];
    char * summ;	char _summ[16];
    int counter;
    };
int localdb_getRegistrationsCount(int counter);
int localdb_getRegistrations(struct recordset * rset, int counter);
int localdb_findRegistration(int id);
int localdb_insertRegistration(struct registration_dbrecord * rec);
int localdb_updateRegistration(struct registration_dbrecord * rec);
int localdb_deleteRegistration(int id);

struct quantity_dbrecord {
    int quantity_id;
    int registration_id;
    int seller;
    int check_type;
    int oper_type;
    char * quantity;	char _quantity[16];
    char * summ;	char _summ[16];
    int counter;
    };
int localdb_getQuantityCount(int counter);
int localdb_getQuantity(struct recordset * rset, int counter);
int localdb_findQuantity(int id);
int localdb_insertQuantity(struct quantity_dbrecord * rec);
int localdb_updateQuantity(struct quantity_dbrecord * rec);
int localdb_deleteQuantity(int id);

struct payment_dbrecord {
    int payment_id;
    int receipt_id;
    int seller;
    int check_type;
    int payment_type;
    char * summ;	char _summ[16];
    int counter;
    };
int localdb_getPaymentsCount(int counter);
int localdb_getPayments(struct recordset * rset, int counter);
int localdb_findPayment(int id);
int localdb_insertPayment(struct payment_dbrecord * rec);
int localdb_updatePayment(struct payment_dbrecord * rec);
int localdb_deletePayment(int id);

#define BREAK_PROCESS	-10
#define DB_EMPTY	-20
#define SERVER_FAIL	-30
#define WAREDB_FAIL	-40
#define MAINDB_FAIL	-50
#define VOLUME_FAIL	-60

enum {
    NO_ACTION,
    SERVERDB_UPDATE,
    LOCALDB_INSERT,
    LOCALDB_UPDATE,
    LOCALDB_DELETE,
    };

int localdb_setVolumeName(int id, char * name);
int localdb_addVolume(struct volumes_dbrecord * rec);

int localdb_newMainDB(void);
int localdb_newWareDB(void);
int localdb_renameMainDB(void);
int localdb_renameWareDB(void);

int localdb_getVolumesCount(int counter);
int localdb_getVolumes(struct recordset * rset, int counter);

int localdb_wareBegin(void);
int localdb_wareCommit(void);
int localdb_wareRollback(void);

#endif //SQLITE_SYNC_H
