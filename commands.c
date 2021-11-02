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
#include <string.h>
#include <stdarg.h>
#include <iconv.h>

#include "protocol.h"
#include "commands.h"
#include "numeric.h"

int commands_command_id=-1;
int commands_answer_id=0;

static void bcdtoa(char str[], uint8_t *bcd, size_t size) {
    str[size*2]=0;
    while (size--) {
	str[size*2+1]='0'+(*bcd&0x0F);
	str[size*2]='0'+((*bcd&0xF0)>>4);
	bcd++;
	};
    }

void commands_getTm(struct tm *ts, uint8_t date[3], uint8_t time[3]) {
    if (date!=NULL) {
	ts->tm_mday=date[0];
	ts->tm_mon=date[1]-1;
	ts->tm_year=date[2]+100;
	}
    if (time!=NULL) {
	ts->tm_hour=time[0];
        ts->tm_min=time[1];
        ts->tm_sec=time[2];
	}
    }

int commands_reply(void * data, size_t size, struct source_info * src) {
    commands_command_id=src->message_id;
    src->answer_id=commands_answer_id;
    protocol_clear();
    if (protocol_answer(data, size, src)<0) return -1;
    commands_answer_id++;
    return 0;
    }

int commands_replyError(int prefix, int err, struct source_info * si) {
//puts("* commands_replyError()");
    struct {
	uint8_t prefix;
	uint8_t err;
	} __attribute__ ((packed)) msg;
    msg.prefix=prefix;
    msg.err=err;
    return commands_reply(&msg, sizeof(msg), si);
    }

int commands_echo(void) {
    struct source_info si;
    uint8_t cmd_prefix=0xFA;
    protocol_clear();
    if (protocol_command(&cmd_prefix, 1, NULL)<0) return -1;
    uint8_t answer_prefix=0;
    int r=protocol_result(&answer_prefix, 1, &si);
    if (r<0) return 0;
    if (r<1) {
	int r=commands_replyError(answer_prefix, -2, &si);
	usleep(COMMAND_INTERVAL);
	return r;
	}
    if (cmd_prefix!=answer_prefix) return 0;
    return 1;
    }

int commands_wait(int n) {
    while (n-->0) {
	int r=commands_echo();
	if (r!=0) return r;
	}
    return 0;
    }

static int ExecCmd(int cmd, void * cmd_data, size_t cmd_size, void * answer_data, size_t answer_size) {
    struct {
	uint8_t prefix;
	uint8_t data[255];
	} __attribute__ ((packed)) msg;
    msg.prefix=cmd;
    size_t len=1;
    if (cmd_size>0) {
	memcpy(msg.data, cmd_data, cmd_size);
	len+=cmd_size;
	}
    struct {
	uint8_t prefix;
	uint8_t err;
	uint8_t data[255];
	} __attribute__ ((packed)) answer;
    int n=5;
    struct source_info si;
    while (n-->0) {
	protocol_clear();
        if (protocol_command(&msg, len, NULL)<0) return -1;
	bzero(&answer, sizeof(answer));
	int r=protocol_result(&answer, sizeof(answer), &si);
	if (r>0 && answer.prefix==cmd) {
	    len=(answer_size>sizeof(answer))?sizeof(answer):answer_size;
	    if (len>0) memcpy(answer_data, answer.data, len);
	    return answer.err;
	    }
	if (r==0) {
	    r=commands_replyError(answer.prefix, -2, &si);
	    usleep(COMMAND_INTERVAL);
	    if (r<0) return -1;
	    }
	}
    return -1;
    }

int commands_beep(void) {
    return ExecCmd(0x13, NULL, 0, NULL, 0);
    }

int commands_kkmInfo(int arg, ...) {
    struct {
	uint8_t type;
	uint8_t subtype;
	uint8_t prot_ver;
	uint8_t prot_subver;
	uint8_t model;
	uint8_t lang;
	char name[256];
	} __attribute__ ((packed)) answer;
    int r=ExecCmd(0xFC, NULL, 0, &answer, sizeof(answer));
    if (r!=0) return r;
    va_list ap;
    va_start(ap,arg);
    while (arg!=END_ARGS) {
	switch (arg) {
	case DEV_TYPE: *(va_arg(ap,int*))=answer.type; break;
	case DEV_SUBTYPE: *(va_arg(ap,int*))=answer.subtype; break;
	case DEV_PROTOCOL_VER: *(va_arg(ap,int*))=answer.prot_ver; break;
	case DEV_PROTOCOL_SUBVER: *(va_arg(ap,int*))=answer.prot_subver; break;
	case DEV_MODEL: *(va_arg(ap,int*))=answer.model; break;
	case DEV_LANG: *(va_arg(ap,int*))=answer.lang; break;
	case DEV_NAME: strcpy(va_arg(ap,char*), answer.name); break;
	default: va_end(ap); return -1;
	    }
	arg=va_arg(ap, int);
	}
    va_end(ap);
    return 0;
    }

