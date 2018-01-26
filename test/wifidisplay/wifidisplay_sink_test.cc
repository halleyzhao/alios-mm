/**
 * Copyright (C) 2017 Alibaba Group Holding Limited. All Rights Reserved.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <stdint.h>
#include <cstdlib>
#include <glib.h>
#include <gtest/gtest.h>

#include "multimedia/media_meta.h"
#include <multimedia/mmmsgthread.h>
#include "multimedia/mm_debug.h"
#include "multimedia/media_attr_str.h"
#include <multimedia/mm_debug.h>
#include "WfdSession.h"
#include "WfdSinkSession.h"

using namespace YUNOS_MM;

typedef int status_t;
#define MM_LOG_TAG  "WifiDisplaySinkTest"

class  WfdRemoteDisplay3;

class WfdSession3CB : public IWfdSessionCB
{
public:
    WfdSession3CB(WfdRemoteDisplay3* owner) : mOwner(owner) {}
    virtual ~WfdSession3CB() {}

    virtual void onDisplayConnected( uint32_t width, uint32_t height,
                                     uint32_t flags, uint32_t session )
    {
        if (!mOwner)
            return;
        DEBUG("WfdSession3CB onDisplayConnected\n");
        //mOwner->onDisplayConnected(width, height, flags, session);
    }

    virtual void onDisplayDisconnected()
    {
        if (!mOwner)
            return;

        DEBUG("WfdSession3CB onDisplayDisconnected\n");
        //mOwner->onDisplayDisconnected();
    }

    virtual void onDisplayError(int32_t error)
    {
        if (!mOwner)
            return;

        DEBUG("WfdSession3CB onDisplayError\n");
        //mOwner->onDisplayError(error);
    }

private:
    WfdRemoteDisplay3* mOwner;
};

class  WfdRemoteDisplay3
{
public:
    WfdRemoteDisplay3( const char *iface )
    {
        DEBUG("create for iface [%s]", iface);

        mSessionListener = new WfdSession3CB(this);

        mSession = new WfdSinkSession(iface, mSessionListener);
        mm_status_t status = mSession->start();

        if (status != MM_ERROR_SUCCESS) {
            DEBUG("fail to start wfd session w/ mmstatus %d", status);
        }
    }

    ~WfdRemoteDisplay3()
    {
        DEBUG("destroy");

        delete mSession;
        delete mSessionListener;
    }

public:
    status_t pause()
    {
        DEBUG("pause");
        return 0;
    }

    status_t resume()
    {
        DEBUG("resume");
        return 0;
    }

    status_t dispose()
    {
        DEBUG("dispose");

        if (mSession)
            mSession->stop();

        DEBUG("session stopped");

        usleep(2*1000*1000);

        delete mSession;
        delete mSessionListener;

        mSession = NULL;
        mSessionListener = NULL;

        return 0;
    }

    void onDisplayConnected(
                         uint32_t width,
                         uint32_t height,
                         uint32_t flags,
                         uint32_t session)
    {
        DEBUG("width:%d,height:%d,flags:%d,session:%d\n",width,height,flags,session);
        return;
    }

    void onDisplayDisconnected()
    {
        DEBUG("this is onDisplayDisconnected");
        return;
    }

    void onDisplayError(int32_t error)
    {
        DEBUG("error:%d\n", error);
        return;
    }

public:
    WfdSinkSession *mSession;
    WfdSession3CB  *mSessionListener;

private:

};

static const char *g_remote_ip = NULL;
static int g_remote_port = 0;
static bool g_quit = false;

static GOptionEntry entries[] = {
    {"remoteIP", 'i', 0, G_OPTION_ARG_STRING, &g_remote_ip, " set remote ip", NULL},
    {"remotePort", 'p', 0, G_OPTION_ARG_INT, &g_remote_port, "set remote port ", NULL},
    {NULL}
};


class WIFIDisplaySinkTest : public testing::Test
{
protected:
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

void RunMainLoop()
{
    char ch;
    char buffer[128] = {0};
    static int flag_stdin = 0;
    //
    char buffer_back[128] = {0};
    ssize_t back_count = 0;

    ssize_t count = 0;

    flag_stdin = fcntl(STDIN_FILENO, F_GETFL);
    if(fcntl(STDIN_FILENO, F_SETFL, flag_stdin&(~O_NONBLOCK))== -1)
        printf("stdin_fileno set error");

    while (1)
    {
        if(g_quit)
            break;
        memset(buffer, 0, sizeof(buffer));
        if ((count = read(STDIN_FILENO, buffer, sizeof(buffer)-1) < 0)){
            printf("read from stdin err, debug me:%s\n", buffer);
            continue;
        }

        buffer[sizeof(buffer) - 1] = '\0';
        count = strlen(buffer);
        DEBUG("read return %zu bytes, %s\n", count, buffer);
        ch = buffer[0];
        if(ch == '-'){
            if (count < 2)
                continue;
             else
                ch = buffer[1];
        }
       if(ch == '\n' && buffer_back[0] != 0){
            printf("Repeat Cmd:%s", buffer_back);//buffer_back has a line change
            memcpy(buffer, buffer_back, sizeof(buffer));
            ch = buffer[0];
            if(ch == '-'){
                if (back_count < 2)
                    continue;
                else
                    ch = buffer[1];
            }
       }else{
            memset(buffer_back, 0, sizeof(buffer_back));
            //printf("deb%sg\n", buffer_back);
            memcpy(buffer_back, buffer, sizeof(buffer));
            back_count = count;
       }
       switch (ch) {
        case 'q':    // exit
            g_quit = true;
            break;

        case '-':
            //some misc cmdline
            break;

        default:
            printf("Unkonw cmd line:%c\n", ch);
            break;
        }
    }

    if(fcntl(STDIN_FILENO,F_SETFL,flag_stdin) == -1)
        printf("stdin_fileno set error");
    printf("\nExit RunMainLoop on yunostest.\n");
}


TEST_F(WIFIDisplaySinkTest, WFDSinkTest)
{
    char buf[128];
    WfdRemoteDisplay3 *sink = NULL;
    MMSharedPtr<WfdRemoteDisplay3> WfdSink;

    sprintf(buf, "%s:%d", g_remote_ip, g_remote_port);
    sink = new WfdRemoteDisplay3( buf );
    if (!sink)
        return;

    WfdSink.reset(sink);
    DEBUG("the wifidisplay sink setup success\n");
    RunMainLoop();
    DEBUG("start to exit the wifidisplay sink\n");
    WfdSink->dispose();
    WfdSink.reset();

    return;
}

int main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;
    std::string iface;
    int ret = -1;
    context = g_option_context_new(MM_LOG_TAG);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, TRUE);

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        DEBUG("option parsing failed: %s\n", error->message);
        goto _end;
    }
    g_option_context_free(context);

    if ( (g_remote_ip == NULL) || (g_remote_port == 0)) {
        DEBUG("please input the parameters:\n");
        DEBUG("remoteIp:%s,remotePort:%d\n", g_remote_ip, g_remote_port);
        goto _end;
    }

    try {
       ::testing::InitGoogleTest(&argc, (char **)argv);
       ret = RUN_ALL_TESTS();
    } catch (...) {
       DEBUG("InitGoogleTest failed!");
       goto _end;
    }

_end:
    if (g_remote_ip)
        g_free(g_remote_ip);
    return 0;
}
