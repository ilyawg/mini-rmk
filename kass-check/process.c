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

#include "global.h"
#include "localdb.h"
#include "serverdb.h"
#include "log.h"

extern int global_action;

void log_waredb_diff(char * table, int local_cnt, int server_cnt) {
    log_printf(0, "БД операций: различаются таблицы `%s':", table);
    log_printf(1, "  * число записей локальной БД: %d", local_cnt);
    log_printf(1, "  * число записей на сервере: %d", server_cnt);
    }

static int process_ware(int counter) {
    int srv_count=serverdb_wareCount(counter);
    if (srv_count<0) return SERVER_FAIL;
    int local_count=localdb_wareCount();
    if (local_count<0) return WAREDB_FAIL;
    if (srv_count<=local_count) return 0;
    log_waredb_diff("ware", local_count, srv_count);
    return serverdb_processWare(counter);
    }

static int process_barcodes(int counter) {
    int srv_count=serverdb_barcodesCount(counter);
    if (srv_count<0) return SERVER_FAIL;
    int local_count=localdb_barcodesCount();
    if (local_count<0) return WAREDB_FAIL;
    if (srv_count<=local_count) return 0;
    log_waredb_diff("barcodes", local_count, srv_count);
    return serverdb_processBarcodes(counter);
    }

static int process_wareTables(int counter) {
    int err=0;

    int r=process_ware(counter);
    if (r==WAREDB_FAIL) return r;
    if (r<0) err=r;

    r=process_barcodes(counter);
    if (r==WAREDB_FAIL) return r;
    if (r<0) err=r;
    return err;
    }

int process_waredb(int terminal_id) {
    int n=serverdb_getWareCounter(terminal_id);
    if (n<0) return -1;
    int r=process_wareTables(n);
    localdb_closeWareDB();
    if (r!=WAREDB_FAIL) return r;
    log_puts(0, "Ошибка в БД справочников");
    if (localdb_renameWareDB()<0) return -1;
    if (localdb_newWareDB()<0) return -1;
    if (!global_action) return BREAK_PROCESS;
    r=process_wareTables(n);
    localdb_closeWareDB();
    return r;
    }

/*****************************************************************************/
int process_globalseq(int terminal_id, int counter) {
    struct globalseq_dbrecord rec;
    bzero(&rec, sizeof(rec));

    int volumes_pk=serverdb_getVolumesSeq(terminal_id, counter);
    if (volumes_pk<0) return SERVER_FAIL;

    int sessions_pk=serverdb_getSessionsSeq(terminal_id, counter);
    if (sessions_pk<0) return SERVER_FAIL;

    int receipts_pk=serverdb_getReceiptsSeq(terminal_id, counter);
    if (receipts_pk<0) return SERVER_FAIL;
    
    int registrations_pk=serverdb_getRegistrationsSeq(terminal_id, counter);
    if (registrations_pk<0) return SERVER_FAIL;
    
    int ware_quantity_pk=serverdb_getQuantitySeq(terminal_id, counter);
    if (ware_quantity_pk<0) return SERVER_FAIL;
    
    int payments_pk=serverdb_getPaymentsSeq(terminal_id, counter);
    if (payments_pk<0) return SERVER_FAIL;
    
    int n=localdb_getGlobalSeq(&rec);
    if (n<0) return MAINDB_FAIL;
    if (n>0) {
	if (volumes_pk>rec.volumes_pk) {
	    log_printf(0, "БД операций: Неверное значение volumes_pk: %d", rec.volumes_pk);
	    if (localdb_setVolumesSeq(volumes_pk)<0) return MAINDB_FAIL;
	    log_printf(1, "  * исправлено: %d", volumes_pk);
	    }
        if (sessions_pk>rec.sessions_pk) {
	    log_printf(0, "БД операций: Неверное значение sessions_pk: %d", rec.sessions_pk);
	     if (localdb_setSessionsSeq(sessions_pk)<0) return MAINDB_FAIL;
	    log_printf(1, "  * исправлено: %d", sessions_pk);
	    }
	if (receipts_pk>rec.receipts_pk) {
	    log_printf(0, "БД операций: Неверное значение receipts_pk: %d", rec.receipts_pk);
	    if (localdb_setReceiptsSeq(receipts_pk)<0) return MAINDB_FAIL;
	    log_printf(1, "  * исправлено: %d", receipts_pk);
	    }
        if (registrations_pk>rec.registrations_pk) {
	    log_printf(0, "БД операций: Неверное значение registrations_pk: %d", rec.registrations_pk);
	    if (localdb_setRegistrationsSeq(registrations_pk)<0) return MAINDB_FAIL;
	    log_printf(1, "  * исправлено: %d", registrations_pk);
	    }
	if (ware_quantity_pk>rec.ware_quantity_pk) {
	    log_printf(0, "БД операций: Неверное значение ware_quantity_pk: %d", rec.ware_quantity_pk);
	    if (localdb_setQuantitySeq(ware_quantity_pk)<0) return MAINDB_FAIL;
	    log_printf(1, "  * исправлено: %d", ware_quantity_pk);
	    }
	if (payments_pk>rec.payments_pk) {
	    log_printf(0, "БД операций: Неверное значение payments_pk: %d", rec.payments_pk);
	    if (localdb_setPaymentsSeq(payments_pk)<0) return MAINDB_FAIL;
	    log_printf(1, "  * исправлено: %d", payments_pk);
	    }
	return 0;
	}
    log_puts(0, "Отсутствуют значения последовательностей в таблице операций");
    rec.volumes_pk=volumes_pk;
    rec.sessions_pk=sessions_pk;
    rec.receipts_pk=receipts_pk;
    rec.registrations_pk=registrations_pk;
    rec.ware_quantity_pk=ware_quantity_pk;
    rec.payments_pk=payments_pk;
    rec.counter_seq=counter;
    if (localdb_createGlobalSeq(&rec)<0) return MAINDB_FAIL;
    log_puts(1, "  * установлены значения последовательностей");
    return 0;
    }    


