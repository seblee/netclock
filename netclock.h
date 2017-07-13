/*
 * neckclock.h
 *
 *  Created on: 2017年7月10日
 *      Author: ceeu
 */

#ifndef NETCLOCK_NETCLOCK_H_
#define NETCLOCK_NETCLOCK_H_
#include "mico.h"
#include "netclockconfig.h"

extern const char Eland_Data[11];
extern json_object *ElandJsonData;
extern ELAND_DES_S *netclock_des_g;

OSStatus netclock_desInit(void);

OSStatus InitNetclockService(void);
OSStatus StartNetclockService(void);
bool CheckNetclockDESSetting(void);
OSStatus Netclock_des_recovery(void);
void ElandParameterConfiguration(mico_thread_arg_t args);

bool get_wifi_status(void);

void free_json_obj(json_object **json_obj);
void destory_upload_data(void);

OSStatus InitUpLoadData(char *OutputJsonstring);
OSStatus ProcessPostJson(char *InputJson);

#endif /* NETCLOCK_NETCLOCK_H_ */
