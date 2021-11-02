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

#ifndef PROCESS_H
#define PROCESS_H

enum {ST_UNDEF, ST_UNAVAILABLE, ST_LOCK, ST_ACTION, ST_DONE};

struct thread_vars {
    int session_counter;
    int client_counter;
    int exec;
    time_t time;
    int ware_exec;
    int locked;
    int pipe[2];
    };

void exchange_thread(struct thread_vars * vars);

#endif //PROCESS_H
