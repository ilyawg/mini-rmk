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


#ifndef ACTIONS_C
#define ACTIONS_C

#include "receipt.h"

//extern struct tm oper_date_time;
//extern int oper_check_num;
//extern int oper_doc_num;
//extern int oper_check_type;
//extern int oper_payment_type;
//extern struct record_receipt * oper_rc;

/*******************************************/
//extern num_t oper_summ;
//extern int oper_code;
/*******************************************/

int actions_processRegistration(struct record_receipt * rc, int n_session, int n_check, int n_doc, int check_type);
int actions_processStorno(int registration_id, num_t * quantity, num_t * summ, int check_type);
int actions_processCancel(struct tm * ts, int n_session, int n_check, int n_doc, int check_type);
int actions_processReceipt(struct tm * ts, int n_session, int n_check, int n_doc, int check_type, num_t * summ);
int actions_processSession(struct tm * ts, int n_session, int n_doc, num_t * summ);

int actions_processConnect(char * n_kkm);
void actions_disconnect(void);

#endif //ACTIONS_C
