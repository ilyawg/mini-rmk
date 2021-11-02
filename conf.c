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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termio.h>

#include "conf.h"

#define HOST_ADDR_DEFAULT	1
#define KKM_ADDR_DEFAULT	2
#define KKM_PORT_DEFAULT	"/dev/ttyS0"
#define KKM_BAUD_DEFAULT	B4800
#define DB_DIR_DEFAULT		"/var/lib/mini-rmk/data"
#define VOLUME_SIZE_DEFAULT	1500

char * global_conf_kkm_port;
int global_conf_kkm_baud;
char * global_conf_db_dir;
int global_conf_volume_size;
char * global_conf_card_code;
int global_conf_return_access;
int global_conf_storno_access;
int global_conf_cancel_access;

static int parse_str(char * str, char * arg, size_t argsize, char * value, int valuesize) {
    char *arg_begin=NULL;
    char *arg_end=NULL;
    char *dif=NULL;
    char *value_begin=NULL;
    char *value_space=NULL;
    char *value_end=NULL;
    while(*str!=0) {
	if (*str!=' ' && *str!='\t') {
	    if (*str=='#' || *str=='\n' || *str=='\r') break;
	    if (arg_begin==NULL) arg_begin=str;
	    if (value_begin==NULL && dif!=NULL) value_begin=str;
	    if (*str=='=' && dif==NULL) dif=str;
	    if (dif==NULL) arg_end=str;
	    if (value_begin!=NULL) value_end=str;
	    }
	else if(value_begin!=NULL && value_space==NULL) value_space=str;
	str++;
	}

    if (arg_begin==NULL && arg_end==NULL 
	&& value_begin==NULL && value_end==NULL)	// пустая строка
	    return 0;

    if (arg_begin==NULL || arg_end==NULL 
	|| value_begin==NULL || value_end==NULL)	// неполная строка
	    return -1;

    int len=arg_end-arg_begin+1;
    if (len<=0 || len>=argsize) return -1;
    memcpy(arg, arg_begin, len);
    
    len=value_end-value_begin+1;
    if (len<=0 || len>=valuesize) return -1;
    memcpy(value, value_begin, len);
    return 1;
    }

static int set_value(char * arg, char * value) {
    if (!strcmp(arg, "kkm_port")) {
//	if (global_conf_kkm_port!=NULL) free(global_conf_kkm_port);
	global_conf_kkm_port=strdup(value);
	return 1;
	}

    if (!strcmp(arg, "kkm_baud")) {
	int v=atoi(value);
	switch (v) {
	case 1200: global_conf_kkm_baud=B1200; return 1;
	case 2400: global_conf_kkm_baud=B2400; return 1;
    	case 4800: global_conf_kkm_baud=B4800; return 1;
	case 9600: global_conf_kkm_baud=B9600; return 1;
	case 19200: global_conf_kkm_baud=B19200; return 1;
	case 38400: global_conf_kkm_baud=B38400; return 1;
	case 57600: global_conf_kkm_baud=B57600; return 1;
	case 115200: global_conf_kkm_baud=B115200; return 1;
	default: return -1;
	    }
	}
    
    if (!strcmp(arg, "db_directory")) {
//	if (global_conf_db_dir!=NULL) free(global_conf_db_dir);
	global_conf_db_dir=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "db_volume_size")) {
	int v=atoi(value);
	if (v<1) return -1;
	global_conf_volume_size=v;
	return 1;
	}
    if (!strcmp(arg, "card_code")) {
	global_conf_card_code=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "storno_access")) {
	if (strcmp(value, "on")==0) global_conf_storno_access=-1;
	else if (strcmp(value, "off")==0) global_conf_storno_access=0;
	else return -1;
	return 1;
	}
    if (!strcmp(arg, "cancel_access")) {
	if (strcmp(value, "on")==0) global_conf_cancel_access=-1;
	else if (strcmp(value, "off")==0) global_conf_cancel_access=0;
	else return -1;
	return 1;
	}
    if (!strcmp(arg, "return_access")) {
	if (strcmp(value, "on")==0) global_conf_return_access=-1;
	else if (strcmp(value, "off")==0) global_conf_return_access=0;
	else return -1;
	return 1;
	}
    return 0;
    }
    

int conf_read(char *file) {
    FILE *fp=fopen(file,"r");
    if (fp==NULL) {
	perror(file);
	return -1;
	}

    global_conf_db_dir=NULL;
    global_conf_kkm_port=NULL;
    global_conf_kkm_baud=KKM_BAUD_DEFAULT;
    global_conf_volume_size=VOLUME_SIZE_DEFAULT;
    global_conf_card_code=NULL;
    global_conf_return_access=-1;
    global_conf_storno_access=-1;
    global_conf_cancel_access=-1;

    size_t bufsize=0;
    char *s=NULL;
    int line=0;
    int len=getline(&s, &bufsize, fp);
    while (len>=0) {
        line++;
	char arg[128]; bzero(arg,sizeof(arg));
	char value[1024]; bzero(value,sizeof(value));
	int r=parse_str(s, arg, sizeof(arg), value, sizeof(value));
	if (r<0) fprintf(stderr, "%s: ошибка в строке %u\n", file, line);
	if (r>0) {
	    r=set_value(arg, value);
	    if (r<0) fprintf(stderr, "%s - неверное значение: <%s>\n", arg, value);
	    if (r==0) fprintf(stderr, "%s - неизвестный параметр\n", arg);
	    }
	len=getline(&s, &bufsize, fp);
	}
    free(s);
    fclose(fp);

    if (global_conf_kkm_port==NULL) 
	global_conf_kkm_port=strdup(KKM_PORT_DEFAULT);

    if (global_conf_db_dir==NULL)
	global_conf_db_dir=strdup(DB_DIR_DEFAULT);

    return 0;
    }

void conf_clear(void) {
    if (global_conf_kkm_port!=NULL) {
	free(global_conf_kkm_port);
	global_conf_kkm_port=NULL;
	}
    if (global_conf_db_dir!=NULL) {
	free(global_conf_db_dir);
	global_conf_db_dir=NULL;
	}
    if (global_conf_card_code!=NULL) {
	free(global_conf_card_code);
	global_conf_card_code=NULL;
	}
    }
