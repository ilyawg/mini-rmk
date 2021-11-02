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

#ifndef RECEIPT_H
#define RECEIPT_H

#include <gtk/gtk.h>

#include "../numeric.h"

struct record_receipt {
    int id;
    int num;
    int code;
    char barcode[20];
    char longtitle[255];
    char shortcut[255];
    num_t price;
    num_t quantity;
    num_t summ;
    GtkTreePath * path;
    struct record_receipt *prev;
    struct record_receipt *next;
    };

struct record_receipt * receipt_newRecord(void);
void receipt_addRecord(struct record_receipt * rec);
struct record_receipt * receipt_findRecordByBarcode(char * barcode, num_t * quantity);
struct record_receipt * receipt_findRecordByCode(int code, num_t * quantity);
struct record_receipt * receipt_getRecordByNum(int num);
struct record_receipt * receipt_getRecordByID(int id);
struct record_receipt * receipt_findRecordByCode(int code, num_t * quantity);
struct record_receipt * receipt_findRecordByBarcode(char * barcode, num_t * quantity);
void receipt_showAll(void);

typedef int (*receipt_callback)(struct record_receipt * rec, void * data);
int receipt_processAll(receipt_callback callback, void * data);

#endif //RECEIPT_H