/***************************************************************************/
void log_maindb_diff(char * table, int local_cnt, int server_cnt) {
    log_printf(0, "БД операций: различаются таблицы `%s':", table);
    log_printf(1, "  * число записей локальной БД: %d", local_cnt);
    log_printf(1, "  * число записей на сервере: %d", server_cnt);
    }

static void log_info(char * table, int srv_upd, int loc_upd, int loc_del) {
    if (srv_upd) {
	log_printf(1, "  * таблица `%s' на сервере обновлена:", table);
	log_printf(1, "    ** обновлено записей: %d", srv_upd);
	}
    if (loc_upd || loc_del)
	log_printf(1, "  * таблица `%s' локальной БД обновлена:", table);
    if (loc_upd)
	log_printf(1, "    ** обновлено записей: %d", loc_upd);
    if (loc_del)
	log_printf(1, "    ** удалено записей: %d", loc_del);
    }

static int do_sessions(int terminal_id, int counter, struct recordset * rset) {
    if (localdb_getSessions(rset, counter)<0) return MAINDB_FAIL;
    int srv_upd=0, loc_upd=0;
    struct session_dbrecord * rec=recordset_begin(rset);
    while (rec!=NULL) {
	int r=serverdb_addSession(terminal_id, rec);
	if (r<0) return r;
	if (r==SERVERDB_UPDATE) srv_upd++;
	if (r==LOCALDB_UPDATE) loc_upd++;
	rec=recordset_next(rset);
	}
    log_info("sessions", srv_upd, loc_upd, 0);
    return 0;
    }

static int process_sessions(int terminal_id, int counter) {
    int local_count=localdb_getSessionsCount(counter);
    if (local_count<0) return MAINDB_FAIL;
    int server_count=serverdb_getSessionsCount(terminal_id, counter);
    if (server_count<0) return SERVER_FAIL;
    if (local_count==server_count) return 0;
    log_maindb_diff("sessions", local_count, server_count);
    if (local_count<server_count) 
	return serverdb_processSessions(terminal_id, counter);
    struct recordset rset;
    recordset_init(&rset);
    int r=do_sessions(terminal_id, counter, &rset);
    recordset_destroy(&rset);
    return r;
    }

/***************************************************************************/

