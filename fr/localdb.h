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

#ifndef LOCALDB_H
#define LOCALDB_H

#include "conf.h"
#include "../recordset.h"
#include "../database_common.h"

enum {
    OPER_TYPE_REGISTRATION=1,
    OPER_TYPE_STORNO,
    OPER_TYPE_CLOSE,
    OPER_TYPE_CANCEL
    };

void localdb_close(void);

struct ware_dbrecord {
    int ware_id;
    char longtitle[512];
    char shortcut[512];
    char price[16];
    };
int localdb_getWare(struct ware_dbrecord * rec);
int localdb_getWareByBarcode(struct recordset * rset, char * barcode);
/*****************************************************************************/
struct session_dbrecord {
    int session_id;
    char date_time[24];
    char n_kkm[21];
    int seller;
    int n_session;
    int n_doc;
    char total_summ[16];
    int counter;
    };
int localdb_addSession(struct session_dbrecord * rec);

struct receipt_dbrecord {
    int receipt_id;
    int session_id;
    char date_time[24];
    char n_kkm[21];
    int seller;
    int n_session;
    int n_check;
    int n_doc;
    int check_type;
    int oper_type;
    int counter;
    };
int localdb_addReceipt(struct receipt_dbrecord * rec);

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
int localdb_addRegistration(struct registration_dbrecord * rec);

struct quantity_dbrecord {
    int quantity_id;
    int registration_id;
    int seller;
    int check_type;
    int oper_type;
    char quantity[16];
    char summ[16];
    int counter;
    };
int localdb_addQuantity(struct quantity_dbrecord * rec);

struct payment_dbrecord {
    int payment_id;
    int receipt_id;
    int seller;
    int check_type;
    int payment_type;
    char summ[16];
    int counter;
    };
int localdb_addPayment(struct payment_dbrecord * rec);

/*****************************************************************************/
struct seq_dbrecord {
    int volumes_pk;
    int sessions_pk;
    int receipts_pk;
    int registrations_pk;
    int quantity_pk;
    int payments_pk;
    int counter_seq;
    };
    
int localdb_getSeq(struct seq_dbrecord * rec);
int localdb_saveSeq(struct seq_dbrecord * rec);

/*****************************************************************************/
int localdb_openMain(void);
void localdb_closeMain(void);
int localdb_openVolume(char * name);
int localdb_checkVolume(void);
void localdb_closeVolume(void);
int localdb_newVolume(int id, char * kkm);
int localdb_updateVolume(int id, int counter);

/*****************************************************************************/
int localdb_getReceiptRecords(struct recordset * rset, int receipt_id);

void localdb_setWareTimeout(int ms);
#endif //LOCALDB_H
