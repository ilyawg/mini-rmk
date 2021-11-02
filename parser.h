#ifndef PARSER_H
#define PARSER_H

#include <obstack.h>

enum {
    TAG_EMPTY=0,
    TAG_HEADER,
    TAG_NORMAL,
    TAG_FULL,
    TAG_BEGIN,
    TAG_END,
    };

enum {
    ITEM_NEXT,
    ITEM_TEXT_EMPTY,
    ITEM_TEXT_STRING,
    ITEM_TAG_NAME,
    ITEM_PARAMETER,
    ITEM_VALUE,
    ITEM_TAG,
    ITEM_FAIL,
    ITEM_END
    };

struct tag_param {
    char * name;
    char * value;
    struct tag_param * next;
    };

struct tag_struct {
    char * name;
    int type;
//    int mode;
    int param_count;
    struct tag_param * first_par;
    struct tag_param * current_par;
    struct obstack pool;
    };

int xml_open(char * name);
void xml_close(void);

void tag_init(struct tag_struct * t);
void tag_destroy(struct tag_struct * t);
void tag_clear(struct tag_struct * t);
struct tag_param * tag_pbegin(struct tag_struct * t);
struct tag_param * tag_pnext(struct tag_struct * t);
char * tag_getValue(struct tag_struct * t, char * pname);

int xml_nextItem(struct tag_struct * tag, char * text, size_t size);
#endif //PARSER_H
