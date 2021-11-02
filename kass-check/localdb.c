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
#include <sys/stat.h>
#include <dirent.h>

#include "conf.h"
#include "localdb.h"
#include "log.h"

void localdb_closeAll(void) {
    localdb_closeVolume();
    localdb_closeWareDB();
    localdb_closeMainDB();
    }

static char * getNewName(char * path, char * dbname) {
    char * name=strchr(path,0);
    struct stat s;
    int i=0;
    do {
	sprintf(name, "%s.%u.ldb", dbname, i++);
	} while (stat(path, &s)==0);
    return name;
    }

static int check_globalseq_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getMainTableStruct(&rset, "_global_seq")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "sessions_pk integer")) result|=0x0001;
	else if (!strcmp(s, "volumes_pk integer")) result|=0x0002;
	else if (!strcmp(s, "receipts_pk integer")) result|=0x0004;
	else if (!strcmp(s, "registrations_pk integer")) result|=0x0008;
	else if (!strcmp(s, "ware_quantity_pk integer")) result|=0x0010;
    	else if (!strcmp(s, "payments_pk integer")) result|=0x0020;
	else if (!strcmp(s, "counter_seq integer")) result|=0x0040;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x007F) return -1;
    return 0;
    }

static int check_sessions_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getMainTableStruct(&rset, "sessions")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "session_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "date_time timestamp")) result|=0x0002;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "seller integer")) result|=0x0008;
	else if (!strcmp(s, "n_session integer")) result|=0x0010;
	else if (!strcmp(s, "n_doc integer")) result|=0x0020;
	else if (!strcmp(s, "total_summ numeric(15,2)")) result|=0x0040;
    	else if (!strcmp(s, "counter integer")) result|=0x0080;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x0FF) return -1;
    return 0;
    }

static int check_volumes_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getMainTableStruct(&rset, "volumes")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "volume_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "volume_name varchar(100)")) result|=0x0002;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "counter integer")) result|=0x0008;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x000F) return -1;
    return 0;
    }

static int check_ware_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getWareTableStruct(&rset, "ware")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char *s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "ware_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "longtitle varchar(255)")) result|=0x0002;
	else if (!strcmp(s, "shortcut varchar(255)")) result|=0x0004;
	else if (!strcmp(s, "price numeric(10,2)")) result|=0x0008;
	else if (!strcmp(s, "quantity numeric(10,3)")) result|=0x0010;
	else if (!strcmp(s, "group_id integer")) result|=0x0020;
	else if (!strcmp(s, "erase boolean")) result|=0x0040;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x007F) return -1;
    return 0;
    }

