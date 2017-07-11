#include "netclockconfig.h"
#include "netclock.h"
#include "netclock_httpd.h"
#define Eland_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)

ELAND_DES_S *netclock_des_g = NULL;
static bool NetclockInitSuccess = false;

static mico_semaphore_t NetclockInitCompleteSem = NULL;
json_object *ElandJsonData = NULL;
OSStatus netclock_desInit(void)
{
    OSStatus err = kGeneralErr;
    if (false == CheckNetclockDESSetting())
    {
        //结构体覆盖
        Eland_log("[NOTICE]recovery settings!!!!!!!");
        err = Netclock_des_recovery();
        require_noerr(err, exit);
    }
    Eland_log("local firmware version:%s", netclock_des_g->ElandFirmwareVersion);
    if (netclock_des_g->IsActivate == false)
    {
        Eland_log("netclock_AP");
        mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Para_config",
                                ElandParameterConfiguration, 0x1000, (uint32_t)NULL);
    }

    return kNoErr;
exit:
    Eland_log("netclock_des_g init err");
    if (netclock_des_g != NULL)
    {
        memset(netclock_des_g, 0, sizeof(ELAND_DES_S));
        mico_system_context_update(mico_system_context_get());
    }

    return kGeneralErr;
}
/**/
OSStatus InitNetclockService(void)
{
    OSStatus err = kGeneralErr;
    mico_Context_t *context = NULL;

    context = mico_system_context_get();
    require_action_string(context != NULL, exit, err = kGeneralErr, "[ERROR]context is NULL!!!");

    netclock_des_g = (ELAND_DES_S *)mico_system_context_get_user_data(context);
    require_action_string(netclock_des_g != NULL, exit, err = kGeneralErr, "[ERROR]fog_des_g is NULL!!!");
    err = kNoErr;
exit:
    return err;
}
//闹钟初始化
static void NetclockInit(mico_thread_arg_t arg)
{
    UNUSED_PARAMETER(arg);
    OSStatus err = kGeneralErr;

    require_string(get_wifi_status() == true, exit, "wifi is not connect");
    /*初始化互斥信号量*/
    //err = mico_rtos_init_mutex(&http_send_setting_mutex);
    //require_noerr(err, exit);

    //开启http client 后台线程
    //err = start_fogcloud_http_client(); //内部有队列初始化,需要先执行 //ssl登录等
    require_noerr(err, exit);

//startsNetclockinit:

exit:
    mico_rtos_set_semaphore(&NetclockInitCompleteSem); //wait until get semaphore

    // if (err != kNoErr)
    // {
    //     stop_fog_bonjour();
    // }

    mico_rtos_delete_thread(NULL);
    return;
}
OSStatus StartNetclockService(void)
{
    OSStatus err = kGeneralErr;
    NetclockInitSuccess = false;

    err = mico_rtos_init_semaphore(&NetclockInitCompleteSem, 1);
    require(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "NetclockInit", NetclockInit, 0x1000, (uint32_t)NULL);
    require(err, exit);

    mico_rtos_get_semaphore(&NetclockInitCompleteSem, MICO_WAIT_FOREVER);
exit:
    return err;
}
//检查从flash读出的数据是否正确，不正确就恢复出厂设置
bool CheckNetclockDESSetting(void)
{
    char firmware[64] = {0};
    sprintf(firmware, "%s", Eland_Firmware_Version);
    /*check Eland_ID*/
    if (0 != strcmp(netclock_des_g->ElandID, Eland_ID))
    {
        Eland_log("ElandID change!");
        return false;
    }
    /*check ElandFirmwareVersion*/
    if (0 != strcmp(netclock_des_g->ElandFirmwareVersion, firmware))
    {
        Eland_log("ElandFirmwareVersion changed!");
        return false;
    }

    /*check MAC Address*/
    if (strlen(netclock_des_g->ElandMAC) != DEVICE_MAC_LEN)
    {
        Eland_log("MAC Address err!");
        return false;
    }

    if (netclock_des_g->IsActivate == true) //设备已激活
    {
        /*check UserID */
        if ((netclock_des_g->ElandZoneOffset < Timezone_offset_sec_Min) ||
            (netclock_des_g->ElandZoneOffset > Timezone_offset_sec_Max))
        {
            Eland_log("ElandZoneOffset is error!");
            return false;
        }
    }
    return true;
}
OSStatus Netclock_des_recovery(void)
{
    unsigned char mac[10] = {0};

    memset(netclock_des_g, 0, sizeof(ELAND_DES_S)); //全局变量清空
    mico_system_context_update(mico_system_context_get());

    netclock_des_g->IsActivate = false;
    netclock_des_g->IsHava_superuser = false;
    netclock_des_g->IsRecovery = false;

    memcpy(netclock_des_g->ElandID, Eland_ID, strlen(Eland_ID));                                          //Eland唯一识别的ID    memcpy(netclock_des_g->ElandFirmwareVersion, Eland_Firmware_Version, strlen(Eland_Firmware_Version));//设置设备软件版本号
    memcpy(netclock_des_g->ElandFirmwareVersion, Eland_Firmware_Version, strlen(Eland_Firmware_Version)); //设置设备软件版本号
    wlan_get_mac_address(mac);                                                                            //MAC地址
    memset(netclock_des_g->ElandMAC, 0, sizeof(netclock_des_g->ElandMAC));                                //MAC地址
    sprintf(netclock_des_g->ElandMAC, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Eland_log("device_mac:%s", netclock_des_g->ElandMAC);

    mico_system_context_update(mico_system_context_get());

    return kNoErr;
}
void ElandParameterConfiguration(mico_thread_arg_t args)
{
    network_InitTypeDef_st wNetConfig;
    Eland_log("Soft_ap_Server");

    memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));

    strcpy((char *)wNetConfig.wifi_ssid, ELAND_AP_SSID);
    strcpy((char *)wNetConfig.wifi_key, ELAND_AP_KEY);

    wNetConfig.wifi_mode = Soft_AP;
    wNetConfig.dhcpMode = DHCP_Server;
    wNetConfig.wifi_retry_interval = 100;
    strcpy((char *)wNetConfig.local_ip_addr, "192.168.0.1");
    strcpy((char *)wNetConfig.net_mask, "255.255.255.0");
    strcpy((char *)wNetConfig.dnsServer_ip_addr, "192.168.0.1");

    Eland_log("ssid:%s  key:%s", wNetConfig.wifi_ssid, wNetConfig.wifi_key);
    micoWlanStart(&wNetConfig);

    /* start http server thread */
    Eland_httpd_start();
    mico_rtos_delete_thread(NULL);
}
//获取网络连接状态
bool get_wifi_status(void)
{
    LinkStatusTypeDef link_status;

    memset(&link_status, 0, sizeof(link_status));

    micoWlanGetLinkStatus(&link_status);

    return (bool)(link_status.is_connected);
}

