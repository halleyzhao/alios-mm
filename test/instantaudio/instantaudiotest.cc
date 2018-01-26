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

#include <gtest/gtest.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "multimedia/instantaudio.h"
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
#include <multimedia/mm_amhelper.h>
#endif

#include "multimedia/mm_debug.h"
MM_LOG_DEFINE_MODULE_NAME("INSTANTAUDIOTEST")
MM_LOG_DEFINE_LEVEL(MM_LOG_DEBUG)

using namespace std;
using namespace YUNOS_MM;

static const char *resouce_uris[] = {
    "/usr/bin/ut/res/audio/tick.ogg"
};

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
static MMAMHelper * s_g_amHelper = NULL;
#endif

static int s_g_resource_count = sizeof(resouce_uris) / sizeof(char*);

#define MAX_CHANNELS 10

static int s_g_loaded_count = 0;

InstantAudio *s_g_instantaudio = NULL;
bool s_g_loaded = false;
int s_g_sample_id = -1;
FILE *g_fp = NULL;

class InstantAudioTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

class MyListener : public InstantAudio::Listener {
    virtual void onMessage(int msg, int param1, int param2, const MMParam *obj)
    {
        MMLOGD("msg: %d, param1: %d, param2: %d, obj: %p", msg, param1, param2, obj);
        switch ( msg ) {
            case URI_SAMPLE_LOADED:
                {
                    s_g_sample_id = param2;
                    const char * uri = obj->readCString();
                    ASSERT_NE(uri, (void *)NULL);
                    EXPECT_EQ(param1, MM_ERROR_SUCCESS);
                    if ( param1 == MM_ERROR_SUCCESS ) {
                        MMLOGI("%s load success, id: %d\n", uri, param2);
                        s_g_loaded = true;

                    } else {
                        MMLOGI("%s load fail\n", uri);
                    }
                }
                ++s_g_loaded_count;
                break;
            case FD_SAMPLE_LOADED:
                {
                    EXPECT_EQ(param1, MM_ERROR_SUCCESS);
                    if ( param1 == MM_ERROR_SUCCESS ) {
                        int id = -1;
                        obj->readInt32(&id);
                        s_g_sample_id = id;
                        s_g_loaded = true;
                        MMLOGI("%d fd load success , sample_id =%d \n", param2, s_g_sample_id);
                    } else {
                        MMLOGI("%d fd load fail\n", param2);
                    }
                }
                ++s_g_loaded_count;
                break;
            case BUFFER_SAMPLE_LOADED:
                {
                    s_g_sample_id = param2;
                    int32_t id = obj->readInt32();
                    EXPECT_EQ(param1, MM_ERROR_SUCCESS);
                    if ( param1 == MM_ERROR_SUCCESS ) {
                        MMLOGI("%d load success, id: %d\n", id, param2);
                    } else {
                        MMLOGI("%d load fail\n", id);
                    }
                    s_g_loaded = true;
                }
                ++s_g_loaded_count;
                break;
            default:
                break;
        }
    }
};


MyListener * s_g_listener = NULL;