static int check_barcodes_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getWareTableStruct(&rset, "barcodes")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char *s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "internal_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "ware_id integer")) result|=0x0002;
	else if (!strcmp(s, "barcode varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "erase boolean")) result|=0x0008;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x000F) return -1;
    return 0;
    }

static int checkMainTables(void) {
    if (check_globalseq_table()<0) return -1;
    if (check_sessions_table()<0) return -1;
    if (check_volumes_table()<0) return -1;
    return 0;
    }

int RenameDB(char * dbpath, char * dbname) {
    char newname[1024];
    strcpy(newname, global_conf_localdb_dir);
    strcat(newname, "/");
    if (getNewName(newname, dbname)==NULL) {
        log_printf(0, "Не удалось подобрать новое имя файла БД: <%s>", dbname);
        return -1;
        }
    if (rename(dbpath, newname)<0) {
        log_printf(0, "Не удалось переименовать файл БД: <%s>", dbname);
        return -1;
        }
    log_printf(0, "Файл БД переименован: <%s>", newname);
    return 0;
    }

static int NewDB(char * dbname) {
    struct stat s;
    char dbpath[1024];
    strcpy(dbpath, global_conf_localdb_dir);
    strcat(dbpath, "/");
    strcat(dbpath, dbname);
    strcat(dbpath, ".ldb");
//#ifdef DEBUG
printf("dbpath: <%s>\n", dbpath);
//#endif
    if (stat(dbname, &s)==0 && s.st_size>0 && RenameDB(dbpath, dbname)<0)
	return -1;
    return 0;
    }

int localdb_checkMainDB(void) {
    struct stat s;
    char dbname[1024];
    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/main.ldb");
    // проверим наличие файла БД
    if (stat(dbname, &s)<0 || s.st_size==0) {
        log_puts(0, "Файл БД операций не найден");
	return 0;
	}
	
    // попробуем открыть
    if (localdb_openMainDB(dbname)<0) 
        log_puts(0, "Не удалось открыть БД операций");
    else {
	// проверим таблицы
	if (checkMainTables()==0) return 1;		// таблицы в порядке
        log_puts(0, "Неверная структура таблиц БД операций");
	localdb_closeMainDB();
	}
    return RenameDB(dbname, "main");
    }

int localdb_renameMainDB(void) {
    char dbname[1024];
    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/main.ldb");
    return RenameDB(dbname, "main");
    }

int localdb_renameWareDB(void) {
    char dbname[1024];
    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/ware.ldb");
    return RenameDB(dbname, "ware");
    }

int localdb_checkWareDB(void) {
    struct stat s;
    char dbname[1024];
    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/ware.ldb");
    // проверим наличие файла БД
    if (stat(dbname, &s)<0 || s.st_size==0) {
        log_puts(0, "Файл БД справочников не найден");
	return 0;
	}
    // попробуем открыть
    if (localdb_openWareDB(dbname)<0) 
        log_puts(0, "Не удалось открыть БД справочников");
    else {
        // проверим таблицы
	if (check_ware_table()==0 && check_barcodes_table()==0) return 1;
        log_puts(0, "Неверная структура таблиц БД справочников");
	localdb_closeWareDB();
	}
    return RenameDB(dbname, "ware");
    }

int localdb_createMainDB(void) {
    char dbname[1024];
    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/main.ldb");
    if (localdb_openMainDB(dbname)<0) {
        log_puts(0, "Не удалось создать новый файл БД операций");
	return -1;
	}
    if (chmod(dbname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)<0) {
        log_puts(0, "Не удалось задать права доступа БД операций");
	return -1;
	}
    if (localdb_createMainTables()<0) {
        log_puts(0, "Не удалось создать структуру таблиц БД операций");
        return -1;
	}
    log_puts(0, "Создан новый файл БД операций");
    return 0;
    }

int localdb_createWareDB(void) {
    char dbname[1024];
    strcpy(dbname, global_conf_localdb_dir);
    strcat(dbname, "/ware.ldb");
    if (localdb_openWareDB(dbname)<0) {
        log_puts(0, "Не удалось создать новый файл БД справочников");
	return -1;
	}
    if (chmod(dbname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)<0) {
        log_puts(0, "Не удалось задать права доступа БД справочников");
	return -1;
	}
    if (localdb_createWareTables()<0) {
        log_puts(0, "Не удалось создать структуру таблиц БД справочников");
	return -1;
	}
    log_printf(0, "Создан новый файл БД справочников: <%s>", dbname);
    return 0;
    }

/*****************************************************************************/
static int check_receipts_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getVolumeTableStruct(&rset, "receipts")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
//printf("s: <%s>\n", s);
	if (!strcmp(s, "receipt_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "session_id integer")) result|=0x0002;
	else if (!strcmp(s, "date_time timestamp")) result|=0x0004;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0008;
	else if (!strcmp(s, "seller integer")) result|=0x0010;
	else if (!strcmp(s, "n_session integer")) result|=0x0020;
	else if (!strcmp(s, "n_check integer")) result|=0x0040;
	else if (!strcmp(s, "n_doc integer")) result|=0x0080;
	else if (!strcmp(s, "check_type integer")) result|=0x0100;
	else if (!strcmp(s, "oper_type integer")) result|=0x0200;
	else if (!strcmp(s, "counter integer")) result|=0x0400;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x07FF) return -1;
    return 0;
    }

static int check_registrations_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getVolumeTableStruct(&rset, "registrations")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "registration_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "receipt_id integer")) result|=0x0002;
	else if (!strcmp(s, "n_kkm varchar(20)")) result|=0x0004;
	else if (!strcmp(s, "n_session integer")) result|=0x0008;
	else if (!strcmp(s, "n_check integer")) result|=0x0010;
	else if (!strcmp(s, "n_doc integer")) result|=0x0020;
	else if (!strcmp(s, "check_type integer")) result|=0x0040;
	else if (!strcmp(s, "n_position integer")) result|=0x0080;
	else if (!strcmp(s, "ware_id integer")) result|=0x0100;
	else if (!strcmp(s, "barcode varchar(20)")) result|=0x0200;
	else if (!strcmp(s, "price numeric(10,2)")) result|=0x0400;
	else if (!strcmp(s, "counter integer")) result|=0x0800;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x0FFF) return -1;
    return 0;
    }

