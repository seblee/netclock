/**
 ******************************************************************************
 * @file    app_https.c
 * @author  QQ DING
 * @version V1.0.0
 * @date    1-September-2015
 * @brief   The main HTTPD server initialization and wsgi handle.
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 */

#include "netclock_httpd.h"
#include "netclock.h"
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include "mico.h"
#include "httpd_priv.h"

#define app_httpd_log(M, ...) custom_log("apphttpd", M, ##__VA_ARGS__)

#define HTTPD_HDR_DEFORT (HTTPD_HDR_ADD_CONN_CLOSE)
static bool is_http_init;
static bool is_handlers_registered;
struct httpd_wsgi_call g_app_handlers[];

static int web_send_Get_Request(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    char *upload_data = NULL;
    uint32_t upload_data_len = 1024;
    app_httpd_log("web_send_Get_Request");
    upload_data = malloc(upload_data_len);
    memset(upload_data, 0, upload_data_len);
    InitUpLoadData(upload_data);

    err = httpd_send_all_header(req, HTTP_RES_200, strlen(upload_data), HTTP_CONTENT_JSON_STR);
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

    err = httpd_send_body(req->sock, (const unsigned char *)upload_data, strlen(upload_data));
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));

exit:
    if (upload_data != NULL) //回收资源
    {
        free(upload_data);
        upload_data = NULL;
    }
    destory_upload_data(); //回收资源
    return err;
}
static int web_send_Post_Request(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    int buf_size = 1024;
    char *buf = NULL;
    mico_Context_t *context = NULL;
    bool para_succ = false;
    app_httpd_log("web_send_Post_Request");
    buf = malloc(buf_size);
    memset(buf, 0, buf_size);
    err = httpd_get_data(req, buf, buf_size);
    app_httpd_log("size = %d,buf = %s", req->body_nbytes, buf);
    if (ProcessPostJson(buf) == kNoErr)
    {
        context = mico_system_context_get();
        strncpy(context->micoSystemConfig.ssid, netclock_des_g->Wifissid, ElandSsid_Len);
        strncpy(context->micoSystemConfig.key, netclock_des_g->WifiKey, ElandKey_Len);
        strncpy(context->micoSystemConfig.user_key, netclock_des_g->WifiKey, ElandKey_Len);
        context->micoSystemConfig.keyLength = strlen(context->micoSystemConfig.key);
        context->micoSystemConfig.user_keyLength = strlen(context->micoSystemConfig.key);

        context->micoSystemConfig.channel = 0;
        memset(context->micoSystemConfig.bssid, 0x0, 6);
        context->micoSystemConfig.security = SECURITY_TYPE_AUTO;
        context->micoSystemConfig.dhcpEnable = true;

        para_succ = true;
    }
    app_httpd_log("web_send_Get_Request");
    memset(buf, 0, buf_size);
    InitUpLoadData(buf);
    err = httpd_send_all_header(req, HTTP_RES_200, strlen(buf), HTTP_CONTENT_JSON_STR);
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

    err = httpd_send_body(req->sock, (const unsigned char *)buf, strlen(buf));
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
    if (para_succ == true)
    {
        context->micoSystemConfig.configured = allConfigured;

        mico_system_context_update(context);

        mico_system_power_perform(context, eState_Software_Reset);
    }
    free(buf);
    buf = NULL;
exit:
    return err;
}
/*
static int web_send_result_page(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    bool para_succ = false;
    int buf_size = 512;
    char *buf;
    char value_ssid[maxSsidLen];
    char value_pass[maxKeyLen];
    mico_Context_t *context = NULL;

    context = mico_system_context_get();

    buf = malloc(buf_size);

    err = httpd_get_data(req, buf, buf_size);
    require_noerr(err, Save_Out);

    err = httpd_get_tag_from_post_data(buf, "SSID", value_ssid, maxSsidLen);
    require_noerr(err, Save_Out);

    if (!strncmp(value_ssid, "\0", 1))
        goto Save_Out;

    strncpy(context->micoSystemConfig.ssid, value_ssid, maxSsidLen);

    err = httpd_get_tag_from_post_data(buf, "PASS", value_pass, maxKeyLen);
    require_noerr(err, Save_Out);

    strncpy(context->micoSystemConfig.key, value_pass, maxKeyLen);
    strncpy(context->micoSystemConfig.user_key, value_pass, maxKeyLen);
    context->micoSystemConfig.keyLength = strlen(context->micoSystemConfig.key);
    context->micoSystemConfig.user_keyLength = strlen(context->micoSystemConfig.key);

    context->micoSystemConfig.channel = 0;
    memset(context->micoSystemConfig.bssid, 0x0, 6);
    context->micoSystemConfig.security = SECURITY_TYPE_AUTO;
    context->micoSystemConfig.dhcpEnable = true;

    para_succ = true;

Save_Out:

    if (para_succ == true)
    {
        err = httpd_send_all_header(req, HTTP_RES_200, sizeof(Eland_Data), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisuccess headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)Eland_Data, sizeof(Eland_Data));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisuccess body."));

        context->micoSystemConfig.configured = allConfigured;

        mico_system_context_update(context);

        mico_system_power_perform(context, eState_Software_Reset);
    }
    else
    {
        err = httpd_send_all_header(req, HTTP_RES_200, sizeof(Eland_Data), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wififail headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)Eland_Data, sizeof(Eland_Data));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wififail body."));
    }

exit:
    if (buf)
        free(buf);
    return err;
}
*/
struct httpd_wsgi_call g_app_handlers[] = {
    {"/", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_Request, NULL, NULL},
    {"/result.htm", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_Request, NULL, NULL},
    {"/setting.htm", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_Request, NULL, NULL},
};

static int g_app_handlers_no = sizeof(g_app_handlers) / sizeof(struct httpd_wsgi_call);

static void app_http_register_handlers()
{
    int rc;
    rc = httpd_register_wsgi_handlers(g_app_handlers, g_app_handlers_no);
    if (rc)
    {
        app_httpd_log("failed to register test web handler");
    }
}

static int _app_httpd_start()
{
    OSStatus err = kNoErr;
    app_httpd_log("initializing web-services");

    /*Initialize HTTPD*/
    if (is_http_init == false)
    {
        err = httpd_init();
        require_noerr_action(err, exit, app_httpd_log("failed to initialize httpd"));
        is_http_init = true;
    }

    /*Start http thread*/
    err = httpd_start();
    if (err != kNoErr)
    {
        app_httpd_log("failed to start httpd thread");
        httpd_shutdown();
    }
exit:
    return err;
}

int Eland_httpd_start(void)
{
    OSStatus err = kNoErr;

    err = _app_httpd_start();
    require_noerr(err, exit);

    if (is_handlers_registered == false)
    {
        app_http_register_handlers();
        is_handlers_registered = true;
    }

exit:
    return err;
}

int Eland_httpd_stop()
{
    OSStatus err = kNoErr;

    /* HTTPD and services */
    app_httpd_log("stopping down httpd");
    err = httpd_stop();
    require_noerr_action(err, exit, app_httpd_log("failed to halt httpd"));

exit:
    return err;
}
