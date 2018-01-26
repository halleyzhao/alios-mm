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
#include <multimedia/mediametaretriever.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include "multimedia/mm_debug.h"

static const int TEST_TIMES = 10;

namespace YUNOS_MM {

static const char * TEST_FILES[] = {
    "/usr/bin/ut/res/audio/dts.mp4",
    "/usr/bin/ut/res/video/error.mp4",
    "/usr/bin/ut/res/video/test.mp4",
    "/usr/bin/ut/res/video/test.webm",
    "/usr/bin/ut/res/audio/ad.mp3",
    "/usr/bin/ut/res/audio/error.mp3",
    "/usr/bin/ut/res/audio/sp.mp3",
    "/usr/bin/ut/res/audio/sp2.mp3",
    "/usr/bin/ut/res/audio/tick.mp3",
    "/usr/bin/ut/res/audio/notexistsmedia",
    "/data/mp3.mp3",
    "/data/mp4.mp4",
};

static const int TEST_FILES_COUNT = sizeof(TEST_FILES) / sizeof(char*);


class MetaRetrieverTest {
public:
    MetaRetrieverTest()
    {
    }
    ~MetaRetrieverTest(){}

public:
    void test();

private:
    mm_status_t testFile(MediaMetaRetriever * r , const char * file);

private:
    MM_DISALLOW_COPY(MetaRetrieverTest)
    DECLARE_LOGTAG()
};

DEFINE_LOGTAG(MetaRetrieverTest)

void MetaRetrieverTest::test()
{
    MMLOGI("start test\n");
    MediaMetaRetriever * r = MediaMetaRetriever::create();
    if ( !r ) {
        MMLOGE("failed to create r\n");
        return;
    }

    int failcount = 0;
    int successcount = 0;
    for ( int i = 0; i < TEST_FILES_COUNT; ++i ) {
        printf("testing file: %s ( %d / %d )\n", TEST_FILES[i], i, TEST_FILES_COUNT);
        MMLOGI("testing file: %s ( %d / %d )\n", TEST_FILES[i], i, TEST_FILES_COUNT);
        mm_status_t ret = testFile(r, TEST_FILES[i]);
        if ( ret != MM_ERROR_SUCCESS ) {
            MMLOGI("test %s not extracted\n", TEST_FILES[i]);
            ++failcount;
            continue;
        }

        ++successcount;
        MMLOGI("test %s extracted\n", TEST_FILES[i]);
    }

    MMLOGI("test over, total: %d, extracted: %d, not extracted: %d\n", successcount + failcount, successcount, failcount);
    MediaMetaRetriever::destroy(r);
}

mm_status_t MetaRetrieverTest::testFile(MediaMetaRetriever * r , const char * file)
{
    mm_status_t ret = r->setDataSource(file);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to set datasource\n");
        return ret;
    }

    MediaMetaSP fileMeta;
    MediaMetaSP audioMeta;
    MediaMetaSP videoMeta;
    ret = r->extractMetadata(fileMeta, audioMeta, videoMeta);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("extract failed\n");
        return ret;
    }

    if ( fileMeta ) {
        fileMeta->dump();
    }
    if ( audioMeta ) {
        audioMeta->dump();
    }
    if ( videoMeta ) {
        videoMeta->dump();
    }

    CoverSP cover = r->extractCover();
    if ( cover ) {
        MMLOGI("has cover, size: %zu\n", cover->mData->getActualSize());
    } else {
        MMLOGI("no cover\n");
    }

    VideoFrameSP frame;
    frame = r->extractVideoFrame(-1);
    if ( frame ) {
        MMLOGI("has video frame, size: %zu, width: %d, height: %d, colorformat: %d, RotationAngle: %d\n",
            frame->mData->getActualSize(),
            frame->mWidth,
            frame->mHeight,
            frame->mColorFormat,
            frame->mRotationAngle);
    } else {
        MMLOGI("no video frame\n");
    }

    frame = r->extractVideoFrame(5);
    if ( frame ) {
        MMLOGI("has video frame, size: %zu, width: %d, height: %d, colorformat: %d, RotationAngle: %d\n",
            frame->mData->getActualSize(),
            frame->mWidth,
            frame->mHeight,
            frame->mColorFormat,
            frame->mRotationAngle);
    } else {
        MMLOGI("no video frame\n");
    }

    frame = r->extractVideoFrame(5, RGB::kFormatRGB24, 2, 3);
    if ( frame ) {
        MMLOGI("has video frame, rgb: size: %zu, width: %d, height: %d, colorformat: %d, RotationAngle: %d\n",
            frame->mData->getActualSize(),
            frame->mWidth,
            frame->mHeight,
            frame->mColorFormat,
            frame->mRotationAngle);

        const char * p = file + strlen(file) - 1;
        while (*p != '/' && p != file) {
            --p;
        }

        char * fname = new char[strlen(p) + 16];
        sprintf(fname, "/data%s_rgb.jpg", p);
        uint32_t jpegW, jpegH;
        RGB::saveAsJpeg(frame->mData, RGB::kFormatRGB24, frame->mWidth, frame->mHeight, 1, 1, false, 0, fname, jpegW, jpegH);
        delete [] fname;
        fname = NULL;
    } else {
        MMLOGI("no video frame\n");
    }

    frame = r->extractVideoFrame(0xffffffff);
    if ( frame ) {
        MMLOGI("has video frame, size: %zu, width: %d, height: %d, colorformat: %d, RotationAngle: %d\n",
            frame->mData->getActualSize(),
            frame->mWidth,
            frame->mHeight,
            frame->mColorFormat,
            frame->mRotationAngle);
    } else {
        MMLOGI("no video frame\n");
    }

    frame = r->extractVideoFrame(10);
    if ( frame ) {
        MMLOGI("has video frame, size: %zu, width: %d, height: %d, colorformat: %d, RotationAngle: %d\n",
            frame->mData->getActualSize(),
            frame->mWidth,
            frame->mHeight,
            frame->mColorFormat,
            frame->mRotationAngle);
    } else {
        MMLOGI("no video frame\n");
    }

    MMLOGI("success\n");
    return ret;
}

}


MM_LOG_DEFINE_MODULE_NAME("MetaRetrieverTest")

class MediametarTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(MediametarTest, mediametarTest) {
    MMLOGI("hello MetaRetriever\n");
    for ( int i = 0; i < TEST_TIMES; ++i ) {
        printf("testing times: %d / %d\n", i, TEST_TIMES);
        MMLOGI("testing times: %d / %d\n", i, TEST_TIMES);
        YUNOS_MM::MetaRetrieverTest * t = new YUNOS_MM::MetaRetrieverTest();
        t->test();
        delete t;
        printf("testing times: %d / %d over\n", i, TEST_TIMES);
        MMLOGI("testing times: %d / %d over\n", i, TEST_TIMES);
    }
    MMLOGI("bye\n");
}
