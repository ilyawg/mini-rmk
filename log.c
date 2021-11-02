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
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#include "log.h"
#include "global.h"

#define LOG_PATH	"/media/work/"

static FILE * internal_control_file;
static int internal_session=0;
static char internal_kkm=0;

static FILE * internal_logfile;
static int internal_errid=0;

static void file_puts(FILE * f, int mode, char * text) {
    if (mode>0) fprintf(f, "                      %s\r\n", text);
    else {
	time_t t=time(NULL);
        struct tm *time=localtime(&t);
	fprintf(f, "%02d.%02d.%04d %02d:%02d:%02d   %s\r\n",
		time->tm_mday,
		time->tm_mon+1,
		time->tm_year+1900,
		time->tm_hour,
		time->tm_min,
		time->tm_sec,
		text);
	}
    fflush(f);
    }

/****************************************************************************/
int log_open(void) {
    internal_logfile=fopen(LOG_FILE, "a");
    if (internal_logfile==NULL) {
	perror(LOG_FILE);
	fprintf(stderr,"ОШИБКА: Не удалось открыть LOG файл\n");
	return -1;
	}
    return 0;
    }

void log_close(void) {
    fclose(internal_logfile);
    }

void log_puts(int mode, char * text) {
    file_puts(internal_logfile, mode, text);
    }

void log_printf(int mode, char *template, ...) {
    char str[256];
    va_list ap;
    va_start(ap,template);
    vsnprintf(str, sizeof(str), template, ap);
    va_end(ap);
    file_puts(internal_logfile, mode, str);
    }

void log_message(int id, ...) {
    if (id==internal_errid) return;
    internal_errid=id;
    va_list ap;
    va_start(ap, id);
    switch (id) {
    case ASSERT:
	log_puts(0, "ASSERT: %s");
	break;
    case DB_CONNECT:
	log_puts(0, "Подключение БД...");
	break;
    case WAREDB_CONNECTED:
	log_puts(0, "БД справочников подключена");
	break;
    case TRANZDB_CONNECTED:
	log_puts(0, "БД операций подключена");
	break;
    case KKM_CONNECT:
	log_puts(0, "Подключение ККМ...");
	break;
    case KKM_DISCONNECT:
	log_puts(0, "ККМ отключена");
	break;
    case OPEN_RECEIPT_FAIL:
	log_puts(0, "Не удалось получить данные по открытому чеку");
	break;
    case CONTROL_FILENAME_FAIL:
	log_puts(0, "Не удалось сформировать имя файла журнала");
	break;
    case CONTROL_OPEN:
	log_printf(0, "Создан новый файл журнала: <%s>", va_arg(ap,char*));
	break;
    case CONTROL_OPEN_FAIL:
	log_puts(0, "Не удалось открыть файл журнала:");
	log_puts(1, va_arg(ap,char*));
	break;
    case EXIT_PROGRAM:
	log_puts(0, "Работа завершена");
	break;
	}	
    va_end(ap);
    }

#ifdef OPERATION_JOURNAL
static char * control_filename(char * path) {
    char * name=strchr(path,0);
    struct stat s;
    int i=0;
    do {
	sprintf(name, "control.%u.log", i++);
	} while (stat(path, &s)==0);
    return name;
    }

static int control_open(void) {
    char path[1024];
    strcpy(path, LOG_PATH);
    char * name=control_filename(path);
    if (name==NULL) {
	log_message(CONTROL_FILENAME_FAIL);
	return -1;
	}
    internal_control_file=fopen(path, "a");
    if (internal_control_file==NULL) {
	log_message(CONTROL_OPEN_FAIL, strerror(errno));
	return -1;
	}
    log_message(CONTROL_OPEN, path);
    return 0;
    }

void control_close(void) {
    if (internal_control_file!=NULL) {
	fclose(internal_control_file);
	internal_control_file=NULL;
	internal_session=0;
	}
    }

static void control_title(char * kkm, int session) {
    char c=0;
    char * ptr=kkm;
    while (*ptr!=0) {
	c^=*ptr;
	ptr++;
	}
    if (internal_kkm==c && session==internal_session) return;
    control_printf(0, "******* ККМ Зав.№%s, смена %d *******", kkm, session);
    internal_kkm=c;
    internal_session=session;
    }

int control_doOpen(char * kkm, int session) {
    if (internal_control_file!=NULL) {
	control_title(kkm, session);
	return 0;
	}
    int r=control_open();
    control_title(kkm, session);
    return r;
    }

void control_puts(int mode, char * text) {
    file_puts(internal_control_file, mode, text);
    }

void control_printf(int mode, char *template, ...) {
    char str[256];
    va_list ap;
    va_start(ap,template);
    vsnprintf(str, sizeof(str), template, ap);
    va_end(ap);
    file_puts(internal_control_file, mode, str);
    }
#endif //OPERATION_JOURNAL
