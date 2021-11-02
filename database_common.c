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
    if (n<0) return database_addText(trg,size,"NULL");
    sprintf(str,"%u",n);
    return database_addText(trg,size,str);
    }

void database_tmToString(char *s, struct tm * ts) {
    sprintf(s,"%u-%02u-%02u %02u:%02u:%02u",
	ts->tm_year+1900,
	ts->tm_mon+1,
	ts->tm_mday,
	ts->tm_hour,
	ts->tm_min,
	ts->tm_sec);
    }