int process_mainTables(int terminal_id, int counter) {
    int err=0;
    int r=process_globalseq(terminal_id, counter);
    if (r==MAINDB_FAIL) return r;
    if (r<0) err=r;

    r=process_sessions(terminal_id, counter);
//printf("*** process_sessions(): r=%d\n",r);
    if (r==MAINDB_FAIL) return r;
    if (r<0) err=r;

    return err;
    }

static int CreateGlobalSeq(void) {
    struct globalseq_dbrecord rec;
    bzero(&rec, sizeof(rec));
    if (localdb_createGlobalSeq(&rec)<0) return MAINDB_FAIL;
    log_puts(1, "  * значения последовательностей установлены в начальное состояние");
    return 0;
    }

int process_maindb(int terminal_id) {
    int err=0;
    int n=serverdb_getSessionCounter(terminal_id);
    if (n<0) return -1;
    if (n==0) {
	n=localdb_getCounterSeq();
	if (n==DB_EMPTY) return CreateGlobalSeq();
	if (n<0) return n;
	if (serverdb_updateSessionOperID(terminal_id, n)<0) return SERVER_FAIL;
	}
    int r=process_mainTables(terminal_id, n);
    if (r!=MAINDB_FAIL) return r;
    localdb_closeMainDB();
    log_puts(0, "Ошибка в БД операций");
    if (localdb_renameMainDB()<0) return -1;
    if (localdb_newMainDB()<0) return -1;
    if (!global_action) return BREAK_PROCESS;
    r=process_mainTables(terminal_id, n);
    if (r==MAINDB_FAIL) localdb_closeMainDB();
    return r;
    }

/****************************************************************************/
void log_volume_diff(char * table, int volume, int local_cnt, int server_cnt) {
    log_printf(0, "том %d: различаются таблицы `%s':", volume, table);
    log_printf(1, "  * число записей локальной БД: %d", local_cnt);
    log_printf(1, "  * число записей на сервере: %d", server_cnt);
    }

static int do_receipts(int terminal_id, int volume_id, int counter, struct recordset * rset) {
    if (localdb_getReceipts(rset, counter)<0) return VOLUME_FAIL;
    int srv_upd=0, loc_upd=0, loc_del=0;
    struct receipt_dbrecord * rec=recordset_begin(rset);
    while (rec!=NULL) {
	int r=serverdb_addReceipt(terminal_id, volume_id, rec);
	if (r<0) return r;
	if (r==SERVERDB_UPDATE) srv_upd++;
	if (r==LOCALDB_UPDATE) loc_upd++;
	if (r==LOCALDB_DELETE) loc_del++;
	rec=recordset_next(rset);
	}
    log_info("receipts", srv_upd, loc_upd, loc_del);
    return 0;
    }

static int process_receipts(int terminal_id, int volume_id, int counter) {
    int local_count=localdb_getReceiptsCount(counter);
    if (local_count<0) return VOLUME_FAIL;
    int server_count=serverdb_getReceiptsCount(terminal_id, volume_id, counter);
    if (server_count<0) return SERVER_FAIL;
    if (local_count==server_count) return 0;
    log_volume_diff("receipts", volume_id, local_count, server_count);
    int r;
    if (local_count<server_count)
	return serverdb_processReceipts(terminal_id, volume_id, counter);
    struct recordset rset;
    recordset_init(&rset);
    r=do_receipts(terminal_id, volume_id, counter, &rset);
    recordset_destroy(&rset);
    return r;
    }

static int do_registrations(int terminal_id, int volume_id, int counter, struct recordset * rset) {
    if (localdb_getRegistrations(rset, counter)<0) return VOLUME_FAIL;
    int srv_upd=0, loc_upd=0, loc_del=0;
    struct registration_dbrecord * rec=recordset_begin(rset);
    while (rec!=NULL) {
	int r=serverdb_addRegistration(terminal_id, volume_id, rec);
	if (r<0) return r;
	if (r==SERVERDB_UPDATE) srv_upd++;
	if (r==LOCALDB_UPDATE) loc_upd++;
	if (r==LOCALDB_DELETE) loc_del++;
	rec=recordset_next(rset);
	}
    log_info("registrations", srv_upd, loc_upd, loc_del);
    return 0;
    }

