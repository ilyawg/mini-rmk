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
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "log.h"
#include "conf.h"
#include "global.h"

static FILE * internal_logfile;

static char * internal_buf=NULL;

int log_open(void) {
    char logfile[1024];
    strcpy(logfile, global_conf_log_dir);
    strcat(logfile, "/agent.log");
    internal_logfile=fopen(logfile, "a");
    if (internal_logfile==NULL) {
	perror(logfile);
	fprintf(stderr,"ОШИБКА: Не удалось открыть LOG файл\n");
	return -1;
	}
    internal_buf=malloc(256);
    bzero(internal_buf, 256);
    return 0;
    }

void log_close(void) {
    fclose(internal_logfile);
    if (internal_buf!=NULL) free(internal_buf);
    internal_buf=NULL;
    }

void log_puts(int mode, char * text) {
#ifdef DEBUG
    printf("=== %s\n", text);
#endif
    if (mode>0) fprintf(internal_logfile, "                      %s\r\n", text);
    else {
	time_t t=time(NULL);
        struct tm *time=localtime(&t);
	fprintf(internal_logfile, "%02d.%02d.%04d %02d:%02d:%02d   %s\r\n",
		time->tm_mday,
		time->tm_mon+1,
		time->tm_year+1900,
		time->tm_hour,
		time->tm_min,
		time->tm_sec,
		text);
	}
    fflush(internal_logfile);
    bzero(internal_buf, 256);
    size_t len=strlen(text);
    if (len>255) len=255;
    memcpy(internal_buf, text, len);
    }

void log_printf(int mode, char *template, ...) {
    char str[256];
    va_list ap;
    va_start(ap,template);
    vsnprintf(str, sizeof(str), template, ap);
    va_end(ap);
    log_puts(mode, str);
    }

void log_message(int mode, char *template, ...) {
    char str[256];
    va_list ap;
    va_start(ap,template);
    vsnprintf(str, sizeof(str), template, ap);
    va_end(ap);
    if (strcmp(internal_buf, str)!=0) log_puts(mode, str);
    }
