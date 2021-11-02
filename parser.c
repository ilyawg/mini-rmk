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
#include <stdlib.h>
#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#include "parser.h"

#define TAB_VALUE	"        "
#define TAB_SIZE	sizeof(TAB_VALUE)-1
FILE * f=NULL;

enum {
    MODE_UNDEF,
    MODE_TEXT,
    MODE_TAG_BEGIN,
    MODE_TAG_NAME,
    MODE_TAG,
    MODE_TAG_END,
    MODE_PARAMETER,
    MODE_VALUE,
    MODE_STRING,
    MODE_TAG_CLOSE,
    MODE_COMMENT_BEGIN,
    MODE_COMMENT_BEGIN2,
    MODE_COMMENT,
    MODE_COMMENT_END,
    MODE_COMMENT_END2,
    MODE_END
    };

static int internal_mode=MODE_UNDEF;
static int internal_symbol=0;
static int internal_tag=TAG_EMPTY;
static int internal_text=ITEM_TEXT_EMPTY;
static char internal_value_delimiter=0;


/****************************************************************************/

int xml_open(char * name) {
    f=fopen(name, "r");
    if (f==NULL) return -1;
    internal_symbol=0;
    internal_mode=MODE_UNDEF;
    return 0;
    }

void xml_close(void) {
    fclose(f);
    }

/****************************************************************************/
void tag_init(struct tag_struct * t) {
    bzero(t, sizeof(struct tag_struct));
    obstack_init(&t->pool);
    }

void tag_destroy(struct tag_struct * t) {
    obstack_free(&t->pool, NULL);
    }

void tag_clear(struct tag_struct * t) {
//puts("* tag_clear()");
    if (t->name==NULL) return;
    obstack_free(&t->pool, t->name);
    t->name=NULL;
    t->param_count=0;
    t->first_par=NULL;
    t->current_par=NULL;
    }

static void tag_name(struct tag_struct * t, char * name) {
    if (t->name!=NULL) {
	puts("*** tag_name() ASSERT: name not empty");
	tag_clear(t);
	}
//printf("!!! tag_name: [[%s]]\n",name);
    t->name=obstack_copy0(&t->pool, name, strlen(name));
    }

static void tag_pname(struct tag_struct * t, char * name) {
    struct tag_param * p=obstack_alloc(&t->pool, sizeof(struct tag_param));
    bzero(p, sizeof(struct tag_param));
    if (t->param_count==0) t->first_par=p;
    else t->current_par->next=p;
    t->current_par=p;
    t->param_count++;
    t->current_par->name=obstack_copy0(&t->pool, name, strlen(name));
    }

static void tag_pvalue(struct tag_struct * t, char * v) {
    t->current_par->value=obstack_copy0(&t->pool, v, strlen(v));
    }

struct tag_param * tag_pbegin(struct tag_struct * t) {
    t->current_par=t->first_par;
    return t->current_par;
    }

struct tag_param * tag_pnext(struct tag_struct * t) {
    t->current_par=t->current_par->next;
    return t->current_par;
    }

char * tag_getValue(struct tag_struct * t, char * pname) {
    struct tag_param * i=t->first_par;
    while (i!=NULL) {
	if (!strcmp(i->name, pname)) return i->value;
	i=i->next;
	}
    return NULL;
    }

/****************************************************************************/
static int process_undef(void) {
//puts("* process_undef()");
    switch (internal_symbol) {
    case '\r':
    case '\n':
	internal_symbol=0;
	return ITEM_NEXT;
	
    case '<':
	internal_mode=MODE_TAG_BEGIN;
	internal_symbol=0;
	return ITEM_NEXT;

    case EOF: return ITEM_END;
	}
    internal_mode=MODE_TEXT;
    internal_text=ITEM_TEXT_EMPTY;
    return ITEM_NEXT;
    }

static int process_text(struct obstack * p) {
//puts("* process_text()");
//printf("\tinternal_symbol: %c\n", internal_symbol);
    switch (internal_symbol) {
    case ' ':
	obstack_1grow(p, ' ');
	internal_symbol=0;
	return ITEM_NEXT;
    case '\t':
	obstack_grow(p, TAB_VALUE, TAB_SIZE);
	internal_symbol=0;
	return ITEM_NEXT;
    case '\r':
    case '\n':
	internal_symbol=0;
	return ITEM_NEXT;
    case '<':
	internal_mode=MODE_TAG_BEGIN;
	internal_symbol=0;
	return internal_text;
    case EOF:
	internal_mode=MODE_END;
	internal_symbol=0;
	return internal_text;
	}
    internal_text=ITEM_TEXT_STRING;
    obstack_1grow(p, internal_symbol);
    internal_symbol=0;
    return ITEM_NEXT;
    }

