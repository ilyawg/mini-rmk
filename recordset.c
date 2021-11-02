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

#include <string.h>
#include <stdlib.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#include "recordset.h"

struct record_list {
    void * record;
    void * next;
    };

void recordset_init(struct recordset * rset) {
    obstack_init(&rset->pool);
    rset->current=NULL;
    rset->first=NULL;
    rset->count=0;
    }

void recordset_destroy(struct recordset * rec) {
    obstack_free(&rec->pool,NULL);
    }

void * recordset_begin(struct recordset * rset) {
    rset->current=rset->first;
    if (rset->current==NULL) return NULL;
    return rset->current->record;
    }

void * recordset_next(struct recordset * rset) {
    if (rset->current==NULL) return NULL;
    rset->current=rset->current->next;
    if (rset->current==NULL) return NULL;
    return rset->current->record;
    }

void recordset_clear(struct recordset * rset) {
    if (rset->first!=NULL) obstack_free(&rset->pool, rset->first);
    rset->current=NULL;
    rset->first=NULL;
    rset->count=0;
    }

void recordset_add(struct recordset * rset, void * data, size_t size) {
    struct record_list * rl=obstack_alloc(&rset->pool, sizeof(struct record_list));
    bzero(rl, sizeof(struct record_list));
    rl->record=obstack_alloc(&rset->pool, size);
    memcpy(rl->record, data, size);
    if (rset->first==NULL) rset->first=rl;
    else rset->current->next=rl;
    rset->current=rl;
    rset->count++;
    }

void * recordset_new(struct recordset * rset, size_t size) {
    struct record_list * rl=obstack_alloc(&rset->pool, sizeof(struct record_list));
    bzero(rl, sizeof(struct record_list));
    rl->record=obstack_alloc(&rset->pool, size);
    bzero(rl->record, size);
    if (rset->first==NULL) rset->first=rl;
    else rset->current->next=rl;
    rset->current=rl;
    rset->count++;
    return rl->record;
    }
