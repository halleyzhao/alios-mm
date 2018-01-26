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
#include <multimedia/wbmp.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include "multimedia/mm_debug.h"

static const char * TEST_FILES[] = {
    "/data/wbmp.wbmp"
};

static const int TEST_FILES_COUNT = sizeof(TEST_FILES) / sizeof(char*);


MM_LOG_DEFINE_MODULE_NAME("WBMPTest")

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
static int doTest(const char * fname, RGB::Format outFormat)
{
    uint32_t w,h;
    MMBufferSP data = WBMP::decode(fname,
                                        outFormat,
                                        w,
                                        h);
    if (!data) {
        MMLOGE("failed to decode\n");
        return 0;
    }

    MMLOGI("width: %u, height: %u\n", w, h);
    //dumpData(data->getBuffer(), data->getActualSize(), 16);

    char name[1024];
#define DUMB_RGB
#ifdef DUMB_RGB
    sprintf(name, "%s.%d_%ux%u.rgb", fname, outFormat, w, h);
    DataDump dumper(name);
    dumper.dump(data->getBuffer(), data->getActualSize());
#endif

    sprintf(name, "%s.save_d_%d.jpg", fname, outFormat);
    uint32_t jpegW, jpegH;
    mm_status_t ret = RGB::saveAsJpeg(data,
                    outFormat,
                    w,
                    h,
                    1,
                    1,
                    true,
                    name,
                    jpegW,
                    jpegH);
    if (ret != MM_ERROR_SUCCESS) {
        MMLOGE("failed to sav as jpeg\n");
        return 0;
    }

    sprintf(name, "%s.save_d_%d.jpg", fname, outFormat);
    ret = RGB::saveAsJpeg(data,
                    outFormat,
                    w,
                    h,
                    1,
                    1,
                    true,
                    name,
                    jpegW,
                    jpegH);
    if (ret != MM_ERROR_SUCCESS) {
        MMLOGE("failed to sav as jpeg\n");
        return 0;
    }
    MMLOGI("jpegw: %u, jpegh: %u\n", jpegW, jpegH);

    return 0;
}


int main(int argc, char * argv[])
{
    MMLOGI("hello wbmp\n");
    for ( int i = 0; i < TEST_FILES_COUNT; ++i ) {
        MMLOGI("testing file: %s\n", TEST_FILES[i]);
        MMLOGI("testing decode\n");
        doTest(TEST_FILES[i], RGB::kFormatRGB32);
        doTest(TEST_FILES[i], RGB::kFormatRGB24);
        doTest(TEST_FILES[i], RGB::kFormatRGB565);

        char name[1024];
        sprintf(name, "%s.save_asjpg.jpg", TEST_FILES[i]);
        uint32_t jpegW, jpegH;
        mm_status_t ret = WBMP::saveAsJpeg(TEST_FILES[i],
                        name,
                        jpegW,
                        jpegH);
        if (ret != MM_ERROR_SUCCESS) {
            MMLOGE("failed to sav as jpeg\n");
            return 0;
        }
        MMLOGI("jpegw: %u, h: %u\n", jpegW, jpegH);
    }
    MMLOGI("bye\n");
    return 0;
}

