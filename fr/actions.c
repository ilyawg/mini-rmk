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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "conf.h"
#include "global.h"
#include "actions.h"
#include "process.h"
#include "kkm.h"
#include "receipt.h"
#include "localdb.h"
#include "../numeric.h"
#include "main_window.h"

//struct tm oper_date_time;
//int oper_check_num;
//int oper_doc_num;
//int oper_check_type;
//int oper_payment_type;

static int internal_receipt_id=-1;
static int internal_session_id=-1;
static int internal_volume_id=-1;
static int internal_volume_size=0;

/****************************************************************************/
//static int oper_code;
//static num_t oper_summ;
/****************************************************************************/
static int NewVolume(int id, char * kkm) {
    char path[1024];
    strcpy(path, global_conf_db_dir);
    strcat(path, "/");
    char * name=strchr(path,0);
    struct stat s;
    int i=0;
    sprintf(name, "vol%05u.ldb", id);
    while (stat(path, &s)==0 && s.st_size>0) 
	sprintf(name, "vol%05u.%u.ldb", id, i++);
#ifdef DEBUG
    printf ("- NewVolume(): <%s>\n", path);
#endif
    if (localdb_openVolume(path)<0) return -1;
    if (localdb_createVolume(kkm, id, name)<0) {
	localdb_closeVolume();
	unlink(path);
	return -1;
	}
    return 0;
    }

/************************** code_process() **********************************/
int actions_processRegistration(struct record_receipt * rc, int n_session, int n_check, int n_doc, int check_type) {
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;					//FIXME!!!
    struct registration_dbrecord registration_rec;
    rc->id=++srec.registrations_pk;
    registration_rec.registration_id=rc->id;
    if (internal_receipt_id<0) internal_receipt_id=++srec.receipts_pk;
    registration_rec.receipt_id=internal_receipt_id;
    registration_rec.n_kkm=registration_rec._n_kkm; kkm_getNum(registration_rec._n_kkm);
    registration_rec.n_session=n_session;
    registration_rec.n_check=n_check;
    registration_rec.n_doc=n_doc;
    registration_rec.check_type=check_type;
    registration_rec.n_position=rc->num;
    registration_rec.ware_id=rc->code;
    if (strlen(rc->barcode)>0) {
	registration_rec.barcode=registration_rec._barcode;
	strcpy(registration_rec._barcode, rc->barcode);
	}
    else registration_rec.barcode=NULL;
    registration_rec.price=registration_rec._price;
	num_toString(registration_rec._price, &rc->price);
    registration_rec.counter=++srec.counter_seq;

    struct quantity_dbrecord quantity_rec;
    quantity_rec.quantity_id=++srec.quantity_pk;
    quantity_rec.registration_id=rc->id;
    quantity_rec.seller=-1;
    quantity_rec.check_type=check_type;
    quantity_rec.oper_type=OPER_TYPE_REGISTRATION;
    num_toString(quantity_rec.quantity, &rc->quantity);
    num_toString(quantity_rec.summ, &rc->summ);
    quantity_rec.counter=srec.counter_seq;
    
    if (internal_volume_id<0) {
	internal_volume_id=++srec.volumes_pk;
	if (NewVolume(internal_volume_id, registration_rec.n_kkm)<0) return -1;
	internal_volume_size=0;
	}
    if (localdb_addRegistration(&registration_rec)<0) return -1;
    if (localdb_addQuantity(&quantity_rec)<0) return -1;
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq)<0) return -1;
    if (localdb_saveSeq(&srec)<0) return -1;
    mw_agentNotify("REGISTRATION", srec.counter_seq);
    return 0;
    }

/*****************************************************************************/
int actions_processStorno(int registration_id, num_t * quantity, num_t * summ, int check_type) {
    if (internal_volume_id<0) return -11;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -12;
    struct quantity_dbrecord rec;
    rec.quantity_id=++srec.quantity_pk;
    rec.registration_id=registration_id;
    rec.seller=-1;
    rec.check_type=check_type;
    rec.oper_type=OPER_TYPE_STORNO;
    num_toString(rec.quantity, quantity);
    num_toString(rec.summ, summ);
    rec.counter=++srec.counter_seq;
    if (localdb_addQuantity(&rec)<0) return -13;
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq)<0) return -14;
    if (localdb_saveSeq(&srec)<0) return -15;
    mw_agentNotify("REGISTRATION", srec.counter_seq);
    }

