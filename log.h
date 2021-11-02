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

#ifndef LOG_H
#define LOG_H

enum {
    ASSERT,
    DB_CONNECT,
    WAREDB_CONNECTED,
    TRANZDB_CONNECTED,
    KKM_CONNECT,
    KKM_DISCONNECT,
    OPEN_RECEIPT_FAIL,
    CONTROL_FILENAME_FAIL,
    CONTROL_OPEN,
    CONTROL_OPEN_FAIL,
    EXIT_PROGRAM,
    };

int log_open(void);
void log_puts(int mode, char * text);
void log_printf(int mode, char *template, ...);
void log_message(int id, ...);
void log_close(void);

#ifdef OPERATION_JOURNAL
int control_doOpen(char * n_kkm, int session);
void control_close(void);
void control_puts(int mode, char * text);
void control_printf(int mode, char *template, ...);
#endif

#endif //LOG_H
