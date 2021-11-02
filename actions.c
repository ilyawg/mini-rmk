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
#include "numeric.h"
#include "main_window.h"

struct tm oper_date_time;
int oper_check_num;
int oper_doc_num;
int oper_check_type;
int oper_seller;
int oper_payment_type;
num_t oper_quantity;
num_t oper_summ;
struct record_receipt * oper_rc;

extern int global_doc_num;
extern int global_session_num;
extern int global_doc_num;
extern int global_doc_state;
extern int global_session_state;
extern int global_sale_num;
extern int global_return_num;
extern num_t global_payment_summ;

static int internal_receipt_id=-1;
static int internal_session_id=-1;
static int internal_volume_id=-1;
static int internal_volume_size=0;

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
int actions_processRegistration(void) {
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;					//FIXME!!!
    struct registration_dbrecord registration_rec;
    oper_rc->id=++srec.registrations_pk;
    registration_rec.registration_id=oper_rc->id;
    if (internal_receipt_id<0) internal_receipt_id=++srec.receipts_pk;
    registration_rec.receipt_id=internal_receipt_id;
    registration_rec.n_kkm=registration_rec._n_kkm; kkm_getNum(registration_rec._n_kkm);
    registration_rec.n_session=global_session_num+1;
    switch (global_doc_state) {
    case DOC_SALE: registration_rec.n_check=global_sale_num+1; break;
    case DOC_RETURN: registration_rec.n_check=global_return_num+1; break;
    default: registration_rec.n_check=-1; break;
	}
    registration_rec.n_doc=global_doc_num+1;
    registration_rec.check_type=global_doc_state;
    registration_rec.n_position=oper_rc->num;
    registration_rec.ware_id=oper_rc->code;
    if (strlen(oper_rc->barcode)>0) {
	registration_rec.barcode=registration_rec._barcode;
	strcpy(registration_rec._barcode, oper_rc->barcode);
	}
    else registration_rec.barcode=NULL;
    registration_rec.price=registration_rec._price;
	num_toString(registration_rec._price, &oper_rc->price);
    registration_rec.counter=++srec.counter_seq;

    struct quantity_dbrecord quantity_rec;
    quantity_rec.quantity_id=++srec.quantity_pk;
    quantity_rec.registration_id=oper_rc->id;
    quantity_rec.seller=oper_seller;
    quantity_rec.check_type=oper_check_type;
    quantity_rec.oper_type=OPER_TYPE_REGISTRATION;
    num_toString(quantity_rec.quantity, &oper_rc->quantity);
    num_toString(quantity_rec.summ, &oper_rc->summ);
    quantity_rec.counter=srec.counter_seq;
    
    if (internal_volume_id<0) {
	internal_volume_id=++srec.volumes_pk;
	if (NewVolume(internal_volume_id, registration_rec.n_kkm)<0) return -1;
	}
    if (localdb_addRegistration(&registration_rec)<0) return -1;
    if (localdb_addQuantity(&quantity_rec)<0) return -1;
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq)<0) return -1;
    if (localdb_saveSeq(&srec)<0) return -1;
    mw_agentNotify("REGISTRATION", srec.counter_seq);
    return 0;
    }

/*****************************************************************************/
int actions_processStorno(void) {
    if (internal_volume_id<0) return -11;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -12;
    struct quantity_dbrecord rec;
    rec.quantity_id=++srec.quantity_pk;
    rec.registration_id=oper_rc->id;
    rec.seller=oper_seller;
    rec.check_type=oper_check_type;
    rec.oper_type=OPER_TYPE_STORNO;
    num_toString(rec.quantity, &oper_quantity);
    num_toString(rec.summ, &oper_summ);
    rec.counter=++srec.counter_seq;
    if (localdb_addQuantity(&rec)<0) return -13;
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq)<0) return -14;
    if (localdb_saveSeq(&srec)<0) return -15;
    mw_agentNotify("REGISTRATION", srec.counter_seq);
    }

