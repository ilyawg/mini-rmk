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

#include "../vararg.h"
#include "../numeric.h"

int commands_beep(void);
enum {
    KKM_OPERATOR=BEGIN_ARGS,
    KKM_FW_VERSION,
    KKM_FW_BUILD,
    KKM_FW_DATE,
    KKM_NUM,
    KKM_DOC_NUM,
    KKM_FLAGS,
    KKM_MODE,
    KKM_SUBMODE,
    KKM_PORT,
    FP_FW_VERSION,
    FP_FW_BUILD,
    FP_FW_DATE,
    KKM_DATE,
    FP_FLAGS,
    KKM_SERIAL,
    KKM_SESSION_NUM,
    KKM_FREE_SESSIONS,
    KKM_REG_NUM,
    KKM_FREE_REGS,
    KKM_INN
    };
int commands_getState(int arg, ...);

enum {
    CMD_CASHIN=0x50,
    CMD_CASHOUT,
    CMD_SALE=0x80,
    CMD_RETURN=0x82,
    CMD_STORNO=0x84
    };
    
int commands_cash(int cmd, num_t * summ);
int commands_registration(int cmd, num_t * price, num_t * quantity, char * text);
int commands_cancel(void);
int commands_moneyReg(int reg, num_t * v);
int commands_operReg(int reg);
int commands_closeReceipt(num_t * summ);
int commands_xreport(void);
int commands_zreport(void);

enum {
    KKM_DOC_SALE=0,
    KKM_DOC_RETURN=2
    };

int commands_open(int doc_type);
#endif //COMMANDS_H
