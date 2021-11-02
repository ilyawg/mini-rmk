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

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#include "database_common.h"
#include "conf.h"
#include "global.h"

int database_addText(char *trg, size_t size, char *src) {
    if (src==NULL) src="NULL";
    size_t trg_len=strlen(trg);
    size_t src_len=strlen(src);
    if (trg_len+src_len>=size) return -1;
    strcat(trg,src);
    return 0;
    }

int database_addUInt(char *trg, size_t size, int n) {
    char str[16];
    if (n<0) return database_addText(trg, size, "NULL");
    sprintf(str,"%u",n);
    return database_addText(trg,size,str);
    }

/*****************************************************************************/
void recordset_init(struct recordset * rec) {
    obstack_init(&rec->pool);
    rec->current_record=NULL;
    rec->first_record=NULL;
    rec->records_count=0;
    }

void recordset_destroy(struct recordset * rec) {
    obstack_free(&rec->pool,NULL);
    }

void * recordset_begin(struct recordset * rec) {
    rec->current_record=rec->first_record;
    if (rec->current_record==NULL) return NULL;
    return rec->current_record->record;
    }

void * recordset_next(struct recordset * rec) {
    if (rec->current_record==NULL) return NULL;
    rec->current_record=rec->current_record->next_record;
    if (rec->current_record==NULL) return NULL;
    return rec->current_record->record;
    }

void recordset_clear(struct recordset * rec) {
    if (rec->first_record==NULL) {
#ifdef DEBUG
	fputs("*** recordset_clear() ASSERT: already clear", stderr);
#endif
	return;
	}
    obstack_free(&rec->pool,rec->first_record);
    rec->current_record=NULL;
    rec->first_record=NULL;
    rec->records_count=0;
    }

void * recordset_new(struct recordset * rset, size_t size) {
    struct record_list * rl=obstack_alloc(&rset->pool, sizeof(struct record_list));
    bzero(rl, sizeof(struct record_list));
    rl->record=obstack_alloc(&rset->pool, size);
    bzero(rl->record, size);
    if (rset->first_record==NULL) rset->first_record=rl;
    else rset->current_record->next_record=rl;
    rset->current_record=rl;
    rset->records_count++;
    return rl->record;
    }