const char Eland_Data[11] = {"ElandData"};

/********
{"name":"ElandData","V":[{"N":"ElandID","Type":"str","V":"","T":"R"},{"N":"UserID","Type":"int32","V":0,"T":"W"},{"N":"N","Type":"str","V":"ElandName","T":"W"},{"N":"ZoneOffset","Type":"int32","V":32400,"T":"W"},{"N":"Serial","Type":"str","V":"000000","T":"R"},{"N":"FirmwareVersion","Type":"str","V":"01.00","T":"R"},{"N":"MAC","Type":"str","V":"000000000000","T":"R"},{"N":"DHCPEnable","Type":"int32","V":1,"T":"W"},{"N":"IPstr","Type":"str","V":"0","T":"W"},{"N":"SubnetMask","Type":"str","V":"0","T":"W"},{"N":"DefaultGateway","Type":"str","V":"0","T":"W"},{"N":"BackLightOffEnable","Type":"int32","V":1,"T":"W"},{"N":"BackLightOffBeginTime","Type":"str","V":"05:00:00","T":"W"},{"N":"BackLightOffEndTime","Type":"str","V":"05:10:00","T":"W"},{"N":"BackLightOffEndTime","Type":"str","V":"05:10:00","T":"W"},{"N":"FirmwareUpdateUrl","Type":"str","V":"url","T":"W"},[{"N":"AlarmData","V":[{"N":"AlarmID","Type":"int32","V":1,"T":"W"},{"N":"AlarmDateTime","Type":"str","V":"yyyy-MM-dd","T":"W"},{"N":"SnoozeEnabled","Type":"int32","V":0,"T":"W"},{"N":"SnoozeCount","Type":"int32","V":3,"T":"W"},{"N":"SnoozeIntervalMin","Type":"int32","V":10,"T":"W"},{"N":"AlarmPattern","Type":"int32","V":1,"T":"W"},{"N":"AlarmSoundDounloadURL","Type":"str","V":"url","T":"W"},{"N":"AlarmVoiceDounloadURL","Type":"str","V":"url","T":"W"},{"N":"AlarmVolume","Type":"int32","V":80,"T":"W"},{"N":"VolumeStepupEnabled","Type":"int32","V":0,"T":"W"},{"N":"AlarmContinueMin","Type":"int32","V":5,"T":"W"}]}]]}

*******/
OSStatus InitUpLoadData(void)
{
    OSStatus err = kNoErr;
    if (ElandJsonData != NULL)
    {
        free_json_obj(&ElandJsonData);
    }
    ElandJsonData = json_object_new_object();
    require_action_string(mico_data, exit, err = kNoMemoryErr, "create json object error!");

exit:
    return err;
}

//释放JSON对象
void free_json_obj(json_object **json_obj)
{
    if (*json_obj != NULL)
    {
        json_object_put(*json_obj); // free memory of json object
        *json_obj = NULL;
    }

    return;
}
