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

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>
#include <iconv.h>

void protocol_closePort(void);
int protocol_openPort(void);

struct source_info {
    int src;
    int trg;
    int message_id;
    int answer_id;
    };
int protocol_message(void *data, size_t size, struct source_info * md);
int protocol_answer(void *data, size_t size, struct source_info * sinfo);
void protocol_clear(void);
int protocol_command(void *data, size_t size, struct source_info * si);
int protocol_result(void *data, size_t size, struct source_info * si);

#endif //PROTOCOL_H_
