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

#ifndef DATABASE_COMMON_H
#define DATABASE_COMMON_H

#include <time.h>
#include <obstack.h>

#include "vararg.h"

//typedef void (*dbErrHandler)(char *text);

//операции
enum {
    DB_NO_OPERATION=0,		// нет операции
    DB_UPDATE,			// вставка/замена указанных позиций
    DB_UPDATE_AND_CLEAR,	// вставка/замена указанных позиций и удаление остальных
    DB_CLEAR_AND_INSERT,	// очистка базы и вставка указанных позиций
    DB_DELETE			// удаление указанных позиций
    };

enum {
    TEXT_VALUE=BEGIN_ARGS,
    STRING_VALUE,
    UINT_VALUE,
    };

#define AS_TEXT(_arg)	    TEXT_VALUE, _arg
#define AS_STRING(_arg)     STRING_VALUE, _arg
#define AS_UINT(_arg)	    UINT_VALUE, _arg

int database_addText(char *trg, size_t size, char *src);
int database_addUInt(char *trg, size_t size, int n);
void database_tmToString(char *s, struct tm * ts);
#endif //DATABASE_COMMON_H
