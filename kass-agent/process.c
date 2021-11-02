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
#include <time.h>

#include "process.h"
#include "global.h"
#include "conf.h"
#include "database_common.h"
#include "localdb.h"
#include "serverdb.h"

#ifdef COMMIT_LOCK
#include <pthread.h>
extern pthread_mutex_t ware_mutex;
#endif

static int WareProcess(int terminal_id, int old_id, int fd) {
    char b;
    int errors=0;
    struct serverSeq_dbrecord rec;
    if (serverdb_getOperationSequence(&rec)<0) return -1;
//printf("old_id=%d\n", old_id); return 0;
    int new_id=0;
//printf("full_clear_counter=%d\n",rec.full_clear_counter);
    if (rec.full_clear_counter>old_id) {
#ifdef COMMIT_LOCK
	pthread_mutex_lock(&ware_mutex);
#endif
	b=ST_LOCK; write(fd, &b, 1);
	if (localdb_clearAll()<0) errors++;
#ifdef COMMIT_LOCK
	pthread_mutex_unlock(&ware_mutex);
#endif
	b=ST_ACTION; write(fd, &b, 1);
	new_id=rec.full_clear_counter;
	}

    if (rec.clear_counter>old_id && rec.clear_counter>new_id) {
#ifdef COMMIT_LOCK
	pthread_mutex_lock(&ware_mutex);
#endif
	b=ST_LOCK; write(fd, &b, 1);
	if (localdb_markAll()<0) errors++;
#ifdef COMMIT_LOCK
	pthread_mutex_unlock(&ware_mutex);
#endif
	b=ST_ACTION; write(fd, &b, 1);
	new_id=rec.clear_counter;
	}

    if (rec.update_counter>old_id && rec.update_counter>=new_id) {
	int first_id=old_id+1;
	if (new_id>first_id) first_id=new_id;
	if (!errors && serverdb_updateWare(first_id, fd)<0) errors++;
	if (!errors && serverdb_updateBarcode(first_id, fd)<0) errors++;
	if (!errors && localdb_clearMarked()<0) errors++;
	new_id=rec.update_counter;
	}

    if (new_id>old_id) {
	if (serverdb_updateWareOperID(terminal_id, new_id)<0) return -1;
	}
    return errors;
    }

/*****************************************************************************/
static int ProcessSessions(int terminal_id, int first_id, int last_id) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_processSessions(&rset, first_id, last_id)<0) {
	recordset_destroy(&rset);
	return -1;
	}
    struct session_dbrecord * rec=recordset_begin(&rset);
    while (rec!=NULL) {
	if (serverdb_addSession(terminal_id, rec)<0) {
	    recordset_destroy(&rset);
	    return -1;
	    }
	rec=recordset_next(&rset);
	}    
    recordset_destroy(&rset);
    return 0;
    }
    
static int ProcessReceipts(int terminal_id, int volume_id, int first_id, int last_id) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_processReceipts(&rset, first_id, last_id)<0) {
//puts(" ### processReceipts<0");
        recordset_destroy(&rset);
	return -1;
	}
    struct receipt_dbrecord * rec=recordset_begin(&rset);
    while (rec!=NULL) {
	if (serverdb_addReceipt(terminal_id, volume_id, rec)<0) {
//puts(" ### serverdb_addReceipt()<0");
	    recordset_destroy(&rset);
	    return -1;
	    }
	rec=recordset_next(&rset);
	}    
    recordset_destroy(&rset);
    return 0;
    }

static int ProcessRegistrations(int terminal_id, int volume_id, int first_id, int last_id) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_processRegistrations(&rset, first_id, last_id)<0) {
//puts(" ### localdb_processRegistrations()<0");
        recordset_destroy(&rset);
	return -1;
	}
    struct registration_dbrecord * rec=recordset_begin(&rset);
    while (rec!=NULL) {
	if (serverdb_addRegistration(terminal_id, volume_id, rec)<0) {
//puts(" ### serverdb_addRegistration()<0");
	    recordset_destroy(&rset);
	    return -1;
	    }
	rec=recordset_next(&rset);
	}    
    recordset_destroy(&rset);
    return 0;
    }

