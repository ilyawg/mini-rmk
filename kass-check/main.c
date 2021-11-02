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
#include <signal.h>
#include <errno.h>

#include "conf.h"
#include "serverdb.h"
#include "localdb.h"
#include "global.h"
//#include "process.h"
#include "log.h"

#ifndef CONFIG_FILE
#define CONFIG_FILE	"/usr/local/etc/mini-rmk/test_srv.conf"
#endif

#ifndef FALSE
#define FALSE	0
#define TRUE	!FALSE
#endif

int global_action=TRUE;

static int check_localdb(void) {
    int r=localdb_checkMainDB();
    if (r<0) return r;

    if (r==0 && localdb_newMainDB()<0) return -1;

    if (!global_action) return BREAK_PROCESS;
    
    r=localdb_checkWareDB();
    if (r<0) return r;

    if (r==0 && localdb_newWareDB()<0) return -1;

    if (!global_action) return BREAK_PROCESS;
    return 0;
    }

static int check_config(void) {
    if (global_conf_conn_alias==NULL) {
	fputs("ОШИБКА: не указано имя сервера\n", stderr);
	return -1;
	}

    if (global_conf_srv_login==NULL) {
	fputs("ОШИБКА: не указано имя пользователя сервера\n", stderr);
	return -1;
	}

    if (global_conf_srv_psw==NULL) {
	fputs("ОШИБКА: не указан пароль пользователя сервера \n", stderr);
	return -1;
	}

    if (global_conf_srv_base==NULL) {
	fputs("ОШИБКА: не указана база данных сервера \n", stderr);
	return -1;
	}

    if (global_conf_terminal_name==NULL) {
	fputs("ОШИБКА: не указано имя терминала\n", stderr);
	return -1;
	}
    return 0;
    }

static void main_process(void) {
    serverdb_doConnect();
    int terminal_id=serverdb_getTerminalID(global_conf_terminal_name);
    if (terminal_id<=0) log_message(UNKNOWN_TERMINAL, global_conf_terminal_name);
    else {
	int r=process_waredb(terminal_id);
	if (r==BREAK_PROCESS) log_message(WORK_BREAK);
	else {
	    if (r<0) log_puts(0, "Ошибка в БД справочников");
	    r=process_maindb(terminal_id);
	    if (r==BREAK_PROCESS) log_message(WORK_BREAK);
	    else {
		if (r<0) log_puts(0, "Ошибка в БД операций");
		else {
		    r=process_volumes(terminal_id);
		    if (r==BREAK_PROCESS) log_message(WORK_BREAK);
		    else {
			if (r<0) log_message(VOLUMES_FAIL);
			else log_message(WORK_END);
			}
		    }
		}
	    }
		    
	}
    serverdb_disconnect();
    }

void term_handler(int i) {
    global_action=0;
    }

int main(void) {
    if (conf_read(CONFIG_FILE)<0) {
	fputs("ОШИБКА: файл конфигурации недоступен\n",stderr);
	return -1;
	}

    if (check_config()<0) {
	conf_clear();
	return -1;
	}

    if (log_open()<0) {
	conf_clear();
	return -1;
	}

    signal(SIGTERM,term_handler);
    log_message(WORK_BEGIN);
    log_puts  (1, "****************** Параметры *******************");
    log_printf(1, "log-файл: ............ %s/check.log", global_conf_log_dir);
    log_printf(1, "каталог локальной БД:  %s", global_conf_localdb_dir);
    log_printf(1, "сервер: .............. %s", global_conf_conn_alias);
    log_printf(1, "пользователь: ........ %s", global_conf_srv_login);
    log_printf(1, "база данных: ......... %s", global_conf_srv_base);
    log_printf(1, "терминал: ............ %s", global_conf_terminal_name);
    log_puts  (1, "************************************************");

    serverdb_init();
    int r=check_localdb();
    if (r==BREAK_PROCESS) log_message(WORK_BREAK);
    if (r==0) {
#ifndef DEBUG    
	r=fork();
	if (r==0) {
	    setsid();
	    chdir("/");
#ifdef PATCHED_TDS_PERROR
	    close(0);
	    close(1);
	    close(2);
#endif
	    main_process();
	    }
	if (r==-1) log_message(FORK_FAIL, strerror(errno));
#else
	main_process();
#endif
	}    
    localdb_closeAll();
    log_close();
    conf_clear();
    dbexit();
    return 0;
    }