int commands_kkmState(int arg, ...) {
    struct {
	uint8_t seller;
	uint8_t fw_ver[2];
	uint16_t fw_build;
	uint8_t fw_date[3];
	uint16_t n_doc;
	uint16_t flags;
	uint8_t mode;
	uint8_t submode;
	uint8_t port;
	uint8_t date[3];
	uint8_t time[3];
	} __attribute__ ((packed)) answer;
    int r=ExecCmd(0x11, NULL, 0, &answer, sizeof(answer));
    if (r!=0) return r;
    struct tm *dtime;
    va_list ap;
    va_start(ap,arg);
    while (arg!=END_ARGS) {
	switch (arg) {
	case KKM_SELLER: *(va_arg(ap,int*))=answer.seller; break;
	case KKM_FW_VER: 
	    memcpy(va_arg(ap,void*),answer.fw_ver, sizeof(answer.fw_ver));
	    break;
	case KKM_FW_BUILD: *(va_arg(ap,int*))=answer.fw_build; break;
	case KKM_FW_DATE:
	    dtime=va_arg(ap,struct tm *);
	    commands_getTm(dtime, answer.fw_date, NULL);
	    break;
	case KKM_DOC_NUM: *(va_arg(ap,int*))=answer.n_doc; break;
	case KKM_FLAGS: *(va_arg(ap,int*))=answer.flags; break;
	case KKM_MODE: *(va_arg(ap,int*))=answer.mode; break;
	case KKM_SUBMODE: *(va_arg(ap,int*))=answer.submode; break;
	case KKM_PORT: *(va_arg(ap,int*))=answer.port; break;
	case KKM_DTIME:
	    dtime=va_arg(ap,struct tm *);
	    commands_getTm(dtime, answer.date, answer.time);
	    break;
	default: va_end(ap); return -1;
	    }
	arg=va_arg(ap, int);
	}
    va_end(ap);
    return 0;
    }

int commands_fpState(int arg, ...) {
    struct {
	uint8_t fw_ver[3];
	uint16_t fw_build;
	uint8_t ser_num[4];
	uint16_t zn;
	uint16_t z_free;
	uint8_t reg_count;
	uint8_t reg_free;
	uint8_t inn[6];
	uint8_t eklz_cnt;
	uint8_t eklz_free;
	uint8_t eklz_num[5];
	} __attribute__ ((packed)) answer;
    int r=ExecCmd(0x12, NULL, 0, &answer, sizeof(answer));
    if (r!=0) return r;
    va_list ap;
    va_start(ap,arg);
    while (arg!=END_ARGS) {
	switch (arg) {
	case FP_FW_VER: 
	    memcpy(va_arg(ap,void*), answer.fw_ver, sizeof(answer.fw_ver));
	    break;
	case FP_FW_BUILD: *(va_arg(ap,int*))=answer.fw_build; break;
	case FP_KKM_NUM:
	    bcdtoa(va_arg(ap,char*), answer.ser_num, sizeof(answer.ser_num));
	    break;
	case FP_Z_NUM: *(va_arg(ap,int*))=answer.zn; break;
	case FP_Z_FREE: *(va_arg(ap,int*))=answer.z_free; break;
	case FP_REG_COUNT: *(va_arg(ap,int*))=answer.reg_count; break;
	case FP_REG_FREE: *(va_arg(ap,int*))=answer.reg_free; break;
	case FP_INN:
	    bcdtoa(va_arg(ap,char*), answer.inn, sizeof(answer.inn));
	    break;
	case FP_EKLZ_COUNT: *(va_arg(ap,int*))=answer.eklz_cnt; break;
	case FP_EKLZ_FREE: *(va_arg(ap,int*))=answer.eklz_free; break;
	case FP_EKLZ_REG_NUM:
	    bcdtoa(va_arg(ap,char*), answer.eklz_num, sizeof(answer.eklz_num));
	    break;
	default: va_end(ap); return -1;
	    }
	arg=va_arg(ap, int);
	}
    va_end(ap);
    return 0;
    }

int commands_operReg(int reg, int *value) {
    uint8_t msg=reg;
    uint16_t answer=0;
    int r=ExecCmd(0x1B, &msg, 1, &answer, sizeof(answer));
    if (r!=0) return r;
    *value=answer;
    return 0;
    }