static int ProcessQuantity(int terminal_id, int volume_id, int first_id, int last_id) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_processQuantity(&rset, first_id, last_id)<0) {
//puts(" ### localdb_processQuantity()<0");
        recordset_destroy(&rset);
	return -1;
	}
    struct quantity_dbrecord * rec=recordset_begin(&rset);
    while (rec!=NULL) {
	if (serverdb_addQuantity(terminal_id, volume_id, rec)<0) {
//puts(" ### serverdb_addQuantity()<0");
	    recordset_destroy(&rset);
	    return -1;
	    }
	rec=recordset_next(&rset);
	}    
    recordset_destroy(&rset);
    return 0;
    }

static int ProcessPayments(int terminal_id, int volume_id, int first_id, int last_id) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_processPayments(&rset, first_id, last_id)<0) {
        recordset_destroy(&rset);
	return -1;
	}
    struct payment_dbrecord * rec=recordset_begin(&rset);
    while (rec!=NULL) {
	if (serverdb_addPayment(terminal_id, volume_id, rec)<0) {
	    recordset_destroy(&rset);
	    return -1;
	    }
	rec=recordset_next(&rset);
	}    
    recordset_destroy(&rset);
    return 0;
    }

static int ProcessVolumeTables(int terminal_id, int volume_id, int first_id, int new_id) {
    if (ProcessReceipts(terminal_id, volume_id, first_id, new_id)<0) return -1;
    if (ProcessRegistrations(terminal_id, volume_id, first_id, new_id)<0) return -2;
    if (ProcessQuantity(terminal_id, volume_id, first_id, new_id)<0) return -3;
    if (ProcessPayments(terminal_id, volume_id, first_id, new_id)<0) return -4;
    return 0;
    }

static int ProcessVolumes(int terminal_id, struct recordset * rset, int first_id, int last_id) {
    if (localdb_getVolumes(rset, first_id, last_id)<0) return -1;

    struct volume_dbrecord * rec=recordset_begin(rset);
    while (rec!=NULL) {
	if (localdb_openVolume(rec)<0) return -1;
        int r=ProcessVolumeTables(terminal_id, rec->volume_id, first_id, last_id);
//printf("### processVolumeTables(): r=%d\n",r);
	localdb_closeVolume();
	if (r<0) return -1;
    	rec=recordset_next(rset);
	}
    return 0;
    }


static int SessionProcess(int terminal_id, int old_id, int new_id) {
    int first_id=old_id+1;
    if (ProcessSessions(terminal_id, first_id, new_id)<0) return -1;
    struct recordset rset;
    recordset_init(&rset);
    int r=ProcessVolumes(terminal_id, &rset, first_id, new_id);
    recordset_destroy(&rset);
    if (r<0) return -1;
    if (serverdb_updateSessionOperID(terminal_id, new_id)<0) return -1;
    return new_id;
    }    

/*****************************************************************************/
static int TerminalValues(struct terminals_dbrecord * rec) {
    int id=serverdb_getTerminalID(rec->name);
    if (id<=0) return -1;
    rec->terminal_id=id;
    return serverdb_getTerminalValues(rec);
    }

void exchange_thread(struct thread_vars * vars) {
//puts("+++ ExchangeThread() +++");
    char b;
    if (serverdb_connect()<0) {
//        log_puts(0, "Подключение к серверу не выполнено");
	vars->ware_exec=0;
	b=ST_UNAVAILABLE; write(vars->pipe[1], &b, 1);
	}
    else {
	b=ST_ACTION; write(vars->pipe[1], &b, 1);
	struct terminals_dbrecord term;
	term.name=global_conf_terminal_name;
	if (TerminalValues(&term)<0) {
	    log_puts(0, "ОШИБКА: нет данных о терминале на сервере");
	    }
	else {
    	    if (term.session_counter>0 && vars->client_counter>term.session_counter) {
		int n=SessionProcess(term.terminal_id, term.session_counter, vars->client_counter);
		if (n<0) log_puts(0, "синхронизация отчетов не выполнена");
		if (n>0) vars->session_counter=n;
		}
	    else vars->session_counter=term.session_counter;

//	    time_t t=time(NULL);
//	    if (t-vars->time>=global_conf_time_interval) {
//		vars->time=t;
	    if (vars->ware_exec) {
		vars->ware_exec=0;
		if (WareProcess(term.terminal_id, term.ware_counter, vars->pipe[1])!=0)
    		    log_puts(0, "синхронизация справочников не выполнена");
		}
	    }
	serverdb_disconnect();
	b=ST_DONE; write(vars->pipe[1], &b, 1);
	}
    vars->exec=0;
//puts("--- ExchangeThread() ---");
    }
