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
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "numeric.h"

#define PLUS	0
#define MINUS	!PLUS

void num_init(num_t * num, int p) {
//puts("* num_init()");
    num->sign=PLUS;
    num->value=0;
    num->precision=p;
    }

void num_clear(num_t * num) {
//puts("* num_init()");
    num->sign=PLUS;
    num->value=0;
    }

static void set_precision(num_t *num, int p) {
//puts("* set_precision()");
    int c=0;
    int i;
//printf("num_p=%d, p=%d\n",num->precision,p);
    while (num->precision<p) {
	num->value*=10;
	num->precision++;
	}
    while (num->precision>p) {
	c=num->value%10;
	num->value/=10;
	num->precision--;
	}
    if (c>=5) num->value++;
//printf("value=%Ld, precision=%d\n",num->value, num->precision);
    }

void num_dup(num_t * trg, num_t * src) {
//puts("* num_dup()");
    trg->precision=src->precision;
    trg->value=src->value;
    trg->sign=src->sign;
    }
    

void num_cpy(num_t * trg, num_t * src) {
//puts("* num_cpy()");
    num_t res;
    num_dup(&res,src);
    set_precision(&res, trg->precision);
    num_dup(trg,&res);
    }

void num_fromllong(num_t * trg, long long int n) {
//puts("* num_fromInt()");
    num_t num;
    num.precision=0;
    if (n<0) {
	num.value=-n;
	num.sign=MINUS;
	}
    else {
	num.value=n;
	num.sign=PLUS;
	}
    num_cpy(trg, &num);
    }

void num_fromDouble(num_t * num, double value) {
puts("* num_fromDouble()");
    int i;
    if (value<0) {
	num->sign=MINUS;
	value=-value;
	}
    else num->sign=PLUS;

    if (num->precision>0)
	for(i=0;i<num->precision;i++) value*=10;
    else         	
	for(i=0;i<-num->precision;i++) value/=10;

    num->value=floor(value);
    int c=floor((value-num->value)*10);
    if (c>=5) num->value++;
    }

void num_add(num_t *trg, num_t *src1, num_t *src2) {
//puts("* num_add()");
    num_t s1, s2, res;
    num_dup(&s1, src1);
    num_dup(&s2, src2);

    if (s1.precision>s2.precision) set_precision(&s2,s1.precision);
    if (s2.precision>s1.precision) set_precision(&s1,s2.precision);
    
    num_init(&res,s1.precision);
    
    if (s1.sign==s2.sign) res.value=s1.value+s2.value;
    else {
	if (s1.value>s2.value) {
	    res.sign=s1.sign;
	    res.value=s1.value-s2.value;
	    }

	if (s2.value>s1.value) {
	    res.sign=s2.sign;
	    res.value=s2.value-s1.value;
	    }
	}
    num_cpy(trg,&res);
    }

void num_addllong(num_t *trg, num_t *src, long long int n) {
//puts("* num_addInt()");
    num_t num;
    num_init(&num,0);
    num_fromllong(&num,n);
    num_add(trg,src,&num);
    }

void num_negThis(num_t *n) {
    n->sign=!n->sign;
    }

void num_sub(num_t *trg, num_t *src1, num_t *src2) {
//puts("* num_sub()");
    num_t num;
    num_dup(&num, src2);
    num_negThis(&num);
    num.sign=!src2->sign;
    num_add(trg,src1,&num);
    }

int num_cmp(num_t *v1, num_t *v2) {
    if (v1->sign==PLUS && v2->sign==PLUS) {
	if (v1->value<v2->value) return -1;
	if (v1->value>v2->value) return 1;
	return 0;
	}
    if (v1->sign==MINUS && v2->sign==MINUS) {
	if (v1->value<v2->value) return 1;
	if (v1->value>v2->value) return -1;
	return 0;
	}

    if (v1->sign==PLUS && v2->sign==MINUS) {
	if (v2->value==0) {
	    puts("*** num_cmp ASSERT: v2=-0");
	    v2->sign=PLUS;
	    return num_cmp(v1,v2);
	    }
	return 1;
	}

    if (v1->sign==MINUS && v2->sign==PLUS) {
	if (v1->value==0) {
	    puts("*** num_cmp ASSERT: v2=-0");
	    v1->sign=PLUS;
	    return num_cmp(v1,v2);
	    }
	return -1;
	}
    }

