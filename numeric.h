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


#ifndef NUMERIC_H
#define NUMERIC_H

typedef struct {
    int sign;
    unsigned long long int value;
    int precision;
    } num_t;

void num_init(num_t * num, int p);
void num_clear(num_t * num);

void num_dup(num_t * trg, num_t * src);
void num_cpy(num_t * trg, num_t * src);

// возвращает 0, если числа равны,
//	     -1, если v1<v2,
//            1, если v1>v2
int num_cmp(num_t *v1, num_t *v2);
int num_cmp0(num_t *v);		// сравнивает с 0

void num_fromllong(num_t * trg, long long int n);
#define num_fromint(_trg, _src)	num_fromllong(_trg, (long long int)(_src))
#define num_fromlong(_trg, _src) num_fromllong(_trg, (long long int)(_src))

void num_fromDouble(num_t * num, double value);
#define num_fromFloat(_trg, _src) num_fromDouble(_trg, (double)(_src))

void num_add(num_t *trg, num_t *src1, num_t *src2);
#define num_addThis(_trg, _src)	num_add(_trg, _trg, _src)

void num_addllong(num_t *trg, num_t *src, long long int n);
#define num_addlong(_trg, _src, _n) num_addllong(_trg, _src, (long long int)(_n))
#define num_addint(_trg, _src, _n) num_addllong(_trg, _src, (long long int)(_n))

void num_negThis(num_t *n);
#define num_neg(_trg, _src)	{num_cpy(_trg, _src); num_negThis(_trg);}

void num_sub(num_t *trg, num_t *src1, num_t *src2);
#define num_subThis(_trg, _src)	num_sub(_trg, _trg, _src)

void num_mul(num_t *trg, num_t *src1, num_t *src2);
void num_mulllong(num_t *trg, num_t *src, long long int n);
#define num_mullong(_trg, _src, _n) num_mulllong(_trg, _src, (long long int)(_n))
#define num_mulint(_trg, _src, _n) num_mulllong(_trg, _src, (long long int)(_n))

#define num_mulintThis(_trg, _n) num_mulllong(_trg, _trg, (long long int)(_n))

void num_div(num_t *trg, num_t *src1, num_t *src2);
void num_divllong(num_t *trg, num_t *src, long long int num);
#define num_divlong(_trg, _src, _n) num_divllong(_trg, _src, (long long int)(_n))
#define num_divint(_trg, _src, _n) num_divllong(_trg, _src, (long long int)(_n))

#define num_divintThis(_trg, _n) num_divllong(_trg, _trg, (long long int)(_n))

void num_toString(char *str, num_t * num);

double num_toDouble(num_t *num);

long long int num_tollong(num_t *src);

void num_fromString(num_t * trg, char *str);

#define NUMERIC(_num, _precision)	num_t _num; num_init(&_num ,_precision);

#endif //NUMERIC_H
