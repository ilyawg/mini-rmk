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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include <fcntl.h>
#include <errno.h>

#include "global.h"
#include "protocol.h"
#include "conf.h"

static int internal_fd=0;

static int internal_C1=1;
static int internal_C2=1;
static int internal_C3=1;
static int internal_C4=1;
#define DEBUG
#ifdef DEBUG
void print_hex(void *data,size_t size) {
    unsigned int i;
    for(i=0;i<size;i++) printf("%02x ",*((uint8_t*)data+i));
    putchar('\n');
    }
#endif

void protocol_closePort(void) {
//puts("* protocol_closePort()");
    if (internal_fd>0) close(internal_fd);
    internal_fd=0;
    }
    
static int SetAttr(int fd) {
    struct termios tios;
    if (tcgetattr(fd, &tios)<0) return -1;
    cfsetspeed(&tios, global_conf_kkm_baud);
    cfmakeraw(&tios);
    tios.c_cc[VMIN]=0;
    tios.c_cc[VTIME]=1;
    if (tcsetattr(fd, TCSAFLUSH, &tios)<0) return -1;
    int status;
    ioctl(fd, TIOCMGET, &status);
    status&=~TIOCM_RTS;
    status|=TIOCM_DTR;
    ioctl(fd, TIOCMSET, &status);
    return 0;
    }
    
int protocol_openPort(void) {
    int fd=open(global_conf_kkm_port, O_RDWR);
    if (fd<0) return -1;
    if (SetAttr(fd)<0) {
	close(fd);
	return -1;
	}
    internal_fd=fd;
    return fd;
    }

static int CalcCRC (void *data, size_t size) {
//puts("check CRC:");
//print_hex(data,size);    
    uint16_t crc=0;
    uint8_t * ptr=data;
    int i;
    while(size--) {
	for (i=0;i<8;i++) crc=(crc&0x8000) ? (crc<<1)^0x1021 : (crc<<=1);
	crc^=*ptr++;
	}
//printf("crc: %04x\tcalc: %04x\n",*(uint16_t*)ptr,crc);
    if (*(uint16_t*)ptr==crc) return 0;
    *(uint16_t*)ptr=crc;	    
    return -1;
    }

static int SerialSend(void * data, size_t size) {
//fputs("SerialSend(): ", stdout);
//print_hex(data, size);
    int count=0;
    while (count<size) {
	int r=write(internal_fd, (uint8_t*)data+count, size-count);
	if (r<=0) return -1;
	count+=r;
	}
    return count;
    }

static int CopyMask(uint8_t * trg, size_t trgsize, uint8_t * src, size_t srcsize) {
    size_t count=0;
    int i;
    for(i=0; i<srcsize; i++) {
	switch (*src) {
	case 0x7E:
	    count+=2; if (count>trgsize) return -1;
	    *(trg++)=0x7D;
	    *(trg++)=0x9E;
	    break;
	case 0x7D:
	    count+=2; if (count>trgsize) return -1;
	    *(trg++)=0x7D;
	    *(trg++)=0x9D;
	    break;
	default:
	    count+=1; if (count>trgsize) return -1;
	    *(trg++)=*src;
	    }
	src++;
	}
    return count;
    }

static int SendMsg(void *data, size_t size) {
    struct {
	uint8_t head;
	uint8_t data[264];
	} __attribute__ ((packed)) msg;
    msg.head=0x7E;
    int count=0;
    size_t len=CopyMask(msg.data, sizeof(msg.data), data, size);
    if (len<0) return -1;
    return SerialSend(&msg, sizeof(msg)-sizeof(msg.data)+len);
    }

int protocol_answer(void *data, size_t size, struct source_info * sinfo) {
    struct {
	uint8_t trg;
	uint8_t src;
	uint8_t mode;
	uint8_t C3;
	uint8_t C4;
	uint8_t size;
	uint8_t data[256];
	uint8_t crc[2];
	} __attribute__ ((packed)) msg;
    msg.src=sinfo->src;
    msg.trg=sinfo->trg;
    msg.mode=1;
    msg.C3=sinfo->message_id;
    msg.C4=sinfo->answer_id;
    if (size>0) memcpy(msg.data, data, size);
    else size=0;
    msg.size=size;
    size+=sizeof(msg)-sizeof(msg.data);
    CalcCRC(&msg, size-sizeof(msg.crc));
    return SendMsg(&msg, size);
    }

