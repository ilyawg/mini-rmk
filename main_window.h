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

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "receipt.h"
#include "numeric.h"

int mw_init(void);
void mw_showSeller(const char *str);
void mw_showSession(int state, int num);

void mw_showDocState(int doc_state, int doc_num);
void mw_showText(char *text);
void mw_clearText(void);
void mw_showCode(int value);
void mw_showQuantity(num_t * v);
void mw_showPrice(num_t * v);
void mw_showSum(num_t * v);
void mw_showTotal(num_t * v);

GtkTreePath * mw_newRecord(void);
void mw_showRecord(struct record_receipt *rc);
void mw_addRecord(struct record_receipt *rc);

void mw_clearTable(void);
gboolean mw_wheelOn(void);
gboolean mw_wheelOff(void);
void mw_showTime(int dot);

void mw_setText(char * text);
void mw_showRecord(struct record_receipt *rc);

void mw_kkmConnect_on(char * kkm);
void mw_kkmConnect_off(void);

void mw_agentNotify(char * type, int v);

void mw_stopOn(void);
void mw_stopOff(void);

extern struct record_receipt * mw_current_record;
extern gboolean global_access_flag;
#endif // MAIN_WINDOW_H
