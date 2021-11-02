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
#include <gtk/gtk.h>

#include "global.h"
#include "scanner.h"
#include "conf.h"
#include "process.h"

#define SCANNER_PORT	"/dev/ttyS1"
#define SCANNER_BAUD	B9600

#define CONNECT_INTERVAL	5

static int internal_fd=-1;
static GThread * internal_tid=NULL;
static int internal_connect_sid=0;
static GMutex * internal_mutex=NULL;

enum {
    SCANNER_UNLOCK,
    SCANNER_LOCK,
    SCANNER_FLUSH,
    SCANNER_CLEAR
    };
static int internal_scanner_status=SCANNER_LOCK;

int scanner_init(void) {
    internal_mutex=g_mutex_new();
    if (internal_mutex==NULL) return -1;
    return 0;
    }

static int SetAttr(int fd) {
    struct termios tios;
    if (tcgetattr(fd, &tios)<0) return -1;
#ifdef SCANNER_BAUD
    cfsetspeed(&tios, SCANNER_BAUD);
#else
    cfsetspeed(&tios, global_conf_scanner_baud);
#endif
    cfmakeraw(&tios);
    tios.c_cc[VMIN]=0;
    tios.c_cc[VTIME]=1;
    if (tcsetattr(fd, TCSAFLUSH, &tios)<0) return -1;
    int status;
    ioctl(fd, TIOCMGET, &status);
    status&=~TIOCM_RTS;
    ioctl(fd, TIOCMSET, &status);
    return 0;
    }

static int OpenPort(void) {
    g_mutex_lock(internal_mutex);
#ifdef SCANNER_PORT
    int fd=open(SCANNER_PORT, O_RDWR);
#else
    int fd=open(global_conf_scanner_port, O_RDWR);
#endif
    g_mutex_unlock(internal_mutex);
    if (fd<0) return -1;
    if (SetAttr(fd)<0) {
	close(fd);
	return -1;
	}
    internal_fd=fd;
    return fd;
    }

static int SerialRead(void * data, size_t size) {
    uint8_t * ptr=data;
    while (size>0) {
	int r=read(internal_fd, ptr, size);
	if (r<0) return -1;
	if (r==0) break;
	size-=r; ptr+=r;
	}
    return 0;
    }

static void ScannerEvent(char * str) {
    num_t q;
    char trg[32];
    size_t len=strlen(str);
    int i;
    for (i=0; i<len; i++) {
	if (str[i]=='\r') {
	    trg[i]=0;
	    num_init(&q, 0);
	    num_fromint(&q, 1);
	    process_event(BARCODE_EVENT, trg, &q);    
	    break;
	    }
	else trg[i]=str[i];
	}
    }

static void ScannerProcess(void) {
    uint8_t str[32]; bzero(str, sizeof(str));
    fd_set ports;
    int status;

    if (internal_scanner_status!=SCANNER_LOCK) {
        ioctl(internal_fd, TIOCMGET, &status);
        status|=TIOCM_RTS;
        ioctl(internal_fd, TIOCMSET, &status);
	}

    FD_ZERO (&ports);
    while (-1) {
	FD_SET (internal_fd, &ports);
	select (FD_SETSIZE, &ports, NULL, NULL, NULL);
	if (FD_ISSET(internal_fd, &ports)) {
	    if (SerialRead(str, sizeof(str)-1)<0) break;
	    if (internal_scanner_status==SCANNER_UNLOCK) ScannerEvent(str);
	    g_mutex_lock(internal_mutex);
		switch (internal_scanner_status) {
		case SCANNER_CLEAR:
    		    ioctl(internal_fd, TIOCMGET, &status);
    		    status&=~TIOCM_RTS;
    		    ioctl(internal_fd, TIOCMSET, &status);
		    internal_scanner_status=SCANNER_LOCK;
		    break;
		case SCANNER_FLUSH:
		    internal_scanner_status=SCANNER_UNLOCK;
		    break;
		    }
	    g_mutex_unlock(internal_mutex);
	    bzero(str, sizeof(str));
	    }	    
	}
    g_mutex_lock(internal_mutex);
    close(internal_fd);
    internal_fd=-1;
    g_mutex_unlock(internal_mutex);
    }

static gboolean ConnectScanner(void) {
    GError * err=NULL;
    int r=OpenPort();
    if (r>0) {
        internal_tid=g_thread_create((GThreadFunc)ScannerProcess, NULL, TRUE, &err);
	if (internal_tid!=NULL) {
	    internal_connect_sid=0;
	    return FALSE;
	    }
#ifdef DEBUG
        if (err!=NULL) fprintf(stderr,"* g_thread_create failed: %s\n", err->message);
	else fputs("* g_thread_create failed: unknown error\n",stderr);
#endif
	protocol_closePort();
	}	    
    return TRUE;
    }
    
void scanner_start(void) {
    if (internal_connect_sid>0) {
	g_source_remove(internal_connect_sid);
	}
    if (ConnectScanner()) internal_connect_sid=g_timeout_add(CONNECT_INTERVAL*100, (GSourceFunc)ConnectScanner, NULL);
    }

void scanner_enable(int flush) {
puts("*** scanner_enable()");
    int status;
    g_mutex_lock(internal_mutex);
	if (internal_fd>0) {
	    ioctl(internal_fd, TIOCMGET, &status);
	    internal_scanner_status=(flush && status&TIOCM_CTS)?SCANNER_FLUSH:SCANNER_UNLOCK;
	    status|=TIOCM_RTS;
	    ioctl(internal_fd, TIOCMSET, &status);
	    }
	else internal_scanner_status=SCANNER_UNLOCK;
    g_mutex_unlock(internal_mutex);
    }

void scanner_disable(void) {
puts("*** scanner_disable()");
    int status;
    g_mutex_lock(internal_mutex);
	if (internal_fd>0) {
	    ioctl(internal_fd, TIOCMGET, &status);
	    if (status&TIOCM_CTS) {
		status|=TIOCM_RTS;
		internal_scanner_status=SCANNER_CLEAR;
		}
	    else {
		status&=~TIOCM_RTS;
		internal_scanner_status=SCANNER_LOCK;
		}
	    ioctl(internal_fd, TIOCMSET, &status);
	    }
	else internal_scanner_status=SCANNER_LOCK;
    g_mutex_unlock(internal_mutex);
    }
