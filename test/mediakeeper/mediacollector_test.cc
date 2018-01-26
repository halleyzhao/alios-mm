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
#include <stdio.h>
#include <stdlib.h>
#include <gtest/gtest.h>
#include <multimedia/mediacollector.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include "multimedia/mm_debug.h"

static const int TEST_TIMES = 1;

namespace YUNOS_MM {

static const char * TEST_FILES[] = {
/*    "/usr/bin/ut/res/audio/dts.mp4",
    "/usr/bin/ut/res",
    "/data",
    "notexistsfile",*/
    //"/data/jpg.jpg"
    //"/data/he.bmp"
    //"/data/y.mp3"
    //"/data/yjz.mp4"
    //"/data/yp.png"
    //"/data/yjz.gif"
    //"/data/collect_test"
    "/data/media/mu"
    //"/mnt/data/yunos/opt/app/hellolocale.yunos.com/res/en-US_1080p/images/background.png"
    //"/data/png.png"
};

static const int TEST_FILES_COUNT = sizeof(TEST_FILES) / sizeof(char*);

#define FUNC_ENTER() MMLOGI("+\n")
#define FUNC_LEAVE() MMLOGI("-\n")

class MyWatcher : public MediaCollectorWatcher {
public:
    MyWatcher(const char * local) : MediaCollectorWatcher(local)
    {
        FUNC_ENTER();
        FUNC_LEAVE();
    }

    ~MyWatcher()
    {
        FUNC_ENTER();
        FUNC_LEAVE();
    }

public:
    // MM_ERROR_SUCCESS
    // MM_ERROR_SKIPPED
    virtual mm_status_t fileFound(const char * path,
                                                            int64_t lastModified,
                                                            int64_t fileSize,
                                                            bool isDirectory)
    {
        FUNC_ENTER();
        FUNC_LEAVE();
        return MM_ERROR_SUCCESS;
    }

    virtual mm_status_t addAttrib(const char * name, const char * tag)
    {
        FUNC_ENTER();
        MMLOGV("(%s, %s)\n", name, tag);
        FUNC_LEAVE();
        return MM_ERROR_SUCCESS;
    }

    virtual mm_status_t setMimeType(const char* mimeType)
    {
        FUNC_ENTER();
        FUNC_LEAVE();
        return MM_ERROR_SUCCESS;
    }

    virtual mm_status_t fileDone(mm_status_t status)
    {
        FUNC_ENTER();
        return MM_ERROR_SUCCESS;
        FUNC_LEAVE();
    }

    MM_DISALLOW_COPY(MyWatcher)
    DECLARE_LOGTAG()
};


class MediaCollectorTest {
public:
    MediaCollectorTest()
    {
        FUNC_ENTER();
        FUNC_LEAVE();
    }

    ~MediaCollectorTest()
    {
    }

public:
    void test();

private:
    mm_status_t testItem(MediaCollector * s , const char * file);

private:
    MM_DISALLOW_COPY(MediaCollectorTest)
    DECLARE_LOGTAG()
};

DEFINE_LOGTAG(MediaCollectorTest)
DEFINE_LOGTAG(MyWatcher)

void MediaCollectorTest::test()
{
    MMLOGI("start test\n");
    MyWatcher * w = new MyWatcher("zh_CN");
    if ( !w ) {
        MMLOGE("failed to create MyWatcher\n");
        return;
    }
    MediaCollector * s = MediaCollector::create(w, 320, 96, "/data");
    if ( !s ) {
        MMLOGE("failed to create MediaCollector\n");
        delete w;
        return;
    }

    int failcount = 0;
    int successcount = 0;
    for ( int i = 0; i < TEST_FILES_COUNT; ++i ) {
        printf("testing: %s ( %d / %d )\n", TEST_FILES[i], i, TEST_FILES_COUNT);
        MMLOGI("testing: %s ( %d / %d )\n", TEST_FILES[i], i, TEST_FILES_COUNT);
        mm_status_t ret = testItem(s, TEST_FILES[i]);
        if ( ret != MM_ERROR_SUCCESS ) {
            MMLOGI("test %s not collected\n", TEST_FILES[i]);
            ++failcount;
            continue;
        }

        ++successcount;
        MMLOGI("test %s collected\n", TEST_FILES[i]);
    }

    MMLOGI("test over, total: %d, collected: %d, not collected: %d\n", successcount + failcount, successcount, failcount);
    MediaCollector::destroy(s);
    delete w;
}

mm_status_t MediaCollectorTest::testItem(MediaCollector * s , const char * file)
{
    mm_status_t ret = s->collect(file);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to collect\n");
        return ret;
    }

    MMLOGI("success\n");
    return ret;
}

}


MM_LOG_DEFINE_MODULE_NAME("MediaCollectorTest")

class MediaCollectorTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(MediaCollectorTest, MediaCollectorTest) {
    MMLOGI("hello MediaCollector\n");
    for ( int i = 0; i < TEST_TIMES; ++i ) {
        printf("testing times: %d / %d\n", i, TEST_TIMES);
        MMLOGI("testing times: %d / %d\n", i, TEST_TIMES);
        YUNOS_MM::MediaCollectorTest * t = new YUNOS_MM::MediaCollectorTest();
        t->test();
        delete t;
        printf("testing times: %d / %d over\n", i, TEST_TIMES);
        MMLOGI("testing times: %d / %d over\n", i, TEST_TIMES);
    }
    MMLOGI("bye\n");
}
