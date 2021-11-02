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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#include "global.h"
#include "receipt.h"
#include "kkm.h"
#include "main_window.h"
#include "localdb.h"
#include "../numeric.h"

static struct obstack internal_receipt_pool;
static struct record_receipt * internal_first_record;
static struct record_receipt * internal_last_record;
static int internal_records_count;

void receipt_init(void) {
    obstack_init(&internal_receipt_pool);
    global_receipt_summ=NULL;
    internal_first_record=NULL;
    internal_last_record=NULL;
    internal_records_count=0;
    }

void receipt_clear(void) {
    if (global_receipt_summ!=NULL) {
	obstack_free(&internal_receipt_pool, global_receipt_summ);
	global_receipt_summ=NULL;
	}
    internal_first_record=NULL;
    internal_last_record=NULL;
    internal_records_count=0;
    }

/*****************************************************************************/
struct record_receipt * receipt_newRecord(void) {
    if (global_receipt_summ==NULL) {
	global_receipt_summ=obstack_alloc(&internal_receipt_pool, sizeof(num_t));
        num_init(global_receipt_summ, 2);
	}
    struct record_receipt * rec=obstack_alloc(&internal_receipt_pool, sizeof(struct record_receipt));
    bzero(rec, sizeof(struct record_receipt));
    rec->num=internal_records_count+1;
    num_init(&rec->price, 2);
    num_init(&rec->quantity, 3);
    num_init(&rec->summ, 2);
    return rec;
    }

void receipt_addRecord(struct record_receipt * rec) {
    if (internal_first_record==NULL) internal_first_record=rec;
    else internal_last_record->next=rec;
    rec->prev=internal_last_record;
    internal_last_record=rec;
    internal_records_count++;
    }

struct record_receipt * receipt_getRecordByNum(int num) {
    struct record_receipt * i=internal_first_record;
    while (i!=NULL) {
	if (i->num==num) return i;
	i=i->next;
	}
    return NULL;
    }

struct record_receipt * receipt_getRecordByID(int id) {
    struct record_receipt * i=internal_first_record;
    while (i!=NULL) {
	if (i->id==id) return i;
	i=i->next;
	}
    return NULL;
    }

struct record_receipt * receipt_findRecordByCode(int code, num_t * quantity) {
    struct record_receipt * i=internal_first_record;
    while (i!=NULL) {
	if (i->code==code && num_cmp(&i->quantity, quantity)>=0) return i;
	i=i->next;
	}
    return NULL;
    }

struct record_receipt * receipt_findRecordByBarcode(char * barcode, num_t * quantity) {
    struct record_receipt * i=internal_first_record;
    while (i!=NULL) {
	if (!strcmp(i->barcode, barcode) && num_cmp(&i->quantity, quantity)>=0) return i;
	i=i->next;
	}
    return NULL;
    }


static gboolean SetCursor(struct record_receipt * rc) {
    mw_setCursor(rc);
    return FALSE;
    }

void receipt_showAll(void) {
    struct record_receipt * rc=internal_first_record;
    if (global_receipt_summ!=NULL) {
        num_clear(global_receipt_summ);
	while (rc!=NULL) {
	    num_addThis(global_receipt_summ, &rc->summ);
	    rc->path=mw_newRecord();
	    mw_showRecord(rc);
	    mw_current_record=rc;
	    rc=rc->next;
	    }	
	g_idle_add((GSourceFunc)SetCursor, NULL);
	}
    }

/*****************************************************************************/
void receipt_upRecord(void) {
    if (mw_current_record!=NULL) {
	struct record_receipt * i=mw_current_record->prev;
	if (i!=NULL) {
	    gdk_threads_enter();
	        mw_setCursor(i);
		mw_showText("");
		mw_showCode(i->code);
		mw_showQuantity(&i->quantity);
		mw_showPrice(&i->price);
		mw_showSum(&i->summ);
    	    gdk_threads_leave();
	    }
	}
    }

void receipt_downRecord(void) {
    if (mw_current_record!=NULL) {
	struct record_receipt * i=mw_current_record->next;
	if (i!=NULL) {
	    gdk_threads_enter();
		mw_setCursor(i);
		mw_showText("");
		mw_showCode(i->code);
		mw_showQuantity(&i->quantity);
		mw_showPrice(&i->price);
		mw_showSum(&i->summ);
    	    gdk_threads_leave();
	    }
	}
    }

int receipt_processAll(receipt_callback callback, void * data) {
    struct record_receipt * i=internal_first_record;
    while (i!=NULL) {
	if (callback(i, data)<0) return -1;
	i=i->next;
	}
    return 0;
    }