/************************** payment process *******************************/
int actions_processPayment(void) {
    if (internal_volume_id<0) return -1;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;
    struct payment_dbrecord rec;
    rec.payment_id=++srec.payments_pk;
    rec.receipt_id=internal_receipt_id;
    rec.seller=oper_seller;
    rec.check_type=oper_check_type;
    rec.payment_type=oper_payment_type;
    num_toString(rec.summ, &oper_summ);
    rec.counter=++srec.counter_seq;
    if (localdb_addPayment(&rec)<0) return -1;
//    localdb_addPayment(&rec);
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq)<0) return -1;
    if (localdb_saveSeq(&srec)<0) return -1;
    return 0;
    }

/************************** cancel_process() ******************************/
int actions_processCancel(void) {
    if (internal_volume_id<0 || internal_receipt_id<0) return 0;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;
    struct receipt_dbrecord rec;
    rec.receipt_id=internal_receipt_id;
    internal_receipt_id=-1;
    rec.session_id=internal_session_id;
    database_tmToString(rec.date_time, &oper_date_time);
    kkm_getNum(rec.n_kkm);
    rec.seller=oper_seller;
    rec.n_session=global_session_num+1;
    rec.n_check=oper_check_num;
    rec.n_doc=oper_doc_num;
    rec.check_type=oper_check_type;
    rec.oper_type=DOC_CANCEL;
    rec.counter=++srec.counter_seq;
    if (localdb_addReceipt(&rec)<0) return -1;
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq));
    if (localdb_saveSeq(&srec)<0) return -1;
    mw_agentNotify("RECEIPT", srec.counter_seq);
    return 0;
    }

/**************************** receipt process ****************************/    
int actions_processReceipt(void) {
    if (internal_volume_id<0) return -1;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;
    struct receipt_dbrecord rec;
    rec.receipt_id=internal_receipt_id;
    internal_receipt_id=-1;
    if (internal_session_id<0) internal_session_id=++srec.sessions_pk;
    rec.session_id=internal_session_id;
    database_tmToString(rec.date_time, &oper_date_time);
    kkm_getNum(rec.n_kkm);
    rec.seller=oper_seller;
    rec.n_session=global_session_num+1;
    rec.n_check=oper_check_num;
    rec.n_doc=oper_doc_num;
    rec.check_type=oper_check_type;
    rec.oper_type=OPER_TYPE_CLOSE;
    rec.counter=++srec.counter_seq;
    if (localdb_addReceipt(&rec)<0) return -1;
    if (localdb_updateVolume(internal_volume_id, srec.counter_seq)<0) return -1;
    if (localdb_saveSeq(&srec)<0) return -1;
    mw_agentNotify("RECEIPT", srec.counter_seq);
    return 0;
    }
    
/**************************** session process ********************************/
static int ProcessSession(void) {
    if (internal_session_id<0) return 0;
    struct seq_dbrecord srec;
    if (localdb_getSeq(&srec)<0) return -1;
    struct session_dbrecord rec;
    rec.session_id=internal_session_id;
    internal_session_id=-1;
    database_tmToString(rec.date_time, &oper_date_time);
    kkm_getNum(rec.n_kkm);
    rec.seller=oper_seller;
    rec.n_session=oper_check_num;
    rec.n_doc=oper_doc_num;
    num_toString(rec.total_summ, &oper_summ);
    rec.counter=++srec.counter_seq;
    if (localdb_addSession(&rec)<0) return -1;
    if (localdb_saveSeq(&srec)<0) return -1;
    mw_agentNotify("SESSION", srec.counter_seq);
    return 0;
    }

int actions_processSession(void) {
    int r=ProcessSession();
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
    num_clear(&global_payment_summ);
    char sum[16];
    r=localdb_getPaymentSum(sum, internal_receipt_id);
    if (r<0) return -1;
    if (r>0 && strlen(sum)>0) num_fromString(&global_payment_summ, sum);
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
    if (global_session_state!=SESSION_CLOSED) {
	if (r>0) global_session_state=SESSION_OPEN;
	if (r==0) global_session_state=SESSION_ERROR;
	}

    r=RestoreReceipt(n_kkm, global_doc_num+1);
    if (r<0) return -1;
//    if (r>0 && global_doc_state!=DOC_CLOSED)
//	global_doc_state=DOC_OPEN;
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
