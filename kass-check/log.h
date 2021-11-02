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

#ifndef LOG_H
#define LOG_H

enum {
    INTERNAL_ERROR,
    WORK_BEGIN,
    
    VOLUME_OPEN_FAIL,
    VOLUME_ERROR,
    VOLUMES_FAIL,
    VOLUME_CREATED,
    VOLUME_CREATEDB_FAIL,
    VOLUME_REGISTER_FAIL,
    VOLUME_STRUCT_FAIL,
    VOLUME_NOT_FIND,
    VOLUME_NOT_FOUND,
    VOLUME_OPENFILE_FAIL,
    VOLUME_CREATETBL_FAIL,
    VOLUME_UNDEF,
    VOLUME_FIND,
        
    OPENDIR_FAIL,
    UNKNOWN_TERMINAL,
    
    FORK_FAIL,
    WORK_BREAK,    
    WORK_END
    };

int log_open(void);
void log_puts(int mode, char * text);
void log_printf(int mode, char *template, ...);
void log_message(int id, ...);
void log_close(void);

#endif //LOG_H
