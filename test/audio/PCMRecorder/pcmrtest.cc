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

#include <multimedia/PCMRecorder.h>

#include <multimedia/mmthread.h>

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
#include <multimedia/mm_amhelper.h>
#endif

#include <multimedia/mm_debug.h>
MM_LOG_DEFINE_MODULE_NAME("PCMRTEST");
using namespace YUNOS_MM;

static const snd_format_t SAMPLE_FORMAT = SND_FORMAT_PCM_16_BIT;
static const uint32_t SAMPLE_RATE = 44100;
static const uint8_t CHANNEL_COUNT = 2;
static const size_t BUF_SIZE = 10240;
static const int RECORD_TIME = 10 * 1000000; // usec
static const int PAUSE_TIME = 2 * 1000000; // usec
static const int PAUSE_TIMES = 1;

static const char * RECORD_FILE = "/data/pcmrt.pcm";

static sem_t s_g_reader_sem;
PCMRecorder * s_g_recorder = NULL;
size_t mReadSize;
static const char * MMTHREAD_NAME = "pcmrtest::Reader";
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
static MMAMHelper * s_g_amHelper = NULL;
#endif

class TestListener : public PCMRecorder::Listener {
public:
    TestListener(){}
    ~TestListener(){}

    virtual void onMessage(int msg, int param1, int param2, const MMParam *obj)
    {
        MMLOGD("msg: %d, param1: %d, param2: %d, obj: %p\n", msg, param1, param2, obj);
        switch ( msg ) {
            case EVENT_MORE_DATA:
                mReadSize = (size_t)param1;
                sem_post(&s_g_reader_sem);
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

class Reader : public MMThread {
public:
    Reader() : MMThread(MMTHREAD_NAME), mContinue(true), mFile(NULL), mBuf(NULL)
    {
        mContinue = true;
        create();
    }

    ~Reader()
    {
        mContinue = false;
        sem_post(&s_g_reader_sem);
        destroy();
    }

protected:
    virtual void main()
    {
        MMLOGV("read thread started\n");
        mFile = fopen(RECORD_FILE, "w");
        ASSERT_NE(mFile, NULL);

        while ( mContinue ) {
            MMLOGV("waitting sem\n");
            sem_wait(&s_g_reader_sem);
            if ( mContinue )
                read();
        }

        fclose(mFile);
        mFile = NULL;
        MMLOGD("read thread exit\n");
    }

    void read()
    {
        mBuf = (uint8_t *)malloc(mReadSize);
        while ( 1 ) {
            size_t readed = s_g_recorder->read(mBuf, mReadSize);
            if ( readed == 0 ) {
                MMLOGV("no more\n");
                break;
            }
            MMLOGV("readed: %d\n", readed);
            size_t written = fwrite(mBuf, 1, readed, mFile);
            if ( written == 0 ) {
                MMLOGE("failed to write\n");
                break;
            }
            MMLOGV("written: %u\n", written);
        }
        if (mBuf) {
            free(mBuf);
            mBuf = NULL;
        }
    }

private:
    bool mContinue;
    FILE * mFile;
    uint8_t * mBuf;
};


static mm_status_t start_and_pause()
{
    mm_status_t ret = s_g_recorder->start();
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    MMLOGI("start success\n");

    MMLOGI("wait for record %d seconds\n", RECORD_TIME / 1000000 / 2);
    usleep(RECORD_TIME / 2);

    MMLOGI("pausing\n");
    ret = s_g_recorder->pause();
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    MMLOGI("pause success\n");

    usleep(PAUSE_TIME / 2);

    return MM_ERROR_SUCCESS;
}

class PcmRecorderTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};


TEST_F(PcmRecorderTest, pcmrTest) {
    MMLOGI("Welcome to PCMRecorder!\n");
    int ret;
    s_g_recorder = PCMRecorder::create();
    ASSERT_NE(s_g_recorder, NULL);

    MMLOGV("newing listener\n");
    TestListener * listener = new TestListener();
    if ( !listener ) {
        MMLOGI("Failed to create listener\n");
        PCMRecorder::destroy(s_g_recorder);
    }

    ASSERT_NE(listener, NULL);

    s_g_recorder->setListener(listener);

    MMLOGV("initing sems\n");
    sem_init(&s_g_reader_sem, 0, 0);

    MMLOGV("newing reader\n");
    Reader * reader = new Reader();
    EXPECT_NE(reader, NULL);

    MMLOGI("preparing\n");
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    ret = s_g_recorder->prepare(ADEV_SOURCE_DEFAULT,
                SAMPLE_FORMAT,
                SAMPLE_RATE,
                CHANNEL_COUNT,
                NULL,
                s_g_amHelper->getConnectionId());
#else
    ret = s_g_recorder->prepare(ADEV_SOURCE_DEFAULT,
                SAMPLE_FORMAT,
                SAMPLE_RATE,
                CHANNEL_COUNT,
                NULL);
#endif
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    MMLOGI("prepare success\n");

    for ( int i = 0; i < PAUSE_TIMES; ++i ) {
        MMLOGI("start and pause\n");
        EXPECT_EQ(start_and_pause(),  MM_ERROR_SUCCESS);
    }

    MMLOGI("reseting\n");
    MM_RELEASE(reader);
    s_g_recorder->reset();
    MMLOGI("reset over\n");

    MMLOGI("releasing\n");
    MM_RELEASE(reader);
    MM_RELEASE(listener);
    sem_destroy(&s_g_reader_sem);
    PCMRecorder::destroy(s_g_recorder);
    MMLOGI("release over\n");
}

int main(int argc, char* const argv[]) {
    int ret ;
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    try {
        s_g_amHelper = new MMAMHelper();
        if (s_g_amHelper->connect(MMAMHelper::recordChnnelMic()) != MM_ERROR_SUCCESS) {
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


