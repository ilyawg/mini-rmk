#ifndef GLOBAL_H
#define GLOBAL_H

#define DEBUG

#define BUILD_CODE	"400b"

#define CONFIG_FILE	"/etc/mini-rmk/rmk.conf"
#define PIXMAP_DIR 	"/usr/share/pixmaps/mini-rmk"

#define LOG_FILE	"/media/work/log/mini-rmk.log"

#include "../numeric.h"

extern int global_doc_state;
extern int global_session_state;
extern int global_check_num;
extern int global_doc_num;
extern int global_session_num;
extern num_t * global_receipt_summ;

/* статус документа */
enum {
    DOC_ERROR=0,	// некорректый документ
    DOC_SALE,		// открыт документ ПРОДАЖА
    DOC_RETURN,		// открыт документ ВОЗВРАТ
    DOC_CASHIN,		// открыт документ ВНЕСЕНИЕ
    DOC_CASHOUT,	// открыт документ ИЗЪЯТИЕ
    DOC_CLOSED,		// документ закрыт
    DOC_CANCEL,		// документ аннулирован
    DOC_OPEN,		// документ открыт
    DOC_UNDEF,		// зарезервирован
    DOC_CURRENT,	// зарезервирован
    DOC_CLEAR		// зарезервирован
    };

#endif //GLOBAL_H
