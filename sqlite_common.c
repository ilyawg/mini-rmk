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

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

#include <sqlite3.h>

#include "vararg.h"
#include "database_common.h"
#include "sqlite_common.h"
#include "log.h"
#include "global.h"

static void internal_message(char * sql, char * err) {
    log_puts(0, sql);
    log_printf(1, "ОШИБКА SQLITE: %s", err);
    }

static int database_addString(char *trg, size_t size, char *src) {
    if (src==NULL) return database_addText(trg,size,"NULL");
    char * ptr=strchr(trg,0);
    char * end=trg+size;
    if (trg+2>=end) return -1;
    *ptr='\'';
    ptr++;
    while (*src!=0) {
	switch(*src) {
	case '\'':
	    if (ptr+2>=end) return -1;
	    *ptr='\'';
	    *(ptr+1)='\'';
	    ptr+=2;
	    break;
	default:
	    if (ptr+1>=end) return -1;
	    *ptr=*src;
	    ptr++;
	    break;
	    }
	src++;
	}
    if (ptr+1>=end) return -1;
    *ptr='\'';
    *(ptr+1)=0;
    ptr++;
    return 0;
    }

sqlite3_stmt * sqlite_prepareText(sqlite3 * db, char * text) {
    sqlite3_stmt * req;
#ifdef DEBUG	
    printf("SQLITE: <%s>\n",text);
#endif
    if (sqlite3_prepare(db,text,-1,&req,NULL)!=SQLITE_OK) {
	char * errmsg=(char*)sqlite3_errmsg(db);
	internal_message(text, errmsg);
    	sqlite3_finalize(req);
	return NULL;
	}
    return req;
    }        

sqlite3_stmt * sqlite_prepare(sqlite3 * db, int p, ...) {
    char text[2048];
    bzero(text,sizeof(text));
    va_list ap;
    va_start(ap,p);
    int r=0;
    while (p!=END_ARGS) {
	switch(p) {
        case TEXT_VALUE: r=database_addText(text,sizeof(text),va_arg(ap,char*)); break;
        case STRING_VALUE: r=database_addString(text,sizeof(text),va_arg(ap,char*)); break;
        case UINT_VALUE: r=database_addUInt(text,sizeof(text),va_arg(ap,int)); break;
	default:
#ifdef DEBUG
	    fprintf(stderr,"*** db_execQuery() ASSERT: invalid value type: %d\n",p);
#endif
	    va_end(ap);
	    return NULL;
	    }
	if (r) {
#ifdef DEBUG
	    fputs("*** db_execQuery() ASSERT: text buffer overflow",stderr);
#endif
	    va_end(ap);
	    return NULL;
	    }
	p=va_arg(ap,int);
	}
    va_end(ap);
    return sqlite_prepareText(db, text);
    }

int sqlite_execCmdText(sqlite3 * db, char * text) {
#ifdef DEBUG	
    printf("SQLITE: <%s>\n",text);
#endif
    char *errmsg=NULL;
    int r=sqlite3_exec(db, text, NULL, NULL, &errmsg);
    if (r!=SQLITE_OK) {
	internal_message(text, errmsg);
	sqlite3_free(errmsg);
	return -1;
	}
    return sqlite3_changes(db);
    }        

int sqlite_execCmd(sqlite3 * db, int p, ...) {
    char text[2048];
    bzero(text,sizeof(text));
    va_list ap;
    va_start(ap, p);
    int r=0;
    while (p!=END_ARGS) {
	switch(p) {
        case TEXT_VALUE: r=database_addText(text, sizeof(text), va_arg(ap, char*)); break;
        case STRING_VALUE: r=database_addString(text, sizeof(text), va_arg(ap, char*)); break;
        case UINT_VALUE: r=database_addUInt(text, sizeof(text), va_arg(ap, int)); break;
	default:
#ifdef DEBUG
	    fprintf(stderr,"*** sqlite_execCmd() ASSERT: invalid value type: %d\n",p);
#endif
	    return -1;
	    }
	if (r) {
#ifdef DEBUG
	    fputs("*** sqlite_execCmd() ASSERT: text buffer overflow",stderr);
#endif
	    va_end(ap);
	    return -1;
	    }
	p=va_arg(ap, int);
	}
    va_end(ap);
    return sqlite_execCmdText(db, text);
    }

int sqlite_getText(char * target, sqlite3_stmt * req, int column) {
    int t=sqlite3_column_type(req, column);
    if (t==SQLITE_NULL || t==SQLITE_BLOB) return -1;
    strcpy(target, sqlite3_column_text(req, column));
    return 0;
    }

int sqlite_getNum(char * target, sqlite3_stmt * req, int column) {
    int t=sqlite3_column_type(req, column);
    if (t==SQLITE_FLOAT || t==SQLITE_INTEGER) {
        strcpy(target, sqlite3_column_text(req, column));
        return 0;
	}
    strcpy(target, "NULL");
    return -1;
    }

