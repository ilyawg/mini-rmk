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

#ifndef KKM_H
#define KKM_H

#include <gtk/gtk.h>

#include "protocol.h"
//#define OPERATION_TIMEOUT	4400
#define OPERATION_TIMEOUT	4300
enum {
    SESSION_ERROR,
    SESSION_CLOSED,
    SESSION_OPEN,
    SESSION_UNDEF,
    SESSION_CLEAR
    };

void kkm_init(void);
gboolean kkm_connect(void);
void kkm_destroy(void);
void kkm_getNum(char * s);
size_t kkm_strToKKM(char * trg, size_t size, char * src);

int kkm_sync(void);
gboolean kkm_error(int err, gboolean f);

#endif //KKM_H