/************************** cancel_process() ******************************/
int actions_processCancel(struct tm * ts, int n_session, int n_check, int n_doc, int check_type) {
    if (internal_volume_id<0 || internal_receipt_id<0) return 0;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;
    struct receipt_dbrecord rec;
    rec.receipt_id=internal_receipt_id;
    internal_receipt_id=-1;
    rec.session_id=internal_session_id;
    database_tmToString(rec.date_time, ts);
    kkm_getNum(rec.n_kkm);
    rec.seller=-1;
    rec.n_session=n_session;
    rec.n_check=n_check;
    rec.n_doc=n_doc;
    rec.check_type=check_type;
    rec.oper_type=DOC_CANCEL;
    rec.counter=++srec.counter_seq;
    if (localdb_addReceipt(&rec)<0) return -1;
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq));
    if (localdb_saveSeq(&srec)<0) return -1;
    mw_agentNotify("RECEIPT", srec.counter_seq);
    return 0;
    }

/**************************** receipt process ****************************/    
int actions_processReceipt(struct tm * ts, int n_session, int n_check, int n_doc, int check_type, num_t * summ) {
    if (internal_volume_id<0) return -1;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;

    struct payment_dbrecord payment_rec;
    payment_rec.payment_id=++srec.payments_pk;
    payment_rec.receipt_id=internal_receipt_id;
    payment_rec.seller=-1;
    payment_rec.check_type=check_type;
    payment_rec.payment_type=1;
    num_toString(payment_rec.summ, summ);
    payment_rec.counter=++srec.counter_seq;

    struct receipt_dbrecord receipt_rec;
    receipt_rec.receipt_id=internal_receipt_id;
    internal_receipt_id=-1;
    if (internal_session_id<0) internal_session_id=++srec.sessions_pk;
    receipt_rec.session_id=internal_session_id;
    database_tmToString(receipt_rec.date_time, ts);
    kkm_getNum(receipt_rec.n_kkm);
    receipt_rec.seller=-1;
    receipt_rec.n_session=n_session;
    receipt_rec.n_check=n_check;
    receipt_rec.n_doc=n_doc;
    receipt_rec.check_type=check_type;
    receipt_rec.oper_type=OPER_TYPE_CLOSE;
    receipt_rec.counter=srec.counter_seq;
    int err=0;
    if (localdb_addPayment(&payment_rec)<0) err++;
    if (localdb_addReceipt(&receipt_rec)<0) err++;
    if (err==0 && localdb_updateVolume(internal_volume_id, srec.counter_seq)<0) err++;
    if (localdb_saveSeq(&srec)<0 || err>0) return -1;
    mw_agentNotify("RECEIPT", srec.counter_seq);
    return 0;
    }
    
/**************************** session process ********************************/
static int ProcessSession(struct tm * ts, int n_session, int n_doc, num_t * summ) {
    if (internal_session_id<0) return 0;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;
    struct session_dbrecord rec;
    rec.session_id=internal_session_id;
    internal_session_id=-1;
    database_tmToString(rec.date_time, ts);
    kkm_getNum(rec.n_kkm);
    rec.seller=-1;
    rec.n_session=n_session;
    rec.n_doc=n_doc;
    num_toString(rec.total_summ, summ);
    rec.counter=++srec.counter_seq;
    if (localdb_addSession(&rec)<0) return -1;
    if (localdb_saveSeq(&srec)<0) return -1;
    mw_agentNotify("SESSION", srec.counter_seq);
    return 0;
    }

int actions_processSession(struct tm * ts, int n_session, int n_doc, num_t * summ) {
    int r=ProcessSession(ts, n_session, n_doc, summ);
    if (internal_volume_size>global_conf_volume_size) {
	localdb_closeVolume();
	internal_volume_id=-1;
	}
    return r;
    }

/****************************************************************************/

