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
#include <multimedia/image.h>
#include <gtest/gtest.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include "multimedia/mm_debug.h"

struct TestSource {
    const char * path;
    bool bottomUp;
};

static TestSource TEST_FILES[] = {
    {"/usr/bin/ut/res/img/jpg.jpg", false},
    {"/usr/bin/ut/res/img/png.png",false},
    {"/usr/bin/ut/res/img/bmp.bmp",true},
    {"/usr/bin/ut/res/img/wbmp.wbmp",true},
    {"/usr/bin/ut/res/img/gif.gif",false},
    {"/usr/bin/ut/res/img/webp.webp",false}
};

static const int TEST_FILES_COUNT = sizeof(TEST_FILES) / sizeof(TestSource);

static const char * TMP_FILE_PATH = "/data";

MM_LOG_DEFINE_MODULE_NAME("ImageTest")

using namespace YUNOS_MM;

/*
static void dumpData(const void* ptr, uint32_t size, uint32_t bytesPerLine)
{
    const uint8_t *data = (uint8_t*)ptr;
    mm_log(MM_LOG_DEBUG, "hexDump", " data=%p, size=%zu, bytesPerLine=%d\n", data, size, bytesPerLine);

    if (!data)
        return;

    char oneLineData[bytesPerLine*4+1];
    uint32_t offset = 0, lineStart = 0, i= 0;
    while (offset < size) {
        sprintf(&oneLineData[i*4], "%02x, ", *(data+offset));
        offset++;
        i++;
        if (offset == size || (i % bytesPerLine == 0)) {
            oneLineData[4*i-1] = '\0';
            mm_log(MM_LOG_DEBUG, "hexDump", "%04x: %s", lineStart, oneLineData);
            lineStart += bytesPerLine;
            i = 0;
        }
    }
}
*/

#define DUMP_RGB

static int doTest(const char * fname, RGB::Format outFormat, int num, int den, bool bottomUp)
{
    uint32_t w,h;
    MMBufferSP data = ImageDecoder::decode(fname,
                                        num,
                                        den,
                                        outFormat,
                                        w,
                                        h);
    if (!data) {
        MMLOGE("failed to decode\n");
        EXPECT_EQ(1, 0);
        return 0;
    }

    MMLOGI("width: %u, height: %u\n", w, h);
    //dumpData(data->getBuffer(), data->getActualSize(), 16);

    char name[1024];

#ifdef DUMB_RGB
    sprintf(name, "%s.%d_%ux%u.rgb", fname, outFormat, w, h);
    DataDump dumper(name);
    dumper.dump(data->getBuffer(), data->getActualSize());
#endif

    const char * p = fname + strlen(fname) - 1;
    while (p != fname && *p != '/')
        --p;
    
    sprintf(name, "%s/%s.save_%d_%d_%d.jpg",
        TMP_FILE_PATH,
        p,
        outFormat,
        num,
        den);
    uint32_t jpegW, jpegH;
    mm_status_t ret = RGB::saveAsJpeg(data,
                    outFormat,
                    w,
                    h,
                    1,
                    1,
                    bottomUp,
                    name,
                    jpegW,
                    jpegH);
    EXPECT_EQ(ret, MM_ERROR_SUCCESS);
    if (ret != MM_ERROR_SUCCESS) {
        MMLOGE("failed to sav as jpeg\n");
        return 0;
    }
    MMLOGI("jpegw: %u, jpegh: %u\n", jpegW, jpegH);

    EXPECT_EQ(1,1);
    return 0;
}


class ImageTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(ImageTest, imageTest) {
    MMLOGI("hello Image\n");

    for ( int i = 0; i < TEST_FILES_COUNT; ++i ) {
        MMLOGI("testing file: %s\n", TEST_FILES[i].path);
        MMLOGI("testing decode\n");
        doTest(TEST_FILES[i].path, RGB::kFormatRGB32, 1, 1, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB24, 1, 1, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB565, 1, 1, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB32, 1, 3, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB24, 1, 3, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB565, 1, 3, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB32, 2, 1, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB24, 2, 1, TEST_FILES[i].bottomUp);
        doTest(TEST_FILES[i].path, RGB::kFormatRGB565, 2, 1, TEST_FILES[i].bottomUp);
    }

    MMLOGI("bye\n");
}