static int process_registrations(int terminal_id, int volume_id, int counter) {
    int local_count=localdb_getRegistrationsCount(counter);
    if (local_count<0) return VOLUME_FAIL;
    int server_count=serverdb_getRegistrationsCount(terminal_id, volume_id, counter);
    if (server_count<0) return SERVER_FAIL;
    if (local_count==server_count) return 0;
    log_volume_diff("registrations", volume_id, local_count, server_count);
    if (local_count<server_count)
	return serverdb_processRegistrations(terminal_id, volume_id, counter);
    struct recordset rset;
    recordset_init(&rset);
    int r=do_registrations(terminal_id, volume_id, counter, &rset);
    recordset_destroy(&rset);
    return r;
    }

static int do_ware_quantity(int terminal_id, int volume_id, int counter, struct recordset * rset) {
    if (localdb_getQuantity(rset, counter)<0) return VOLUME_FAIL;
    int srv_upd=0, loc_upd=0, loc_del=0;
    struct quantity_dbrecord * rec=recordset_begin(rset);
    int n=0;
    while (rec!=NULL) {
	int r=serverdb_addQuantity(terminal_id, volume_id, rec);
	if (r<0) return r;
	if (r==SERVERDB_UPDATE) srv_upd++;
	if (r==LOCALDB_UPDATE) loc_upd++;
	if (r==LOCALDB_DELETE) loc_del++;
	rec=recordset_next(rset);
	}
    log_info("ware_quantity", srv_upd, loc_upd, loc_del);
    return 0;
    }

static int process_ware_quantity(int terminal_id, int volume_id, int counter) {
    int local_count=localdb_getQuantityCount(counter);
    if (local_count<0) return VOLUME_FAIL;
    int server_count=serverdb_getQuantityCount(terminal_id, volume_id, counter);
    if (server_count<0) return SERVER_FAIL;
    if (local_count==server_count) return 0;
    log_volume_diff("ware_quantity", volume_id, local_count, server_count);
    if (local_count<server_count)
	return serverdb_processQuantity(terminal_id, volume_id, counter);
    struct recordset rset;
    recordset_init(&rset);
    int r=do_ware_quantity(terminal_id, volume_id, counter, &rset);
    recordset_destroy(&rset);
    return r;
    }

static int do_payments(int terminal_id, int volume_id, int counter, struct recordset * rset) {
    if (localdb_getPayments(rset, counter)<0) return VOLUME_FAIL;
    int srv_upd=0, loc_upd=0, loc_del=0;
    struct payment_dbrecord * rec=recordset_begin(rset);
    while (rec!=NULL) {
	int r=serverdb_addPayment(terminal_id, volume_id, rec);
	if (r<0) return r;
	if (r==SERVERDB_UPDATE) srv_upd++;
	if (r==LOCALDB_UPDATE) loc_upd++;
	if (r==LOCALDB_DELETE) loc_del++;
	rec=recordset_next(rset);
	}
    log_info("payments", srv_upd, loc_upd, loc_del);
    return 0;
    }

static int process_payments(int terminal_id, int volume_id, int counter) {
    int local_count=localdb_getPaymentsCount(counter);
    if (local_count<0) return VOLUME_FAIL;
    int server_count=serverdb_getPaymentsCount(terminal_id, volume_id, counter);
    if (server_count<0) return SERVER_FAIL;
    if (local_count==server_count) return 0;
    log_volume_diff("payments", volume_id, local_count, server_count);
    if (local_count<server_count)
	serverdb_processPayments(terminal_id, volume_id, counter);
    struct recordset rset;
    recordset_init(&rset);
    int r=do_payments(terminal_id, volume_id, counter, &rset);
    recordset_destroy(&rset);
    return r;
    }

