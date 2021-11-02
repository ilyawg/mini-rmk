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

#include "localdb.h"

void serverdb_init(void);
int serverdb_connect(void);
void serverdb_disconnect(void);
void serverdb_destroy(void);

void serverdb_doConnect(void);
/***************************************************************************/
//int serverdb_processSessions(void);
int serverdb_getTerminalID(char * name);
int serverdb_getWareCounter(int terminal_id);

int serverdb_groupsCount(int last_id);
int serverdb_processGroups(int last_id);
int serverdb_wareCount(int last_id);
int serverdb_processWare(int last_id);
int serverdb_barcodesCount(int last_id);
int serverdb_processBarcodes(int last_id);

/****************************************************************************/
int serverdb_getVolumesSeq(int terminal_id, int counter);
int serverdb_getSessionsSeq(int terminal_id, int counter);
int serverdb_getReceiptsSeq(int terminal_id, int counter);
int serverdb_getRegistrationsSeq(int terminal_id, int counter);
int serverdb_getQuantitySeq(int terminal_id, int counter);
int serverdb_getPaymentsSeq(int terminal_id, int counter);

/*****************************************************************************/
int serverdb_addSession(int terminal_id, struct session_dbrecord * rec);
int serverdb_processSessions(int terminal_id, int counter);
int serverdb_getSessionsCount(int terminal_id, int counter);
int serverdb_getSessionCounter(int terminal_id);

int serverdb_getReceiptsCount(int terminal_id, int volume_id, int counter);
int serverdb_getTotalsCount(int terminal_id, int volume_id, int counter);
int serverdb_getReceiptStatusCount(int terminal_id, int volume_id, int counter);
int serverdb_getRegistrationsCount(int terminal_id, int volume_id, int counter);
int serverdb_getQuantityCount(int terminal_id, int volume_id, int counter);
int serverdb_getStornoCount(int terminal_id, int volume_id, int counter);
int serverdb_getPaymentsCount(int terminal_id, int volume_id, int counter);

int serverdb_processReceipts(int terminal_id, int volume_id, int max_counter);

int serverdb_addReceipt(int terminal_id, int volume_id, struct receipt_dbrecord * rec);
int serverdb_addRegistration(int terminal_id, int volume_id, struct registration_dbrecord * rec);
int serverdb_addQuantity(int terminal_id, int volume_id, struct quantity_dbrecord * rec);
int serverdb_addPayment(int terminal_id, int volume_id, struct payment_dbrecord * rec);

int serverdb_getVolumes(struct recordset * rset, int terminal_id, int counter);
int serverdb_getVolumeRecord(struct volumes_dbrecord * rec, int terminal_id, int counter);

int serverdb_updateSessionOperID(int terminal_id, int counter);
int serverdb_getVolumesCount(int terminal_id, int counter);

#endif //SERVERDB_H