/*****************************************************************************/
static int SerialGetData(void * data, size_t size) {
//puts("* SerialGetData()");
    int i=0;
    while (i<size) {
	uint8_t b=0;
	int r=read(internal_fd, &b, 1);
	if (r<=0) return -1;
	if (b==0x7D) {
	    r=read(internal_fd, &b, 1);
	    if (r<=0) return -1;
	    switch (b) {
	    case 0x9E: b=0x7E; break;
	    case 0x9D: b=0x7D; break;
	    default: return -1;
		}
	    }
	*(uint8_t*)(data+i)=b;
	i++;
	}
//print_hex(data, i);
    return i;
    }

int protocol_message(void *data, size_t size, struct source_info * sinfo) {
    struct {
        uint8_t src;
        uint8_t trg;
        uint8_t mode;
        uint8_t C1;
        uint8_t C2;
        uint8_t size;
        uint8_t data[255];
	uint8_t crc[2];
	} __attribute__ ((packed)) msg;
    uint8_t prefix=0;
    int r=read(internal_fd, &prefix, 1);
    if (r==0) return 0;
    if (r<0) return -1;
    if (prefix!=0x7E) return -1;
    if (SerialGetData(&msg, sizeof(msg)-sizeof(msg.data)-sizeof(msg.crc))<0) return -1;
    if (SerialGetData(msg.data, msg.size+sizeof(msg.crc))<0) return -1;
    if (CalcCRC(&msg, sizeof(msg)-sizeof(msg.data)-sizeof(msg.crc)+msg.size)) return -1;
    if (data!=NULL && size>0) {
	if (size>msg.size) size=msg.size;
	memcpy(data, msg.data, size);
	}
    if (msg.mode!=0) return -1;
    if (sinfo!=NULL) {
	sinfo->src=msg.src;
	sinfo->trg=msg.trg;
	sinfo->message_id=msg.C1;
	sinfo->answer_id=msg.C2;
	}
    return size;
    }

void protocol_clear(void) {
//puts("* protocol_clear()");
    tcflush(internal_fd, TCIFLUSH);
    }

/*****************************************************************************/
int protocol_command(void *data, size_t size, struct source_info * si) {
    struct {
	uint8_t trg;
	uint8_t src;
	uint8_t mode;
	uint8_t message_id;
	uint8_t answer_id;
	uint8_t size;
	uint8_t data[256];
	uint8_t crc[2];
	} __attribute__ ((packed)) msg;
    if (si!=NULL) {
	msg.src=si->src;
	msg.trg=si->trg;
	msg.message_id=si->message_id;
	msg.answer_id=si->answer_id;
	}
    else {
	msg.src=0xFF;
        msg.trg=0;
	msg.message_id=0;
	msg.answer_id=0;
	}
    msg.mode=0;
    if (size>0) memcpy(msg.data, data, size);
    else size=0;
    msg.size=size;
    size+=sizeof(msg)-sizeof(msg.data);
    CalcCRC(&msg, size-sizeof(msg.crc));
    return SendMsg(&msg, size);
    }

int protocol_result(void *data, size_t size, struct source_info * si) {
    struct timeval tout;
    tout.tv_sec=1;
    tout.tv_usec=0;
    fd_set ports;
    FD_ZERO (&ports);
    FD_SET (internal_fd, &ports);
    select (FD_SETSIZE, &ports, NULL, NULL, &tout);
    if (!FD_ISSET(internal_fd, &ports)) return -1;
    struct {
        uint8_t src;
        uint8_t trg;
        uint8_t mode;
        uint8_t message_id;
        uint8_t answer_id;
        uint8_t size;
        uint8_t data[255];
	uint8_t crc[2];
	} __attribute__ ((packed)) msg;
    uint8_t prefix=0;
    if (read(internal_fd, &prefix, 1)<=0) return -1;
    if (prefix!=0x7E) return -1;
    if (SerialGetData(&msg, sizeof(msg)-sizeof(msg.data)-sizeof(msg.crc))<0) return -1;
    if (SerialGetData(msg.data, msg.size+sizeof(msg.crc))<0) return -1;
    if (CalcCRC(&msg, sizeof(msg)-sizeof(msg.data)-sizeof(msg.crc)+msg.size)) return -1;
    if (si!=NULL) {
	si->src=msg.src;
	si->trg=msg.trg;
	si->message_id=msg.message_id;
	si->answer_id=msg.answer_id;
	}
    if (data!=NULL && size>0) {
	if (size>msg.size) size=msg.size;
	memcpy(data, msg.data, size);
	}
    if (msg.mode!=1) return 0;
    return 1;
    }
