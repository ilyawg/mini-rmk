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

#ifndef SQLITE_COMMON_H
#define SQLITE_COMMON_H

#include <stdarg.h>
#include <sqlite3.h>

#include "database_common.h"

sqlite3_stmt * sqlite_prepareText(sqlite3 * db, char * text);
sqlite3_stmt * sqlite_prepare(sqlite3 * db, int p, ...);
int sqlite_execCmdText(sqlite3 * db, char * text);
int sqlite_execCmd(sqlite3 * db, int p, ...);

#endif //SQLITE_COMMON_H
