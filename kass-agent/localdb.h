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
#include "serverdb.h"

enum {
    OP_TYPE_NONE=0,
    OP_TYPE_CASH,
    OP_TYPE_REGISTRATION,
    OP_TYPE_PAYMENT,
    OP_TYPE_RECEIPT,
    OP_TYPE_SESSION,
    OP_TYPE_LOCK=0xFF
    };

struct volume_dbrecord {
    int volume_id;
    char * volume_name;	char _volume_name[100];
    char * n_kkm;	char _n_kkm[21];
    int counter;
    };

int localdb_openMain(void);
int localdb_openVolume(struct volume_dbrecord * rec);
int localdb_openVolumeName(char * name);
void localdb_closeMain(void);
void localdb_closeVolume(void);

struct ware_dbrecord {
    int ware_id;
    int group_id;
    char * longtitle;	char _longtitle[512];
    char * shortcut;	char _shortcut[512];
    char * price;	char _price[16];
    };    
int localdb_updateWare(struct ware_dbrecord * rec);

struct barcode_dbrecord {
    int ware_id;
    char * barcode;	char _barcode[21];
    };
int localdb_updateBarcode(struct barcode_dbrecord * rec);

int localdb_markBarcode(int code);
int localdb_markWareBarcodes(int code);
int localdb_markAll(void);
int localdb_clearMarked(void);
int localdb_clearAll(void);

/******************************************************************************/
int localdb_getVersionSequence(void);

int localdb_processSessions(struct recordset * rset, int first_id, int last_id);
int localdb_processReceipts(struct recordset * rset, int first_id, int last_id);
int localdb_processRegistrations(struct recordset * rset, int first_id, int last_id);
int localdb_processQuantity(struct recordset * rset, int first_id, int last_id);
int localdb_processPayments(struct recordset * rset, int first_id, int last_id);
int localdb_getVolumes(struct recordset * rset, int first_id, int last_id);

/*****************************************************************************/
int localdb_updateSession(struct session_dbrecord * rec);
int localdb_updateReceipt(struct receipt_dbrecord * rec);
int localdb_deleteReceipt(int id);
int localdb_updateRegistration(struct registration_dbrecord * rec);
int localdb_deleteRegistration(int id);
int localdb_updateQuantity(struct quantity_dbrecord * rec);
int localdb_deleteQuantity(int id);
int localdb_updatePayment(struct payment_dbrecord * rec);
int localdb_deletePayment(int id);

int localdb_wareBegin(void);
int localdb_wareCommit(void);
int localdb_wareRollback(void);

#endif //SQLITE_SYNC_H