static int process_volume_tables(int terminal_id, int volume_id, int counter) {
    int err=0;
    int r=process_receipts(terminal_id, volume_id, counter);
    if (r==VOLUME_FAIL || r==BREAK_PROCESS) return r;
    if (r<0) err=-1;

    r=process_registrations(terminal_id, volume_id, counter);
    if (r==VOLUME_FAIL || r==BREAK_PROCESS) return r;
    if (r<0) err=-1;

    r=process_ware_quantity(terminal_id, volume_id, counter);
    if (r==VOLUME_FAIL || r==BREAK_PROCESS) return r;
    if (r<0) err=-1;

    r=process_payments(terminal_id, volume_id, counter);
    if (r==VOLUME_FAIL || r==BREAK_PROCESS) return r;
    if (r<0) err=-1;
    return r;
    }

static int OpenVolume(int volume_id, int terminal_id, int counter) {
    struct volumes_dbrecord rec;
    bzero(&rec, sizeof(rec));
    rec.volume_id=volume_id;
    rec.volume_name=rec._volume_name;
    int r=localdb_getVolume(&rec);
    // r>0  - есть данные
    // r==0 - нет данных
    // r<0  - ошибка обращения к БД
    if (r<0) return MAINDB_FAIL;
    if (r>0) {
	r=localdb_openVolume(rec._volume_name);
	// r>0  - том открыт успешно
	// r==0 - нет такого файла
	// r<0  - файл тома существует, но не может быть использован
	if (r>0) return 0;
	}
    else if (serverdb_getVolumeRecord(&rec, terminal_id, counter)<0) return SERVER_FAIL;
    // r==0 - нет данных о файле или данные некорректны - попробовать найти файл
    // r<0  - существующий файл не может быть использован - создать новый
    if (r==0) {
	r=localdb_findVolume(volume_id, rec._volume_name);
	if (r<0) return -1;
	if (r>0) r=localdb_openVolume(rec._volume_name);
	}
    // r>0  - том открыт успешно - зарегистрировать
    // r==0 - не найден пригодный для работы файл - создать новый файл
    // r<0  - существующий файл не может быть использован - создать новый файл
    if (r<=0) {
	r=localdb_newVolume(volume_id, rec._volume_name);
	if (r<0) return -1;
	}
    if (localdb_addVolume(&rec)<0) return MAINDB_FAIL;
    return 0;
    }
    
static int do_volumes(struct recordset * rset, int terminal_id, int counter) {
    int local_count=localdb_getVolumesCount(counter);
    if (local_count<0) return MAINDB_FAIL;
    int server_count=serverdb_getVolumesCount(terminal_id, counter);
    if (server_count<0) return SERVER_FAIL;
    if (local_count<server_count) {
	if (serverdb_getVolumes(rset, terminal_id, counter)<0) return SERVER_FAIL;
	}
    else {
	if (localdb_getVolumes(rset, counter)<0) return VOLUMES_FAIL;
	}
    int r;
    int * volume_id=recordset_begin(rset);
    while (volume_id!=NULL) {
	r=OpenVolume(*volume_id, terminal_id, counter);
	if (r<0) return r;
        r=process_volume_tables(terminal_id, *volume_id, counter);
    	localdb_closeVolume();
        if (r==VOLUME_FAIL) {
    	    log_printf(0, "Ошибка в томе %d", *volume_id);
	    char name[201];
	    if (localdb_newVolume(*volume_id, name)<0) return -1;
	    if (localdb_setVolumeName(*volume_id, name)<0) return MAINDB_FAIL;
	    r=process_volume_tables(terminal_id, *volume_id, counter);
    	    localdb_closeVolume();
	    }
	if (r<0) return r;
	volume_id=recordset_next(rset);
	}
    return 0;
    }

int process_volumes(int terminal_id) {
    int n=serverdb_getSessionCounter(terminal_id);
    if (n<0) return -1;
    struct recordset rset;
    recordset_init(&rset);
    int r=do_volumes(&rset, terminal_id, n);
    if (r==MAINDB_FAIL) {
    	localdb_closeMainDB();
        log_puts(0, "Ошибка в БД операций");
	if (localdb_renameMainDB()<0) return -1;
	if (localdb_newMainDB()<0) return -1;
	if (process_maindb(terminal_id)<0) return -1;
	r=do_volumes(&rset, terminal_id, n);
	}
    recordset_destroy(&rset);
    return r;
    }
