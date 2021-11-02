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
#include <stdint.h>
#include <stdarg.h>
#include <stdarg.h>
#include <time.h>

#include "conf.h"
#include "protocol.h"
#include "commands.h"

static int ExecCmd(int cmd, int t, void * data, size_t size) {
    struct {
	uint8_t cmd;
	uint32_t psw;
	} __attribute__ ((packed)) msg;
    msg.cmd=cmd;
    msg.psw=global_conf_kkm_psw;
    if (protocol_send(&msg, sizeof(msg))<0) return -256;
    enum {CMD=0, ERR, DATA};
    uint8_t buf[256];
    if (protocol_recv(buf, sizeof(buf), t)<0 || buf[CMD]!=cmd) return -256;
//print_hex(buf, size+2);
    if (buf[ERR]!=0) return -buf[ERR];
    if (size>sizeof(buf)) size=sizeof(buf);
    if (size>0) memcpy(data, &buf[DATA], size);
    return size;
    }

int commands_beep(void) {
    return ExecCmd(0x13, 1, NULL, 0);
    }

static void GetTm(struct tm *ts, uint8_t date[3], uint8_t time[3]) {
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

static void GetINN(char * inn, uint8_t * num) {
    int64_t n=0;
    memcpy(&n, num, 6);
    sprintf(inn, "%Ld\n", n);
    }

int commands_getState(int arg, ...) {
    struct {
	uint8_t oper;
	uint8_t kkm_fw_ver[2];
	uint16_t kkm_fw_build;
	uint8_t kkm_fw_date[3];
	uint8_t n_kkm;
	uint16_t doc_num;
	uint16_t flags;
	uint8_t mode;
	uint8_t submode;
	uint8_t port;
	uint8_t fp_fw_ver[2];
	uint16_t fp_fw_build;
	uint8_t fp_fw_date[3];
	uint8_t date[3];
	uint8_t time[3];
	uint8_t fp_flags;
	uint32_t serial;
	uint16_t n_session;
	uint16_t free_sessions;
	uint8_t n_reg;
	uint8_t free_regs;
	uint8_t inn[6];
	} __attribute__ ((packed)) msg;
    int r=ExecCmd(0x11, 10, &msg, sizeof(msg));
    if (r<0) return r;

    va_list ap;
    va_start(ap,arg);
    while (arg!=END_ARGS) {
	switch (arg) {
	case KKM_OPERATOR:	*(va_arg(ap, int*))=msg.oper; break;
	case KKM_FW_VERSION:
	    sprintf(va_arg(ap, char*), "%c.%c", msg.kkm_fw_ver[0], msg.kkm_fw_ver[1]); break;
	case KKM_FW_BUILD:	*(va_arg(ap, int*))=msg.kkm_fw_build; break;
	case KKM_FW_DATE:	GetTm(va_arg(ap, struct tm*), msg.kkm_fw_date, NULL); break;
	case KKM_NUM:		*(va_arg(ap, int*))=msg.n_kkm; break;
	case KKM_DOC_NUM:	*(va_arg(ap, int*))=msg.doc_num; break;
	case KKM_FLAGS:		*(va_arg(ap, int*))=msg.flags; break;
	case KKM_MODE:		*(va_arg(ap, int*))=msg.mode; break;
	case KKM_SUBMODE:	*(va_arg(ap, int*))=msg.submode; break;
	case KKM_PORT:		*(va_arg(ap, int*))=msg.port; break;
	case FP_FW_VERSION:
	    sprintf(va_arg(ap, char*), "%c.%c", msg.fp_fw_ver[0], msg.fp_fw_ver[1]); break;
	case FP_FW_BUILD:	*(va_arg(ap, int*))=msg.fp_fw_build; break;
	case FP_FW_DATE:	GetTm(va_arg(ap, struct tm*), msg.fp_fw_date, NULL); break;
	case KKM_DATE:		GetTm(va_arg(ap, struct tm*), msg.date, msg.time); break;
	case FP_FLAGS:		*(va_arg(ap, int*))=msg.fp_flags; break;
	case KKM_SERIAL:	sprintf(va_arg(ap, char*), "%d", msg.serial); break;
	case KKM_SESSION_NUM:	*(va_arg(ap, int*))=msg.n_session; break;
	case KKM_FREE_SESSIONS: *(va_arg(ap, int*))=msg.free_sessions; break;
	case KKM_REG_NUM:	*(va_arg(ap, int*))=msg.n_reg; break;
	case KKM_FREE_REGS:	*(va_arg(ap, int*))=msg.free_regs; break;
	case KKM_INN:		GetINN(va_arg(ap, char*), msg.inn); break;
	    }
	arg=va_arg(ap, int);
	}
    va_end(ap);
    return 0;
    }

int commands_xreport(void) {
    return ExecCmd(0x40, 5, NULL, 0);
    }

int commands_zreport(void) {
    return ExecCmd(0x41, 300, NULL, 0);
    }

int commands_cash(int cmd, num_t * summ) {
//puts("* commands_cash()");
    NUMERIC(v, 2);
    num_cpy(&v, summ);
    struct {
	uint8_t cmd;
	uint32_t psw;
	uint8_t summ[5];
	} __attribute__ ((packed)) msg;
    msg.cmd=cmd;
    msg.psw=global_conf_kkm_psw;
    memcpy(msg.summ, &v.value, sizeof(msg.summ));
    if (protocol_send(&msg, sizeof(msg))<0) return -256;
    struct {
	uint8_t cmd;
	uint8_t err;
	uint16_t n_doc;
	} __attribute__ ((packed)) answer;
    bzero(&answer, sizeof(answer));
    if (protocol_recv(&answer, sizeof(answer), 5)<0 || answer.cmd!=cmd) return -256;
    if (answer.err!=0) return -answer.err;
    return answer.n_doc;
    }
    
int commands_registration(int cmd, num_t * price, num_t * quantity, char * text) {
    NUMERIC(p, 2);
    NUMERIC(q, 3);
    struct {
	uint8_t cmd;
	uint32_t psw;
	uint8_t quantity[5];
	uint8_t price[5];
	uint8_t depart;
	uint8_t tax[4];
	uint8_t text[40];
	} __attribute__ ((packed)) msg;
    bzero(&msg, sizeof(msg));
    msg.cmd=cmd;
    msg.psw=global_conf_kkm_psw;
    if (quantity!=NULL) {
        num_cpy(&q, quantity);
	memcpy(msg.quantity, &q.value, sizeof(msg.quantity));
	}
    else {
	uint64_t n=1000;
	memcpy(msg.quantity, &n, sizeof(msg.quantity));
	}

    if (price!=NULL) {
	num_cpy(&p, price);
	memcpy(msg.price, &p.value, sizeof(msg.price));
	}
	
    msg.depart=1;
    if (text!=NULL) {
	size_t len=strlen(text);
	if (len>sizeof(msg.text)) len=sizeof(msg.text);
	if (len>0) memcpy(msg.text, text, len);
	}
    if (protocol_send(&msg, sizeof(msg))<0) return -256;
    struct {
	uint8_t cmd;
	uint8_t err;
	} __attribute__ ((packed)) answer;
    bzero(&answer, sizeof(answer));
    if (protocol_recv(&answer, sizeof(answer), 5)<0 || answer.cmd!=cmd) return -256;
    return -answer.err;
    }

int commands_cancel(void) {
    return ExecCmd(0x88, 5, NULL, 0);
    }

int commands_total(num_t * summ) {
    NUMERIC(v, 2);
    struct {
	uint8_t oper;
	uint8_t summ[5];
	} __attribute__ ((packed)) answer;
    int r=ExecCmd(0x89, 5, &answer, sizeof(answer));
    if (r<0) return r;
    memcpy(&v.value, answer.summ, sizeof(answer.summ));
    num_cpy(summ, &v);
    return 0;
    }

int commands_moneyReg(int reg, num_t * value) {
    NUMERIC(v, 2);
    const int cmd=0x1A;
    struct {
	uint8_t cmd;
	uint32_t psw;
	uint8_t reg;
	} __attribute__ ((packed)) msg;
    msg.cmd=cmd;
    msg.psw=global_conf_kkm_psw;
    msg.reg=reg;
    if (protocol_send(&msg, sizeof(msg))<0) return -256;

    struct {
	uint8_t cmd;
	uint8_t err;
	uint8_t oper;
	uint8_t v[5];
	} __attribute__ ((packed)) answer;
    if (protocol_recv(&answer, sizeof(answer), 5)<0 || answer.cmd!=cmd) return -256;
    if (answer.err!=0) return -answer.err;
    memcpy(&v.value, answer.v, sizeof(answer.v));
    num_cpy(value, &v);
    return 0;
    }

int commands_operReg(int reg) {
    const int cmd=0x1B;
    struct {
	uint8_t cmd;
	uint32_t psw;
	uint8_t reg;
	} __attribute__ ((packed)) msg;
    msg.cmd=cmd;
    msg.psw=global_conf_kkm_psw;
    msg.reg=reg;
    if (protocol_send(&msg, sizeof(msg))<0) return -256;

    struct {
	uint8_t cmd;
	uint8_t err;
	uint8_t oper;
	uint16_t v;
	} __attribute__ ((packed)) answer;
    if (protocol_recv(&answer, sizeof(answer), 5)<0 || answer.cmd!=cmd) return -256;
    if (answer.err!=0) return -answer.err;
    return answer.v;
    }

int commands_closeReceipt(num_t * summ) {
    NUMERIC(v, 2);
    const int cmd=0x85;
    struct {
	uint8_t cmd;
	uint32_t psw;
	uint8_t summ1[5];
	uint8_t summ2[5];
	uint8_t summ3[5];
	uint8_t summ4[5];
	uint16_t disc;
	uint8_t tax[4];
	uint8_t text[40];
	} __attribute__ ((packed)) msg;
    bzero(&msg, sizeof(msg));
    msg.cmd=cmd;
    msg.psw=global_conf_kkm_psw;
    num_cpy(&v, summ);
    memcpy(msg.summ1, &v.value, sizeof(msg.summ1));
    if (protocol_send(&msg, sizeof(msg))<0) return -256;

    struct {
	uint8_t cmd;
	uint8_t err;
	uint8_t oper;
	uint8_t summ[5];
	} __attribute__ ((packed)) answer;
    if (protocol_recv(&answer, sizeof(answer), 10)<0 || answer.cmd!=cmd) return -256;
    if (answer.err!=0) return -answer.err;
    num_init(summ, 2);
    memcpy(&summ->value, answer.summ, sizeof(answer.summ));
    return 0;
    }

int commands_open(int doc_type) {
    const int cmd=0x8D;
    struct {
	uint8_t cmd;
	uint32_t psw;
	uint8_t doc_type;
	} __attribute__ ((packed)) msg;
    msg.cmd=cmd;
    msg.psw=global_conf_kkm_psw;
    msg.doc_type=doc_type;
    if (protocol_send(&msg, sizeof(msg))<0) return -256;

    struct {
	uint8_t cmd;
	uint8_t err;
	} __attribute__ ((packed)) answer;
    if (protocol_recv(&answer, sizeof(answer), 5)<0 || answer.cmd!=cmd) return -256;
    if (answer.err!=0) return -answer.err;
    return 0;
    }

int commands_print(void) {
    return ExecCmd(0xB0, 5, NULL, 0);
    }
