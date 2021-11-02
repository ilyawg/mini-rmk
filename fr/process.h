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

#ifndef PROCESS_C
#define PROCESS_C

#include <gtk/gtk.h>

enum {
    XREPORT_EVENT,
    CASHIN_EVENT,
    CASHOUT_EVENT,
    CANCEL_EVENT,
    TOTAL_EVENT,
    CASH_EVENT,
    CASH_CLOSE_EVENT,
    RECEIPT_EVENT,
    RECEIPT_CLOSE_EVENT,
    SESSION_EVENT,
    RETURN_EVENT,
    STORNO_EVENT,
    CODE_EVENT,
    BARCODE_EVENT,
    SERVICE_OPEN_EVENT,
    SERVICE_CLOSE_EVENT,
    WARE_OPEN_EVENT,
    WARE_CLOSE_EVENT,
    };

int process_event(int id, ...);
#endif //PROCESS_C
