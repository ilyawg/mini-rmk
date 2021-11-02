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

#ifndef CONFIG_FILE
#define CONFIG_FILE	"./shtrihm.conf"
#endif

int global_doc_state;
int global_session_state;
int global_sale_num=0;
int global_return_num=0;
int global_doc_num=0;
int global_session_num;
num_t global_total_summ;
num_t global_payment_summ;

static gboolean show_time(void);

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
    return FALSE;
    }

int main(int argc, char ** argv) {
    num_init(&global_total_summ, 2);
    num_init(&global_payment_summ, 2);
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
    process_init();
    kkm_init();
    
    g_idle_add((GSourceFunc)Start, NULL);

    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();
    process_destroy();
//    kkm_serverStop();
    localdb_closeMain();
    conf_clear();
    }