static int process_tag_begin(void) {
//puts("* process_tag_begin()");
    switch (internal_symbol) {
    case '!':
	internal_mode=MODE_COMMENT_BEGIN;
	internal_symbol=0;
	return ITEM_NEXT;
	
    case '?':
	internal_mode=MODE_TAG_NAME;
	internal_tag=TAG_HEADER;
	internal_symbol=0;
	return ITEM_NEXT;

    case '/':
	internal_mode=MODE_TAG_NAME;
	internal_tag=TAG_END;
	internal_symbol=0;
	return ITEM_NEXT;

    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
	internal_mode=MODE_TAG_NAME;
	internal_tag=TAG_BEGIN;
	return ITEM_NEXT;
	}
    return ITEM_FAIL;
    }

static int process_tag_name(struct obstack * p) {
//puts("* process_tag_name()");
    switch (internal_symbol) {
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
	obstack_1grow(p, internal_symbol);
	internal_symbol=0;
	return ITEM_NEXT;
    case ' ':
//    case '\r':
//    case '\n':
    case '\t':
	internal_mode=MODE_TAG;
	internal_symbol=0;
	return ITEM_TAG_NAME;
    case '?':
    case '/':
    case '>':
	internal_mode=MODE_TAG_END;
	return ITEM_TAG_NAME;
	}
    return ITEM_FAIL;
    }
    
static int process_tag(void) {
//puts("* process_tag()");
    switch (internal_symbol) {
    case ' ':
//    case '\r':
//    case '\n':
    case '\t':
	internal_symbol=0;
	return ITEM_NEXT;
    
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
	internal_mode=MODE_PARAMETER;    
	return ITEM_NEXT;
    case '?':
    case '/':
    case '>':
	internal_mode=MODE_TAG_END;
	return ITEM_NEXT;
	}
    return ITEM_FAIL;
    }

static int process_parameter(struct obstack * p) {
//puts("* process_parameter()");
    switch (internal_symbol) {
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
	obstack_1grow(p, internal_symbol);
	internal_symbol=0;
	return ITEM_NEXT;
    case '=':
	internal_mode=MODE_VALUE;
	internal_symbol=0;
	return ITEM_PARAMETER;
	}
    return ITEM_FAIL;
    }

static int process_value(void) {
//puts("* process_value()");
    if (internal_symbol=='\'' || internal_symbol=='"') {
	internal_mode=MODE_STRING;
	internal_value_delimiter=internal_symbol;
	internal_symbol=0;
	return ITEM_NEXT;
	}
    return ITEM_FAIL;
    }

static int process_string(struct obstack * p) {
//puts("* process_string()");
    if (internal_symbol==internal_value_delimiter) {
	internal_mode=MODE_TAG;
	internal_symbol=0;
	return ITEM_VALUE;
	}
    switch (internal_symbol) {
    case '\r':
    case '\n':
    case EOF:
	return ITEM_FAIL;
    case '\t':
	obstack_grow(p, TAB_VALUE, TAB_SIZE);
	internal_symbol=0;
	return ITEM_NEXT;
	}    
    obstack_1grow(p, internal_symbol);
    internal_symbol=0;
    return ITEM_NEXT;
    }

static int process_tag_end(void) {
//puts("* process_tag_end()");
    switch (internal_symbol) {
    case '?':
	if (internal_tag==TAG_HEADER) {
	    internal_mode=MODE_TAG_CLOSE;
	    internal_symbol=0;
	    return ITEM_NEXT;
	    }
	 break;
    case '/':
	if (internal_tag==TAG_BEGIN) {
	    internal_tag=TAG_FULL;
	    internal_mode=MODE_TAG_CLOSE;
	    internal_symbol=0;
	    return ITEM_NEXT;
	    }
	break;
    case '>':
	if (internal_tag==TAG_BEGIN || internal_tag==TAG_END) {
	    internal_mode=MODE_TAG_CLOSE;
	    return ITEM_NEXT;
	    }
	break;
	}
    return ITEM_FAIL;
    }

static int process_tag_close(void) {
//puts("* process_tag_close()");
    if (internal_symbol=='>') {
	internal_mode=MODE_UNDEF;
        internal_symbol=0;
        return ITEM_TAG;
    	}
    return ITEM_FAIL;
    }

