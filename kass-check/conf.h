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

#ifndef CONF_H
#define CONF_H

extern char * global_conf_log_dir;
extern char * global_conf_localdb_dir;
extern char * global_conf_conn_alias;
extern char * global_conf_srv_login;
extern char * global_conf_srv_psw;
extern char * global_conf_srv_base;
extern int global_conf_time_interval;
extern char * global_conf_terminal_name;

int conf_read(char *file);
void conf_clear(void);

#endif //CONF_H