static int check_ware_quantity_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getVolumeTableStruct(&rset, "ware_quantity")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "quantity_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "registration_id integer")) result|=0x0002;
	else if (!strcmp(s, "seller integer")) result|=0x0004;
	else if (!strcmp(s, "check_type integer")) result|=0x0008;
	else if (!strcmp(s, "oper_type integer")) result|=0x0010;
	else if (!strcmp(s, "quantity numeric(10,3)")) result|=0x0020;
	else if (!strcmp(s, "summ numeric(10,2)")) result|=0x0040;
	else if (!strcmp(s, "counter integer")) result|=0x0080;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x00FF) return -1;
    return 0;
    }

static int check_payments_table(void) {
    struct recordset rset;
    recordset_init(&rset);
    if (localdb_getVolumeTableStruct(&rset, "payments")<0) {
	recordset_destroy(&rset);
	return -1;
	}
    int result=0;
    char * s=recordset_begin(&rset);
    while (s!=NULL) {
	if (!strcmp(s, "payment_id INTEGER")) result|=0x0001;
	else if (!strcmp(s, "receipt_id integer")) result|=0x0002;
	else if (!strcmp(s, "seller integer")) result|=0x0004;
	else if (!strcmp(s, "check_type integer")) result|=0x0008;
	else if (!strcmp(s, "payment_type integer")) result|=0x0010;
	else if (!strcmp(s, "summ numeric(10,2)")) result|=0x0020;
	else if (!strcmp(s, "counter integer")) result|=0x0040;
    	s=recordset_next(&rset);
	}
    recordset_destroy(&rset);
    if (result!=0x007F) return -1;
    return 0;
    }

/****************************************************************************/
static int volume_check(void) {
    if (check_receipts_table()<0) return -1;
    if (check_registrations_table()<0) return -1;
    if (check_ware_quantity_table()<0) return -1;
    if (check_payments_table()<0) return -1;
    return 0;
    }

struct file_record {
    char name[256];
    time_t mtime;
    int version;
    int flag;
    };

static int volume_isName(char * name, int id) {
    size_t len=strlen(name);
    if (len<12) return -1;

    char magic[4]; bzero(magic, sizeof(magic));
    memcpy(magic, name, 3);
    if (strcmp(magic, "vol")) return -1;    
    name+=3;

    char file_id[6]; bzero(file_id, sizeof(file_id));
    memcpy(file_id, name, 5);

    char s_id[16];
    sprintf(s_id, "%05u", id);
    if (strcmp(file_id, s_id)) return -1;
    name+=5;
    
    if (!strcmp(name,".ldb")) return 0;
    if (*name!='.') return -1;
    name++;
    char * ext;
    int v=strtoul(name, &ext, 10);
    if (!strcmp(ext,".ldb")) return v;
    return -1;
    }

static int FileList(struct recordset * rset, int id) {
    char path[1024];
    strcpy(path, global_conf_localdb_dir);
    strcat(path, "/");
    char *name=strchr(path, 0);
    DIR * dp=opendir(path);
    if (dp==NULL) {
        log_printf(0, "Не удалось прочитать содержимое каталога <%s>", path);
	log_message(OPENDIR_FAIL, path);
	return -1;
	}
    struct dirent * ep;
    int count=0;
    struct stat s;
    while (ep=readdir(dp))
	if (ep->d_type==DT_REG) {
	    int v=volume_isName(ep->d_name, id);
	    if (v>=0) {
		strcpy(name, ep->d_name);
		if (stat(path, &s)==0 && s.st_size>0) {
		    struct file_record * rec=recordset_new(rset, sizeof(struct file_record));
		    strcpy(rec->name, ep->d_name);
		    rec->mtime=s.st_mtime;
		    rec->version=v;
		    rec->flag=0;
		    count++;
		    }
		}
	    }
    closedir(dp);
    return count;
    }

