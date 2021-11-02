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
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dbus/dbus.h>

#include "conf.h"
#include "log.h"
#include "process.h"
#include "global.h"

#ifndef CONFIG_FILE
#define CONFIG_FILE	"/usr/local/etc/mini-rmk/kass_srv.conf"
#endif

typedef void * (*thread_func)(void*);

int * global_stop_flag=NULL;

static const char * dbus_interface_name="mrmk.agent";

#if defined WARE_PROCESS_LOCK || defined COMMIT_LOCK
pthread_mutex_t ware_mutex=PTHREAD_MUTEX_INITIALIZER;
#endif

/****************************************************************************/
struct message_vars {
    int counter;
    char * status;
#if defined WARE_PROCESS_LOCK || defined COMMIT_LOCK
    int lock;
#endif
    };

static DBusHandlerResult MessageHandler(DBusConnection * conn, DBusMessage *msg, void * data) {
//puts("* MessageHandler()");
    struct message_vars * vars=data;
//printf("\t * member: <%s>\n", dbus_message_get_member(msg));
#if defined WARE_PROCESS_LOCK || defined COMMIT_LOCK
    if (dbus_message_is_method_call(msg, dbus_interface_name, "lock")) {
	vars->lock=1;
	return DBUS_HANDLER_RESULT_HANDLED;
	}
    if (dbus_message_is_method_call(msg, dbus_interface_name, "unlock")) {
	vars->lock=0;
	return DBUS_HANDLER_RESULT_HANDLED;
	}
#endif	
    if (dbus_message_is_method_call(msg, dbus_interface_name, "status")) {
        DBusMessage * reply=dbus_message_new_method_return(msg);
	if (reply!=NULL) {
	    dbus_message_append_args(reply,
				     DBUS_TYPE_STRING, &vars->status,
				     DBUS_TYPE_INVALID);
	    dbus_connection_send(conn, reply, NULL);
	    dbus_connection_flush(conn);
	    dbus_message_unref(reply);
	    }
	return DBUS_HANDLER_RESULT_HANDLED;
	}
    if (dbus_message_is_signal(msg, dbus_interface_name, "notify")) {
	char * oper_type=NULL;
	int counter=0;
        DBusError error;
	dbus_error_init(&error);
	dbus_message_get_args(msg,
			      &error,
			      DBUS_TYPE_STRING, &oper_type,
			      DBUS_TYPE_UINT32, &counter,
			      DBUS_TYPE_INVALID);
	if (dbus_error_is_set(&error)) {
	    log_printf(0, "ОШИБКА D-Bus: %s\n", error.message);
    	    dbus_error_free(&error);
    	    }
	else if (oper_type!=NULL && counter>vars->counter) {
		vars->counter=counter;
		if (strcmp(oper_type, "CASH")==0
	    	    || strcmp(oper_type, "RECEIPT")==0
	    	    || strcmp(oper_type, "SESSION")==0)
			vars->lock=0;
	    }
	return DBUS_HANDLER_RESULT_HANDLED;
	}
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

/****************************************************************************/
struct watch_list {
    DBusWatch * read;
//    DBusWatch * write;
    };

static dbus_bool_t AddWatch(DBusWatch *watch, void * data) {
//puts("* add_watch()");
    struct watch_list * w=data;
    int f=dbus_watch_get_flags(watch);
    if (f & DBUS_WATCH_READABLE) w->read=watch;
//    if (f & DBUS_WATCH_WRITABLE) w->write=watch;
    return TRUE;
    }
 
static void RemoveWatch(DBusWatch *watch, void *data) {
//puts("* remove_watch()");
    struct watch_list * w=data;
    int f=dbus_watch_get_flags(watch);
    if (f & DBUS_WATCH_READABLE) w->read=NULL;
//    if (f & DBUS_WATCH_WRITABLE) w->write=watch;
    }

static void ToggleWatch(DBusWatch *watch, void *data) {
//puts("* toggle_watch()");
    } 

static int ProcessStatus(DBusConnection * conn, int fd, struct message_vars * vars) {
    char st=0;
    const char * str;
    if (read(fd, &st, 1)<1) return ST_UNDEF;
    switch(st) {
    case ST_UNAVAILABLE:	str="UNAVAILABLE"; break;
    case ST_ACTION: 		str="ACTION"; break;
    case ST_DONE: 		str="DONE"; break;
#if defined WARE_PROCESS_LOCK || defined COMMIT_LOCK
    case ST_LOCK: 		str="LOCK"; break;
#endif
    default:			str="UNDEF"; break;
        }
    if (vars->status!=str) {
        DBusMessage * msg=dbus_message_new_signal("/", dbus_interface_name, "status");
        if (msg==NULL) log_puts(0, "ОШИБКА D-bus: не удалось создать сообщение");
        else {
	    dbus_message_append_args(msg, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);
	    dbus_connection_send(conn, msg, NULL);
	    dbus_connection_flush(conn);
	    dbus_message_unref(msg);
	    }
	vars->status=(char*)str;
	}
    return st;
    }

static void MainLoop(DBusConnection * conn) {
    const int timeout=global_conf_time_interval*1000;
    pthread_t thread;
    int stop_flag=FALSE; global_stop_flag=&stop_flag;
    struct thread_vars thv;
    thv.locked=FALSE;
    thv.session_counter=0;
    thv.client_counter=0;
    thv.time=0;
    thv.exec=FALSE;
    thv.ware_exec=TRUE;
    pipe(thv.pipe);

/**********************************************/
    struct message_vars msgv;
    msgv.status="UNDEF";
    dbus_connection_add_filter(conn, MessageHandler, &msgv, NULL);

    struct watch_list watch;
    dbus_connection_set_watch_functions(conn, AddWatch, RemoveWatch, ToggleWatch, &watch, NULL);

    fd_set fds;
    struct timeval tout;
    tout.tv_sec=global_conf_time_interval;
    tout.tv_usec=0;
    struct timeval * t=&tout;
    log_puts(0, "Агент запущен");
    while (!stop_flag) {
	msgv.counter=-1;
#if defined WARE_PROCESS_LOCK || defined COMMIT_LOCK
	msgv.lock=-1;
#endif
	int r=dbus_connection_get_dispatch_status(conn);
	while (r==DBUS_DISPATCH_DATA_REMAINS) r=dbus_connection_dispatch(conn);
	if (r!=DBUS_DISPATCH_COMPLETE) puts("*** DBUS_DISPATCH FAIL");

#if defined WARE_PROCESS_LOCK || defined COMMIT_LOCK
	if (msgv.lock==0 && thv.locked) {
	    thv.locked=FALSE;
	    pthread_mutex_unlock(&ware_mutex);
	    }
	else if (msgv.lock==1 && !thv.locked) {
	    thv.locked=TRUE;
	    pthread_mutex_lock(&ware_mutex);
	    }
#endif
	if (msgv.counter>thv.client_counter) thv.client_counter=msgv.counter;
	if ((thv.ware_exec || msgv.counter>=0) && !thv.exec) {
	    if (msgv.counter<0) {
		int counter=localdb_getCounter();
		if (counter<0) log_puts(0, "Не удалось получить значение локального счетчика операций");
		if (counter>thv.client_counter) thv.client_counter=counter;
		}

	    if (thv.client_counter>thv.session_counter || thv.ware_exec) {
		thv.exec=TRUE;
		if (pthread_create(&thread, NULL, (thread_func)exchange_thread, &thv)==0) {
	    	    pthread_detach(thread);
		    t=NULL;
	    	    }
		else {
	    	    log_puts(0, "ОШИБКА: Не удалось создать поток");
		    thv.exec=FALSE;
	    	    }
		}
	    }

        FD_ZERO (&fds);
        FD_SET(thv.pipe[0], &fds);
	if (watch.read!=NULL) FD_SET(dbus_watch_get_fd(watch.read), &fds);

	r=select(FD_SETSIZE, &fds, NULL, NULL, t);
	if (tout.tv_sec<1) {
	    tout.tv_sec=global_conf_time_interval;
    	    tout.tv_usec=0;
	    thv.ware_exec=TRUE;
	    }

	if (r>0) {
	    if (watch.read!=NULL && FD_ISSET(dbus_watch_get_fd(watch.read), &fds))
            	dbus_watch_handle(watch.read, DBUS_WATCH_READABLE);
	
	    if (FD_ISSET(thv.pipe[0], &fds)) {
		r=ProcessStatus(conn, thv.pipe[0], &msgv);
		if (r==ST_UNAVAILABLE || r==ST_DONE) t=&tout;
		}
	    }
	}
    if (thv.exec) pthread_join(thread, NULL);
    close(thv.pipe[0]);
    close(thv.pipe[1]);
    dbus_connection_unref(conn);
    localdb_closeMain();
    serverdb_destroy();
    log_close();
    conf_clear();
    dbexit();
    }

/****************************************************************************/
static DBusConnection * ConnectDbus(void) {
    char text[1024];
    DBusError error;
    dbus_error_init(&error);
    DBusConnection * bus=dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
	sprintf(text, "ОШИБКА D-Bus: %s", error.message);
	log_puts(1, text);
	fputs(text, stderr);
	fputc('\n', stderr);
        dbus_error_free(&error);
        return NULL;
        }

    int r=dbus_bus_request_name(bus, dbus_interface_name, 0, &error);
    if (dbus_error_is_set(&error)) {
	sprintf(text, "ОШИБКА: D-Bus: %s", error.message);
	log_puts(1, text);
	fputs(text, stderr);
	fputc('\n', stderr);
        dbus_error_free(&error);
        return NULL;
        }
    
    if (r!=DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
	char * s="ОШИБКА: Агент уже запущен";
	log_puts(1, s);
	fputs(s, stderr);
	fputc('\n', stderr);
        dbus_error_free(&error);
        return NULL;
        }
    return bus;
    }