int num_cmp0(num_t *v) {
    if (v->value==0) return 0;
    return (v->sign==PLUS)?1:-1;
    }

static void num_dropZero(num_t *num) {
//puts("* num_dropZero()");
//printf("precision1: %d\n",num->precision);
    if (num->value>0) 
	while (num->value%10==0) {
	    num->value/=10;
	    num->precision--;
	    }
//printf("precision2: %d\n",num->precision);
    }
    	
void num_mul(num_t *trg, num_t *src1, num_t *src2) {
//puts("* num_mul()");
    num_t res;
    num_t s1,s2;
    num_dup(&s1,src1);
    num_dropZero(&s1);
    num_dup(&s2,src2);
    num_dropZero(&s2);
    res.precision=s1.precision+s2.precision;
    res.value=s1.value*s2.value;
//printf("res.value=%d\n",res.value);
    res.sign=s1.sign || s2.sign;
    num_cpy(trg,&res);
//printf("trg.value=%d\n",trg->value);
    }

void num_div(num_t *trg, num_t *src1, num_t *src2) {
//puts("* num_div()");
    num_t res;
    num_t s1,s2;
    num_dup(&s1,src1);
    num_dropZero(&s1);
    num_dup(&s2,src2);
    num_dropZero(&s2);
    if (s1.precision-s2.precision<=trg->precision)
	set_precision(&s1,s2.precision+trg->precision+1);
//printf("s1.value=%Lu\n",s1.value);
//printf("s2.value=%Lu\n",s2.value);

    res.precision=s1.precision-s2.precision;
    res.value=s1.value/s2.value;
//printf("res.value=%Lu\n",res.value);
    res.sign=s1.sign || s2.sign;
    num_cpy(trg,&res);
    }

void num_divllong(num_t *trg, num_t *src, long long int n) {
    num_t num;
    num_init(&num,0);
    num_fromllong(&num,n);
    num_div(trg,src,&num);
    }

void num_mulllong(num_t *trg, num_t *src, long long int n) {
    num_t num;
    num_init(&num,0);
    num_fromllong(&num,n);
    num_mul(trg,src,&num);
    }

void num_toString(char *trg, num_t * num) {
    char data[256];
    bzero(data,sizeof(data));
    memset(data,'0',sizeof(data)-1);
    
//printf("value: %Lu\n",num->value);
    char str[16];
    bzero(str,sizeof(str));
    sprintf(str,"%Lu",num->value);
    int len=strlen(str);
    int p=num->precision;
    size_t s=(p<len)?0:(p-len+1);
    size_t l1=(p<len)?len-p:1;
//printf("цел: %d\n",l1);
    size_t l2=(p>0)?p:0;
//printf("дроб: %d\n",l2);
    memcpy(data+s,str,len);
    
    char str1[256];
    bzero(str1,sizeof(str1));
    
    char str2[256];
    bzero(str2,sizeof(str2));
    
    memcpy(str1,data,l1);
    memcpy(str2,data+l1,l2);
    if (num->sign) strcpy(trg,"-");
    else *trg=0;
    strcat(trg,str1);
    if (num->precision>0) {
	strcat(trg,".");
	strcat(trg,str2);
	}
//printf("num_toString(): <%s>\n",trg);
    }

double num_toDouble(num_t *num) {
    double d=(num->sign)?(double)(-num->value):(double)num->value;
    int i;
    if (num->precision>0)
	for(i=0;i<num->precision;i++) d/=10;

    if (num->precision<0)
	for(i=0;i<-num->precision;i++) d*=10;
    return d;
    }

long long int num_tollong(num_t *src) {
    num_t n;
    num_dup(&n,src);
    set_precision(&n,0);
    if (n.sign==PLUS) return n.value;
    return -n.value;
    }

static int digit(char c) {
    switch(c) {
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    default: return 0;
	}
    }
    
void num_fromString(num_t * trg, char *str) {
    NUMERIC(num,0);

    while (*str==' ') str++;

    if (*str=='+') str++;
    else if(*str=='-') {
	num.sign=MINUS;
	*str++;
	}
    int exit=0;
    int point=0;
    do {
	switch (*str) {
        case '0' ... '9':
	    num.value=num.value*10+digit(*str);
	    num.precision+=point;
	    break;

        case '.':
	case ',':
	    if (point) exit=-1;
	    point=1;
	    break;
	default: exit=-1;
	    }
	str++;
	} while (!exit);

    num_cpy(trg,&num);
    }