static struct file_record * last_file(struct recordset * rset) {
    time_t mtime=0;
    struct file_record * result=NULL;
    struct file_record * rec=recordset_begin(rset);
    while (rec!=NULL) {
//printf("mtime=%d\n", rec->mtime);
	if (rec->flag==0 && rec->mtime>mtime) {
	    result=rec;
	    mtime=rec->mtime;
	    }
	rec=recordset_next(rset);
	}
    return result;
    }

static int last_version(struct recordset * rset) {
    int ver=-1;
    struct file_record * rec=recordset_begin(rset);
    while (rec!=NULL) {
	if (rec->flag==0 && rec->version>ver) ver=rec->version;
	rec=recordset_next(rset);
	}
    return ver;
    }

/****************************************************************************/
int localdb_newVolume(int id, char * dbname) {
    char path[1024];
    strcpy(path, global_conf_localdb_dir);
    strcat(path, "/");
    struct recordset rset;
    recordset_init(&rset);
    if (FileList(&rset, id)<0) return -1;
    int v=last_version(&rset)+1;
    recordset_destroy(&rset);
    char * name=strchr(path, 0);
    sprintf(name, "vol%05u.%u.ldb", id, v);
    if (localdb_openVolumeName(path)<0) {
    	log_printf(0, "Не удалось открыть файл тома <%s>", path);
	return -1;
	}
    if (chmod(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)<0) {
        log_printf(0, "Не удалось задать права доступа тома <%s>", path);
	return -1;
	}
    if (localdb_createVolume()<0) {
    	log_printf(0, "Не удалось создать структуру таблиц тома <%s>", path);
	localdb_closeVolume();
	return -1;
	}
    strcpy(dbname, name);
    return 0;
    }

/*****************************************************************************/
int localdb_openVolume(char * name) {
    char path[1024];
    strcpy(path, global_conf_localdb_dir);
    strcat(path, "/");
    strcat(path, name);
    struct stat s;
    if (stat(path, &s)<0 || s.st_size==0) {
        log_printf(0, "Файл тома не обнаружен: <%s>", path);
	return 0;
	}
    if (localdb_openVolumeName(path)<0) {
        log_printf(0, "Не удалось открыть файл тома <%s>", path);
	return -1;
	}
    if (volume_check()==0) return 1;
    log_printf(0, "Неверная структура таблиц тома <%s>", path);
    localdb_closeVolume();
    return -1;
    }

/*****************************************************************************/

int localdb_findVolume(int id, char * name) {
    struct recordset rset;
    recordset_init(&rset);
    if (FileList(&rset, id)<0) return -1;
    struct file_record * rec=last_file(&rset);
    if (rec==NULL) {
	recordset_destroy(&rset);
        log_printf(0, "Файл тома %u не обнаружен", id);
	return 0;
	}
    strcpy(name, rec->name);
    recordset_destroy(&rset);
    log_printf(0, "Обнаружен файл тома %u: <%s>", id, name);
    return 1;
    }

int localdb_newMainDB(void) {
    if (NewDB("main")<0) {
        log_puts(0, "Не удалось создать новый файл БД операций");
        return -1;
        }
    if (localdb_createMainDB()<0) {
	log_puts(0, "Не удалось создать структуру таблиц БД операций");
	return -1;
	}
    return 0;
    }

int localdb_newWareDB(void) {
    if (NewDB("ware")<0) {
        log_puts(0, "Не удалось создать новый файл БД операций");
	return -1;
	}
    if (localdb_createWareDB()<0) {
        log_puts(0, "Не удалось создать структуру таблиц БД операций");
	return -1;
	}
    return 0;
    }