static int InitDB(void) {
    serverdb_init();
    if (localdb_openMain()<0) {
	char * s="ОШИБКА: локальная БД недоступна";
	log_puts(1, s);
	fputs(s, stderr);
	fputc('\n', stderr);
	serverdb_destroy();
	return -1;
	}
    
    log_puts  (1, "****************** Параметры *******************");
    log_printf(1, "каталог локальной БД:  %s", global_conf_localdb_dir);
    log_printf(1, "сервер: .............. %s", global_conf_conn_alias);
    log_printf(1, "пользователь: ........ %s", global_conf_srv_login);
    log_printf(1, "база данных: ......... %s", global_conf_srv_base);
    log_printf(1, "период опроса сервера: %u", global_conf_time_interval);
    log_printf(1, "терминал: ............ %s", global_conf_terminal_name);
    log_puts  (1, "************************************************");
    return 0;
    }

static void term_handler(void) {
    if (global_stop_flag!=NULL) *global_stop_flag=TRUE;
    }

int main(void) {
    if (conf_read(CONFIG_FILE)<0) {
	fputs("ОШИБКА: файл конфигурации недоступен\n",stderr);
	return -1;
	}
    int err=0;
    if (global_conf_conn_alias==NULL) {
	fputs("ОШИБКА: не указано имя сервера\n", stderr);
	err++;
	}
    if (global_conf_srv_login==NULL) {
	fputs("ОШИБКА: не указано имя пользователя сервера\n", stderr);
	err++;
	}
    if (global_conf_srv_psw==NULL) {
	fputs("ОШИБКА: не указан пароль пользователя сервера \n", stderr);
	err++;
	}
    if (global_conf_srv_base==NULL) {
	fputs("ОШИБКА: не указана база данных сервера \n", stderr);
	err++;
	}
    if (global_conf_terminal_name==NULL) {
	fputs("ОШИБКА: не указано имя терминала\n", stderr);
	err++;
	}
    if (err>0) {
	conf_clear();
	return -1;
	}	
    
    puts  ("****************** Параметры *******************");
    printf("log-файл: ............ %s/agent.log\n", global_conf_log_dir);
    printf("каталог локальной БД:  %s\n", global_conf_localdb_dir);
    printf("сервер: .............. %s\n", global_conf_conn_alias);
    printf("пользователь: ........ %s\n", global_conf_srv_login);
    printf("база данных: ......... %s\n", global_conf_srv_base);
    printf("период опроса сервера: %u\n", global_conf_time_interval);
    printf("терминал: ............ %s\n", global_conf_terminal_name);
    puts  ("************************************************");
    
    if (log_open()<0) {
	conf_clear();
	return -1;
	}
    log_puts(0, "Запуск агента...");

    DBusConnection * conn=ConnectDbus();
    if (conn==NULL) {
	log_close();
	conf_clear();
	return -1;
	}

    if (InitDB()<0) {
	log_close();
	conf_clear();
	return -1;
	}
    
    signal(SIGTERM, (__sighandler_t)term_handler);

#ifndef DEBUG    
    int r=fork();
    if (r==0) {
	setsid();
	chdir("/");
#ifdef PATCHED_TDS_PERROR
	close(0);
	close(1);
	close(2);
#endif
	MainLoop(conn);
        log_puts(0, "Агент остановлен");
	}
    if (r==-1) {
	char * s1="ОШИБКА: Не удалось создать процесс:";
	char s2[1024];
	strcpy(s2, "\t");
	strcat(s2, strerror(errno));
	log_puts(1, s1);
	fputs(s1, stderr); fputc('\n', stderr);
	log_puts(1, s2);
	fputs(s2, stderr); fputc('\n', stderr);
	}
    dbus_connection_unref(conn);
    localdb_closeMain();
    serverdb_destroy();
    log_close();
    conf_clear();
    dbexit();
#else
    MainLoop(conn);
#endif
    return 0;
    }