static void test()
{
    mm_status_t ret;
    int channelID ,length ;
    FILE *fp = NULL;
    const char *file_uri = NULL;
    InstantAudio::VolumeInfo vol;
    vol.left = vol.right = 1.0;
    MMIDBufferSP buf;

    MMLOGD("test instantaudio case1: \n");
    for ( int i = 0; i < s_g_resource_count; ++i ) {
        ret = s_g_instantaudio->load(resouce_uris[i]);
        if ( ret != MM_ERROR_SUCCESS ) {
                MMLOGE("calling load %s failed\n", resouce_uris[i]);
                if(fp){
                     fclose(fp);
                     fp = NULL;
                }
        }
        ASSERT_EQ(ret, MM_ERROR_SUCCESS);
     }

     while (1){
        if(s_g_loaded) {
            MMLOGI("instantaudio load complete\n");
            break;
        }
        usleep(1000);
    }

    vol.left = 1.0;
    vol.right = 1.0;
    MMLOGI("calling play\n");
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    MMLOGI("dev: %s\n", s_g_amHelper->getConnectionId());
    channelID = s_g_instantaudio->play(s_g_sample_id, &vol, 0, 1.0, 1, s_g_amHelper->getConnectionId());
#else
    channelID = s_g_instantaudio->play(s_g_sample_id, &vol, 0, 1.0, 1, 3);
#endif
    MMLOGI("calling play over: %d\n", channelID);

    MMLOGI("sleep for a while\n");
    usleep(2000000);
    MMLOGI("sleep for a while over\n");

    s_g_instantaudio->unloadAll();
    MMLOGD("test instantaudio case2: \n");
    //reset global variables
    s_g_loaded = false;
    s_g_sample_id = -1;
    //test load by fd with the first resource uri
    file_uri = resouce_uris[0];
    g_fp = fopen(file_uri, "r");
    ASSERT_NE(g_fp, NULL);

    fseek(g_fp,0,SEEK_END);
    length = ftell(g_fp);
    printf("file length =%d \n",length);
    ret = s_g_instantaudio->load(fileno(g_fp),1024, 15807);
    if ( ret != MM_ERROR_SUCCESS ) {
            MMLOGE("calling load  %s  by fd failed\n", file_uri);
            if(fp){
                     fclose(fp);
                     fp = NULL;
            }
        }

    ASSERT_EQ(ret, MM_ERROR_SUCCESS);

    while (1){
        if(s_g_loaded) {
            MMLOGI("instantaudio load fd complete\n");
            break;
        }
        usleep(1000);
    }

    MMLOGI("calling play instantaudio load by fd\n");
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    channelID = s_g_instantaudio->play(s_g_sample_id, &vol, 0, 1.0, 1, s_g_amHelper->getConnectionId());
#else
    channelID = s_g_instantaudio->play(s_g_sample_id, &vol, 0, 1.0, 1, 3);
#endif

    MMLOGI("calling play over: %d\n", channelID);

    s_g_instantaudio->setLoop(channelID ,3);
    s_g_instantaudio->setRate(channelID, 1.0);
    s_g_instantaudio->setPriority(channelID, 1);
    s_g_instantaudio->setVolume(channelID , &vol);

    MMLOGI("sleep for a short while\n");
    usleep(50000);
    MMLOGI("sleep over, calling pause\n");

    ret = s_g_instantaudio->pause(channelID);
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    printf("pause ok \n");

    ret = s_g_instantaudio->resume(channelID);
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    printf("resume ok \n");

    usleep(50000);
    ret = s_g_instantaudio->pauseAll();
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    printf("pause all ok \n");

    usleep(50000);
    ret = s_g_instantaudio->resumeAll();
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    printf("resume all ok \n");

    MMLOGI("sleep for a while\n");
    usleep(2000000);
    MMLOGI("sleep for a while over\n");
    s_g_instantaudio->stop(channelID);

    usleep(50000);
    s_g_instantaudio->unload(channelID);

    fclose(g_fp);
    g_fp = NULL;
    MMLOGD("test instantaudio case3: \n");
    //reset global variables
    s_g_loaded = false;
    s_g_sample_id = -1;
    file_uri = resouce_uris[0];
    g_fp = fopen(file_uri, "r");
    ASSERT_NE(g_fp, NULL);

    fseek(g_fp,0,SEEK_END);
    length = ftell(g_fp);
    MMLOGI("file length =%d \n",length);
    fseek(g_fp,0,SEEK_SET);
    buf = MMIDBuffer::create(length, 1);
    if ( !buf ) {
            MMLOGE("no mem\n");
             if(fp){
                     fclose(fp);
                     fp = NULL;
            }
    }
    ASSERT_NE(buf.get(), NULL);

    uint8_t * theBuf = buf->getWritableBuffer();
    size_t readret = fread(theBuf, 1, length, g_fp);
    MMLOGI("read ret: %u\n", readret);
    buf->setActualSize(length);
    ret = s_g_instantaudio->load(buf);
    if ( ret != MM_ERROR_SUCCESS ) {
            MMLOGE("calling load 1 failed\n");
             if(fp){
                     fclose(fp);
                     fp = NULL;
            }
    }
    ASSERT_EQ(ret, MM_ERROR_SUCCESS);

    while (1){
       if(s_g_loaded) {
           MMLOGI("instantaudio load fd complete\n");
           break;
        }
        usleep(1000);
    }

    MMLOGI("calling play\n");
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    channelID = s_g_instantaudio->play(s_g_sample_id, &vol, 0, 1.0, 1, s_g_amHelper->getConnectionId());
#else
    channelID = s_g_instantaudio->play(s_g_sample_id, &vol, 0, 1.0, 1, 3);
#endif
    MMLOGI("calling play over: %d\n", channelID);

    MMLOGI("sleep for a while\n");
    usleep(2000000);
    MMLOGI("sleep for a while over\n");

    s_g_instantaudio->unloadAll();
}

TEST_F(InstantAudioTest, loopPlayTest) {
    s_g_instantaudio = InstantAudio::create(MAX_CHANNELS);
    ASSERT_NE(s_g_instantaudio, NULL);

    s_g_listener = new MyListener();
    ASSERT_NE(s_g_listener, NULL);

    s_g_instantaudio->setListener(s_g_listener);
    for ( int i = 0; i < 5; ++i ) {
        MMLOGI("testing No. %d\n", i);
        test();
        usleep(1000000);
    }
}

int main(int argc, char* const argv[]) {
   int ret;
   MMLOGI("hello instantaudio\n");
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    try {
        s_g_amHelper = new MMAMHelper();
        if (s_g_amHelper->connect() != MM_ERROR_SUCCESS) {
            MMLOGE("failed to connect audiomanger\n");
            delete s_g_amHelper;
            return -1;
        }
    } catch (...) {
        MMLOGE("failed to new amhelper\n");
        return -1;
    }
#endif
   try {
         ::testing::InitGoogleTest(&argc, (char **)argv);
         ret = RUN_ALL_TESTS();
     } catch (...) {
         MMLOGE("InitGoogleTest failed!");
         ret = -1;
  }

  printf("test over \n");
  if(g_fp){
         fclose(g_fp);
         g_fp = NULL;
    }

  if(s_g_instantaudio){
        InstantAudio::destroy(s_g_instantaudio);
        s_g_instantaudio = NULL;
    }
    printf("InstantAudio::destroy over \n");

    if(s_g_listener){
        MM_RELEASE(s_g_listener);
        s_g_listener = NULL;
    }
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    s_g_amHelper->disconnect();
#endif
  MMLOGI("bye\n");
  return ret;
}