/****************************************************************************/
static int process_comment_begin(void) {
//puts("* process_comment_begin()");
    if (internal_symbol=='-') {
	internal_mode++;
	internal_symbol=0;
	return ITEM_NEXT;
	}
    return ITEM_FAIL;
    }

static int process_comment(void) {
//puts("* process_comment()");
    switch (internal_symbol) {
    case EOF:
	return ITEM_FAIL;
    case '-':
	internal_mode++;
	internal_symbol=0;
	return ITEM_NEXT;
	break;
	}
    internal_mode=MODE_COMMENT;
    internal_symbol=0;
    return ITEM_NEXT;
    }

static int process_comment_end(void) {
//puts("* process_comment_end()");
    switch (internal_symbol) {
    case EOF: 
	return ITEM_FAIL;
    case '>': 
	internal_mode=MODE_UNDEF; 
	internal_symbol=0;
	return ITEM_NEXT;
    case '-':
	internal_symbol=0;
	return ITEM_NEXT;
	}
    internal_mode=MODE_COMMENT;
    internal_symbol=0;
    return ITEM_NEXT;
    }
/****************************************************************************/

int xml_nextItem(struct tag_struct * tag, char * text, size_t size) {
//puts("* xml_nextItem()");
    char * s;
    size_t l;
    int status;
    struct obstack pool;
    obstack_init(&pool);
    do {
	if (internal_symbol==0) internal_symbol=fgetc(f);
	switch (internal_mode) {
	case MODE_UNDEF:	status=process_undef(); break;
	case MODE_TEXT:		status=process_text(&pool); break;
	case MODE_TAG_BEGIN:	status=process_tag_begin(); break;
	case MODE_TAG_NAME:	status=process_tag_name(&pool); break;
	case MODE_TAG:		status=process_tag(); break;
	case MODE_PARAMETER:	status=process_parameter(&pool); break;
	case MODE_VALUE:	status=process_value(); break;
	case MODE_STRING:	status=process_string(&pool); break;
	case MODE_TAG_END:	status=process_tag_end(); break;
	case MODE_TAG_CLOSE:	status=process_tag_close(); break;
	case MODE_COMMENT_BEGIN:
	case MODE_COMMENT_BEGIN2:
				status=process_comment_begin(); break;
	case MODE_COMMENT:
	case MODE_COMMENT_END:
				status=process_comment(); break;
	case MODE_COMMENT_END2:
				status=process_comment_end(); break;
	default:
	    printf("*** xml_nextItem() ASSERT: unknown mode: %d\n",internal_mode);
	    status=ITEM_FAIL;
	    break;
	    }

	switch (status) {
	case ITEM_TEXT_EMPTY:
	    if (size==0) status=ITEM_NEXT;
//	    else {
		obstack_1grow(&pool, 0);
		l=obstack_object_size(&pool);
		s=obstack_finish(&pool);
		if (size>l) size=l;
		if (size>0) memcpy(text, s, size);
//		}
	    obstack_free(&pool, s);
	    break;
	case ITEM_TEXT_STRING:
	    if (size==0) status=ITEM_FAIL;
//	    else {
		obstack_1grow(&pool, 0);
		l=obstack_object_size(&pool);
		s=obstack_finish(&pool);
		if (size>l) size=l;
	        memcpy(text, s, size);
//	        }
	    obstack_free(&pool, s);
	    break;
	case ITEM_TAG_NAME:
	    obstack_1grow(&pool, 0);
	    s=obstack_finish(&pool);
	    tag_name(tag, s);
	    obstack_free(&pool, s);
	    status=ITEM_NEXT;
	    break;
	case ITEM_PARAMETER:
	    obstack_1grow(&pool, 0);
	    s=obstack_finish(&pool);
	    obstack_free(&pool, s);
	    tag_pname(tag, s);
	    status=ITEM_NEXT;
	    break;
	case ITEM_VALUE:
	    obstack_1grow(&pool, 0);
	    s=obstack_finish(&pool);
	    tag_pvalue(tag, s);
	    obstack_free(&pool, s);
	    status=ITEM_NEXT;
	    break;
	case ITEM_TAG:
	    tag->type=internal_tag;
	    break;
	case ITEM_NEXT:
	case ITEM_FAIL:
	case ITEM_END:
	    break;
	default:
	    printf("*** xml_nextItem() ASSERT: unknown status: %d\n", status);
	    status=ITEM_FAIL;
	    break;
	    }
	} while (status==ITEM_NEXT);
    obstack_free(&pool, NULL);
    return status;
    }