static int RestoreRecords(struct recordset * rset) {
    int r=localdb_getReceiptRecords(rset, internal_receipt_id);
    if (r<0) return -1;
    struct ware_dbrecord ware;
    struct record_receipt * rc;
    struct registration_dbrecord * rec=recordset_begin(rset);
    gboolean init=TRUE;
    int n_doc=-1;
    int n_session=-1;
    int check_type=DOC_UNDEF;
    while (rec!=NULL) {
	if (init) {
	    n_doc=rec->n_doc;
	    n_session=rec->n_session;
	    check_type=rec->check_type;
	    global_doc_state=rec->check_type;
	    }
	else if (rec->n_doc!=n_doc || rec->n_session!=n_session || rec->check_type!=check_type)
		global_doc_state=DOC_ERROR;
        rc=receipt_newRecord();
        rc->id=rec->registration_id;
        rc->num=rec->n_position;
        rc->code=rec->ware_id;
        if (rec->barcode!=NULL) strcpy(rc->barcode, rec->barcode);
        if (rec->price!=NULL) num_fromString(&rc->price, rec->price);
        if (rec->quantity!=NULL) num_fromString(&rc->quantity, rec->quantity);
        if (rec->summ!=NULL) num_fromString(&rc->summ, rec->summ);
        ware.ware_id=rec->ware_id;
        int n=localdb_getWare(&ware);
        if (n>0) {
	    strcpy(rc->longtitle, ware.longtitle);
	    strcpy(rc->shortcut, ware.shortcut);
	    }
	else strcpy(rc->longtitle, "неизвестный товар");
	receipt_addRecord(rc);
	rec=recordset_next(rset);
	}
    return 0;
    }

static int RestoreVolume(char * kkm) {
    internal_volume_id=-1;
    internal_volume_size=0;
    int volume_id=localdb_getVolumeID(kkm);
    if (volume_id<0) return -1;
    if (volume_id==0) return 0;
    char path[1024];
    strcpy(path, global_conf_db_dir);
    strcat(path, "/");
    char * name=strchr(path, 0);
    if (localdb_getVolumeName(name, volume_id)<1) return -1;
    if (localdb_openVolume(path)<0 || localdb_checkVolume()<0) return -1;
    int count=localdb_volumeSize();
    if (count<0) return -1;
    internal_volume_id=volume_id;
    internal_volume_size=count;
    return 1;
    }    

static int RestoreReceipt(char * kkm, int n_doc) {
    int receipt_id=localdb_getLastReceipt(kkm);
    if (receipt_id<0) return -1;
    if (receipt_id==0) return 0;
    int n=localdb_getReceiptStatus(receipt_id);
    if (n<0) return -1;
    if (n>0) return 0;
    n=localdb_getDocNum(receipt_id);
    if (n<0) return -1;
    if (n!=n_doc) return 0;
    internal_receipt_id=receipt_id;
    return 1;
    }

static int RestoreSession(char * kkm, int n_session) {
    internal_session_id=-1;
    internal_receipt_id=-1;
    int session_id=localdb_getLastSession(kkm);
//printf("last session id: %d\n", session_id);
    if (session_id<0) return -1;
    if (session_id==0) return 0;
    int n=localdb_getSessionStatus(session_id);
//printf("last session status: %d\n", n);
    if (n<0) return -1;
    if (n>0) return 0;
    n=localdb_getSessionNum(session_id);
    if (n<0) return -1;
    if (n!=n_session) return 0;
    internal_session_id=session_id;
    return 1;
    }

int actions_processConnect(char * n_kkm) {
    int r=RestoreVolume(n_kkm);
    if (r<0) return -1;
    if (r==0) {
	if (global_session_state!=SESSION_CLOSED) global_session_state=SESSION_ERROR;
	if (global_doc_state!=DOC_CLOSED) global_doc_state=DOC_ERROR;
	return 0;
	}
    r=RestoreSession(n_kkm, global_session_num+1);
    if (r<0) return -1;
    if (global_session_state==SESSION_UNDEF)
	global_session_state=(r>0)?SESSION_OPEN:SESSION_CLOSED;
    if (global_session_state==SESSION_OPEN && r==0) global_session_state=SESSION_ERROR;

    r=RestoreReceipt(n_kkm, global_doc_num+1);
    if (r<0) return -1;
    if (global_doc_state!=DOC_CLOSED) {
	if (r>0) global_doc_state=DOC_OPEN;
	if (r==0) global_doc_state=DOC_ERROR;
	}
    if (global_doc_state==DOC_CLOSED) {
	if (global_session_state==SESSION_CLOSED && internal_volume_size>global_conf_volume_size) {
	    localdb_closeVolume();
	    internal_volume_id=-1;
	    }
	return 0;
	}
    struct recordset rset;
    recordset_init(&rset);
    r=RestoreRecords(&rset);
    recordset_destroy(&rset);
    return r;
    }	

void actions_disconnect(void) {
    receipt_clear();
    localdb_closeVolume();
    }
