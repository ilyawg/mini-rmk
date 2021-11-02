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

#include "log.h"
#include "conf.h"
#include "global.h"

static FILE * internal_logfile;

static int internal_errid=0;

int log_open(void) {
    char logfile[1024];
    strcpy(logfile, global_conf_log_dir);
    strcat(logfile, "/check.log");
    internal_logfile=fopen(logfile, "a");
    if (internal_logfile==NULL) {
	perror(logfile);
	fprintf(stderr,"ОШИБКА: Не удалось открыть LOG файл\n");
	return -1;
	}
    return 0;
    }

void log_close(void) {
    fclose(internal_logfile);
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
//	printf("%02d.%02d.%04d %02d:%02d:%02d   %s\n",
		time->tm_mday,
		time->tm_mon+1,
		time->tm_year+1900,
		time->tm_hour,
		time->tm_min,
		time->tm_sec,
		text);
	}
    fflush(internal_logfile);
    }

void log_printf(int mode, char *template, ...) {
    char str[256];
    va_list ap;
    va_start(ap,template);
    vsnprintf(str, sizeof(str), template, ap);
    va_end(ap);
    log_puts(mode, str);
    }

void log_message(int id, ...) {
    if (id==internal_errid) return;
    internal_errid=id;
    char * text;
    int num;
    va_list ap;
    va_start(ap, id);
    switch (id) {
	case WORK_BEGIN:
    	    log_puts(0, "Начало работы...");
	    break;

	case UNKNOWN_TERMINAL:
    	    log_printf(0, "Терминал <%s> не зарегистрирован", va_arg(ap,char*));
	    break;

	case VOLUME_ERROR:
    	    log_printf(0, "Ошибка в томе %d", va_arg(ap, int));
	    break;

	case VOLUME_OPEN_FAIL:
    	    log_printf(0, "Не удалось открыть том %d", va_arg(ap, int));
	    break;

	case VOLUME_CREATED:
    	    log_printf(0, "Создан новый том: <%s>", va_arg(ap,char*));
	    break;

	case VOLUME_CREATEDB_FAIL:
    	    log_printf(0, "Не удалось создать новый том: <%s>", va_arg(ap,char*));
	    break;

	case VOLUME_CREATETBL_FAIL:
    	    log_printf(0, "Не удалось создать структуру таблиц тома <%s>", va_arg(ap,char*));
	    break;

	case VOLUME_REGISTER_FAIL:
    	    log_printf(0, "Не удалось зарегистрировать файл тома <%s>", va_arg(ap, char*));
	    break;

	case VOLUME_NOT_FOUND:
    	    log_printf(0, "Файл тома <%s> не найден", va_arg(ap, char*));
	    break;

	case VOLUME_OPENFILE_FAIL:
    	    log_printf(0, "Не удалось открыть файл тома <%s>", va_arg(ap,char*));
	    break;

	case VOLUME_STRUCT_FAIL:
    	    log_printf(0, "Неверная структура таблиц тома <%s>", va_arg(ap,char*));
	    break;

	case VOLUME_NOT_FIND:
    	    log_printf(0, "Файл тома %d не обнаружен", va_arg(ap, int));
	    break;

	case VOLUME_UNDEF:
    	    log_printf(0, "Том %d не зарегистрирован в БД операций", va_arg(ap, int));
	    break;

	case VOLUMES_FAIL:
    	    log_puts(0, "Проверка томов не завершена");
	    break;

	case OPENDIR_FAIL:
    	    log_printf(0, "Не удалось прочитать содержимое каталога <%s>", va_arg(ap, int));
	    break;

	case VOLUME_FIND:
	    num=va_arg(ap, int);
	    text=va_arg(ap, char*);
    	    log_printf(0, "Обнаружен файл тома %d: <%s>", num, text);
	    break;

	case FORK_FAIL:
    	    log_puts(0, "Не удалось создать процесс:");
	    log_puts(1, va_arg(ap, char*));
	    break;

	case WORK_BREAK:
    	    log_puts(0, "Проверка прервана");
	    break;
	case WORK_END:
    	    log_puts(0, "Проверка завершена");
	    break;

	}
    va_end(ap);
    }
