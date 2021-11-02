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

#ifndef RECORDSET_H
#define RECORDSET_H

#include <obstack.h>

struct recordset {
    int count;
    struct record_list * first;
    struct record_list * current;
    struct obstack pool;
    };

void recordset_init(struct recordset * rec);
void recordset_destroy(struct recordset * rec);
void * recordset_begin(struct recordset * rec);
void * recordset_next(struct recordset * rec);
void recordset_clear(struct recordset * rec);
void recordset_add(struct recordset * rec, void * data, size_t size);
void * recordset_new(struct recordset * rset, size_t size);
void recordset_clear(struct recordset * rec);

#endif //RECORDSET_H
