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


#ifndef ACTIONS_C
#define ACTIONS_C

int actions_processRegistration(void);
int actions_processStorno(void);
int actions_processPayment(void);
int actions_processCancel(void);
int actions_processReceipt(void);
int actions_processSession(void);

int actions_processConnect(char * n_kkm);
void actions_disconnect(void);

#endif //ACTIONS_C
