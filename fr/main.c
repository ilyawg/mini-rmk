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
#include <termio.h>
#include <pthread.h>

#include "global.h"
#include "conf.h"
#include "kkm.h"
#include "process.h"
#include "scanner.h"

#ifndef CONFIG_FILE
#define CONFIG_FILE	"./shtrihm.conf"
#endif

gboolean show_time(void);

static gboolean StartAction(void) {
    if (localdb_openMain()<0) return TRUE;
    mw_clearText();
    mw_wheelOff();
    kkm_serverStart();
    return FALSE;
    }

static gboolean Start(void) {
    if (StartAction()) {
	mw_showText("Подключение БД...");
	mw_wheelOn();
	g_timeout_add(2000, (GSourceFunc)StartAction, NULL);
	}
scanner_start();
    return FALSE;
    }

int main(int argc, char ** argv) {
    g_thread_init(NULL);
    gdk_threads_init();
    gtk_init(&argc, &argv);
    if (log_open()<0) return -1;
    if (conf_read(CONFIG_FILE)<0) {
	fprintf(stderr, "%s: не удалось прочитать\n", CONFIG_FILE);
	return -1;
	}

puts("****************** Параметры *******************");
printf("Порт подключения ККМ: . %s\n", global_conf_kkm_port);
    int b=0;
    switch (global_conf_kkm_baud) {
    case B1200: b=1200; break;
    case B2400: b=2400; break;
    case B4800: b=4800; break;
    case B9600: b=9600; break;
    case B19200: b=19200; break;
    case B38400: b=38400; break;
    case B57600: b=57600; break;
    case B115200: b=115200; break;
	}
printf("Скорость обмена ККМ: .. %d\n", b);
printf("Каталог БД: ........... %s\n", global_conf_db_dir);
printf("Размер тома БД: ....... %d\n", global_conf_volume_size);
puts ("************************************************");

/*****************************************************************************/
    if (mw_init()<0) {
puts("xml fail");
	return -1;
	}

    receipt_init();
    kkm_init();
    scanner_init();    
    g_idle_add((GSourceFunc)Start, NULL);
    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();
//    kkm_serverStop();
    localdb_closeMain();
    conf_clear();
    }
