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

#define LOG_DIR_DEFAULT		"/var/log/mini-rmk"
#define LOCALDB_DIR_DEFAULT	"/var/lib/mini-rmk/data"
#define TIME_INTERVAL_DEFAULT	30

char * global_conf_log_dir=NULL;
char * global_conf_localdb_dir=NULL;
char * global_conf_conn_alias=NULL;
char * global_conf_srv_login=NULL;
char * global_conf_srv_psw=NULL;
char * global_conf_srv_base=NULL;
int global_conf_time_interval=0;
char * global_conf_terminal_name=NULL;

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
    if (!strcmp(arg, "log_directory")) {
	if (global_conf_log_dir!=NULL) free(global_conf_log_dir);
	global_conf_log_dir=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "localdb_path")) {
	if (global_conf_localdb_dir!=NULL) free(global_conf_localdb_dir);
	global_conf_localdb_dir=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "conn_alias")) {
	if (global_conf_conn_alias!=NULL) free(global_conf_conn_alias);
	global_conf_conn_alias=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "server_login")) {
	if (global_conf_srv_login!=NULL) free(global_conf_srv_login);
	global_conf_srv_login=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "server_psw")) {
	if (global_conf_srv_psw!=NULL) free(global_conf_srv_psw);
	global_conf_srv_psw=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "server_base")) {
	if (global_conf_srv_base!=NULL) free(global_conf_srv_base);
	global_conf_srv_base=strdup(value);
	return 1;
	}
    if (!strcmp(arg, "time_interval")) {
	int v=atoi(value);
	if (v<1) return -1;
	global_conf_time_interval=v;
	return 1;
	}
    if (!strcmp(arg, "terminal_name")) {
	if (global_conf_terminal_name!=NULL) free(global_conf_terminal_name);
	global_conf_terminal_name=strdup(value);
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
    size_t bufsize=0;
    char *s=NULL;
    int line=0;
    while (1) {
	int len=getline(&s,&bufsize,fp);
	if (len<0) break;
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
	}
    free(s);
    fclose(fp);
    
    if (global_conf_log_dir==NULL) 
	global_conf_log_dir=strdup(LOG_DIR_DEFAULT);

    if (global_conf_localdb_dir==NULL) 
	global_conf_localdb_dir=strdup(LOCALDB_DIR_DEFAULT);

    if (global_conf_time_interval==0) 
	global_conf_time_interval=TIME_INTERVAL_DEFAULT;
	
    return 0;
    }

void conf_clear(void) {
    if (global_conf_log_dir!=NULL) {
	free(global_conf_log_dir);
	global_conf_log_dir=NULL;
	}

    if (global_conf_localdb_dir!=NULL) {
	free(global_conf_localdb_dir);
	global_conf_localdb_dir=NULL;
	}

    if (global_conf_conn_alias!=NULL) {
	free(global_conf_conn_alias);
	global_conf_conn_alias=NULL;
	}

    if (global_conf_srv_login!=NULL) {
	free(global_conf_srv_login);
	global_conf_srv_login=NULL;
	}

    if (global_conf_srv_psw!=NULL) {
	free(global_conf_srv_psw);
	global_conf_srv_psw=NULL;
	}

    if (global_conf_srv_base!=NULL) {
	free(global_conf_srv_base);
	global_conf_srv_base=NULL;
	}

    if (global_conf_terminal_name!=NULL) {
	free(global_conf_terminal_name);
	global_conf_terminal_name=NULL;
	}

    global_conf_time_interval=0;
    }
