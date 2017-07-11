/*
 * neckclock.h
 *
 *  Created on: 2017年7月10日
 *      Author: ceeu
 */

#ifndef NETCLOCK_NETCLOCK_H_
#define NETCLOCK_NETCLOCK_H_
#include "mico.h"

extern const char Eland_Data[11];

OSStatus netclock_desInit(void);
OSStatus InitNetclockService(void);
OSStatus StartNetclockService(void);
bool CheckNetclockDESSetting(void);
OSStatus Netclock_des_recovery(void);
void ElandParameterConfiguration(mico_thread_arg_t args);
bool get_wifi_status(void);
#endif /* NETCLOCK_NETCLOCK_H_ */
