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

#ifndef SERVERDB_H
#define SERVERDB_H

void serverdb_init(void);
int serverdb_connect(void);
void serverdb_disconnect(void);
void serverdb_destroy(void);

/***************************************************************************/

struct serverSeq_dbrecord {
    int global_counter;
    int update_counter;
    int clear_counter;
    int full_clear_counter;
    };
int serverdb_getOperationSequence(struct serverSeq_dbrecord * rec);

int serverdb_updateWareOperID(int terminal_id, int ware_counter);

int serverdb_updateGroup(int first_id);
int serverdb_updateWare(int first_id, int fd);
int serverdb_updateBarcode(int first_id, int fd);

/***************************************************************************/

struct session_dbrecord {
    int session_id;
    char * date_time;	char _date_time[24];
    char * n_kkm; 	char _n_kkm[21];
    int seller;
    int n_session;
    int n_doc;
    char * total_summ;	char _total_summ[16];
    int counter;
    };
int serverdb_addSession(int terminal_id, struct session_dbrecord *rec);

struct receipt_dbrecord {
    int receipt_id;
    int session_id;
    char * date_time; 	char _date_time[24];
    char * n_kkm; 	char _n_kkm[21];
    int seller;
    int n_session;
    int n_check;
    int n_doc;
    int check_type;
    int oper_type;
    int counter;
    };
int serverdb_addReceipt(int terminal_id, int volume_id, struct receipt_dbrecord * rec);

struct registration_dbrecord {
    int registration_id;
    int receipt_id;
    char * n_kkm; 	char _n_kkm[21];
    int n_session;
    int n_check;
    int n_doc;
    int check_type;
    int n_position;
    int ware_id;
    char * barcode; 	char _barcode[21];
    char * price;	char _price[16];
    int counter;
    };
int serverdb_addRegistration(int terminal_id, int volume_id, struct registration_dbrecord * rec);

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
int serverdb_addQuantity(int terminal_id, int volume_id, struct quantity_dbrecord * rec);

struct payment_dbrecord {
    int payment_id;
    int receipt_id;
    int seller;
    int check_type;
    int payment_type;
    char * summ;	char _summ[16];
    int counter;
    };
int serverdb_addPayment(int terminal_id, int volume_id, struct payment_dbrecord * rec);

int serverdb_getTerminalID(char * name);
struct terminals_dbrecord {
    int terminal_id;
    char * name;	char _name[41];
    int ware_counter;
    int session_counter;
    };
int serverdb_getTerminalValues(struct terminals_dbrecord * rec);
#endif //SERVERDB_H
