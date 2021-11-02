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

#ifndef COMMANDS_H
#define COMMANDS_H

#include <time.h>

#include "protocol.h"
#include "numeric.h"
#include "vararg.h"

#define COMMAND_INTERVAL	10000
#define ANSWER_INTERVAL		5000

extern int commands_command_id;
extern int commands_answer_id;

void commands_getTm(struct tm *ts, uint8_t date[3], uint8_t time[3]);

int commands_reply(void * data, size_t size, struct source_info * src);
int commands_replyError(int prefix, int err, struct source_info * si);

int commands_beep(void);
int commands_echo(void);
int commands_wait(int n);

enum {
    DEV_TYPE=BEGIN_ARGS,
    DEV_SUBTYPE,
    DEV_PROTOCOL_VER,
    DEV_PROTOCOL_SUBVER,
    DEV_MODEL,
    DEV_LANG,
    DEV_NAME,

    KKM_SELLER,
    KKM_FW_VER,
    KKM_FW_BUILD,
    KKM_FW_DATE,
    KKM_DOC_NUM,
    KKM_FLAGS,
    KKM_MODE,
    KKM_SUBMODE,
    KKM_PORT,
    KKM_DTIME,

    FP_FW_VER,
    FP_FW_BUILD,
    FP_KKM_NUM,
    FP_Z_NUM,
    FP_Z_FREE,
    FP_REG_COUNT,
    FP_REG_FREE,
    FP_INN,
    FP_EKLZ_COUNT,
    FP_EKLZ_FREE,
    FP_EKLZ_REG_NUM,
    };
int commands_kkmInfo(int arg, ...);
int commands_kkmState(int arg, ...);
int commands_fpState(int arg, ...);

#endif //COMMANDS_H
