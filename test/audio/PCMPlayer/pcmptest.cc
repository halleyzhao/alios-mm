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
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <multimedia/PCMPlayer.h>
#include <multimedia/mmthread.h>
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
#include <multimedia/mm_amhelper.h>
#endif

#include <multimedia/mm_debug.h>

MM_LOG_DEFINE_MODULE_NAME("PCMPTEST");
using namespace YUNOS_MM;

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
static MMAMHelper * s_g_amHelper = NULL;
#endif

class TestListener;

PCMPlayer * s_g_player = NULL;
TestListener * listener = NULL;
static bool play_complete = false;
static const char *input_url = NULL;
static snd_format_t format;
static uint32_t sampleRate;
static int channelCount;

class TestListener : public PCMPlayer::Listener {
public:
    TestListener(){}
    ~TestListener(){}

    virtual void onMessage(int msg, int param1, int param2, const MMParam *obj)
    {
        MMLOGD("msg: %d, param1: %d, param2: %d, obj: %p\n", msg, param1, param2, obj);
        switch ( msg ) {
            case PLAY_COMPLETE:
                play_complete = true;
                break;
            case MSG_ERROR:
                MMLOGE("err\n");
                exit(-2);
                break;
            default:
                MMASSERT(0);
                break;
        }
    }

};

class PcmPlayerTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};


TEST_F(PcmPlayerTest, pcmpTest) {
    mm_status_t ret = MM_ERROR_SUCCESS;

    MMLOGI("Welcome to PCMPlayer!\n");
    s_g_player = PCMPlayer::create();
    ASSERT_TRUE(s_g_player != NULL);

    MMLOGV("newing listener\n");
    listener = new TestListener();
    if ( !listener ) {
        MMLOGI("Failed to create listener\n");
        PCMPlayer::destroy(s_g_player);
    }
    ASSERT_TRUE(listener != NULL);
    s_g_player->setListener(listener);

    MMLOGI("start play\n");
    ret = s_g_player->play(input_url, format, sampleRate, channelCount
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
            , s_g_amHelper->getConnectionId()
#endif
        );
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);

    while(!play_complete){
        usleep(1000000);
    }

    s_g_player->stop();

    if (listener) {
        free(listener);
        listener = NULL;
    }
    if (s_g_player) {
        PCMPlayer::destroy(s_g_player);
        s_g_player = NULL;
    }
    MMLOGI("play done\n");
}


int main(int argc, char* const argv[]) {
    int ret ;
    if (argc < 5) {
        printf("usage: uri format sampleRate channelcount\n");
        return -1;
    }

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

    input_url = argv[1];
    int i;
    sscanf(argv[2], "%d", &i);
    format = (snd_format_t)i;
    sscanf(argv[3], "%u", &sampleRate);
    sscanf(argv[4], "%d", &channelCount);
    printf("input_url = %s\n",input_url);
    MMLOGI("hello pcmplayer\n");
    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
    } catch (...) {
        printf("InitGoogleTest failed!");
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
        s_g_amHelper->disconnect();
#endif
        return -1;
    }
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    s_g_amHelper->disconnect();
#endif
    return ret;
}

