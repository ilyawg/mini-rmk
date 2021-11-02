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

void protocol_clear(void) {
//puts("* protocol_clear()");
    tcflush(internal_fd, TCIFLUSH);
    }

static int RecvByte(void) {
//puts("* RecvByte()");
    uint8_t b;
    if (read(internal_fd, &b, 1)<1) return -1;
//printf("RecvByte: %02x\n", b);
    return b;
    }

static void SendByte(int byte) {
//printf("SendByte: %02x\n", byte);
    uint8_t b=byte;
    write(internal_fd, &b, 1);
    }

#define SEND_ENQ	SendByte(0x05)
#define SEND_ACK	SendByte(0x06)
#define SEND_NAK	SendByte(0x15)

static int CalcCRC(void * data, size_t size) {
    uint8_t crc=0;
    uint8_t * ptr=data;
    while (size--) crc^=*ptr++;
//printf("crc: %02x\tcalc:%02x\n", *ptr, crc);
    if (*ptr==crc) return 0;
    *ptr=crc;
    return -1;
    }

static int SerialRecv(void * data, size_t size) {
    int count=0;
    while (count<size) {
	int r=read(internal_fd, (uint8_t*)data+count, size-count);
	if (r<=0) return -1;
	count+=r;
	}
//print_hex(data, count);
    return count;
    }

int protocol_send(void * data, size_t size) {
    enum {PREFIX=0, SIZE, DATA};
    uint8_t buf[260];
#ifdef DEBUG
    if (size<1 || size>255) {
	fprintf(stderr, "*** protocol_send() ASSERT: bad msg size: %d\n", size);
	return -1;
	}
#endif /* DEBUG */
    buf[PREFIX]=0x02;
    buf[SIZE]=size;
    memcpy(&buf[DATA], data, size);
    CalcCRC(&buf[SIZE], size+1);
    int i;
    for (i=0; i<2; i++) {
	if (SerialSend(buf, size+3)<0) break;
	int r=RecvByte();
	if (r<0) r=RecvByte();
	if (r==0x06) return 0;
	if (r!=0x15) break;
	}
    return -1;
    }

int protocol_recv(void * data, size_t size, int t) {
//puts("* protocol_recv()");
    enum {PREFIX=0, SIZE, DATA};
    uint8_t buf[260];
    struct timeval tout;
    fd_set ports;
    tout.tv_sec=t/10;
    tout.tv_usec=100000*(t%10);
    FD_ZERO (&ports);
    FD_SET (internal_fd, &ports);
    select (FD_SETSIZE, &ports, NULL, NULL, &tout);
    if (!FD_ISSET(internal_fd, &ports)) return -1;
    if (SerialRecv(buf, 2)<0 || buf[PREFIX]!=0x02) return -1;
    if (SerialRecv(&buf[DATA], buf[SIZE]+1)<0) return -1;
    if (CalcCRC(&buf[SIZE], buf[SIZE]+1)<0) {
#ifdef DEBUG
	fputs("*** protocol_recv() ERROR: bad CRC", stderr);
#endif /* DEBUG */
	SEND_NAK;
	return -1;
	}
//print_hex(&buf[DATA], buf[SIZE]);
    SEND_ACK;
    if (buf[SIZE]<size) size=buf[SIZE];
    if (size>0) memcpy(data, &buf[DATA], size);
    return size;
    }

int protocol_start(void) {
    uint8_t b=0x05;
    struct timeval tout;
    fd_set ports;
    int i;
    for (i=0; i<2; i++) {
//	tout.tv_sec=10;
	tout.tv_sec=1;
	tout.tv_usec=0;
	FD_ZERO (&ports);
	FD_SET (internal_fd, &ports);
	tcflush(internal_fd, TCIFLUSH);
	SEND_ENQ;
	select (FD_SETSIZE, &ports, NULL, NULL, &tout);
	if (!FD_ISSET(internal_fd, &ports)) return -1;
	int r=RecvByte();
	if (r==0x15) return 0;
	if (r!=0x06) return -1;
	protocol_recv(NULL, 0, 1);
	}
    return -1;
    }
