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
#include <multimedia/regiondecoder.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include "multimedia/mm_debug.h"

static const char * TEST_FILES[] = {
    "/usr/bin/ut/res/jpg.jpg"
};

static const int TEST_FILES_COUNT = sizeof(TEST_FILES) / sizeof(char*);


MM_LOG_DEFINE_MODULE_NAME("REGIONDECTest")

using namespace YUNOS_MM;

#define DUMP_RGB

static int doTest(const char * fname, RGB::Format outFormat)
{
    MMLOGI("creating\n");
    RegionDecoder * d = RegionDecoder::create(MEDIA_MIMETYPE_IMAGE_JPEG);
    if (!d) {
        MMLOGE("failed to create decoder\n");
        return -1;
    }

    uint32_t srcW, srcH;
    MMLOGI("preparing\n");
    if (!d->prepare(fname, srcW, srcH)) {
        MMLOGE("failed to prepare\n");
        RegionDecoder::destroy(d);
        return -1;
    }
    MMLOGI("preparing over, srcW: %u, srcH: %u\n", srcW, srcH);

    MMLOGI("decoding\n");
    uint32_t startX = 1000;
    uint32_t startY = 1000;
    uint32_t w = 1000;
    uint32_t h = 1000;
    MMBufferSP data = d->decode(outFormat,
                                        startX,
                                        startY,
                                        w,
                                        h);
    if (!data) {
        MMLOGE("failed to decode\n");
        RegionDecoder::destroy(d);
        return -1;
    }

    const char * p = fname;
    if (!p || *p == '\0') {
        MMLOGE("invalid path\n");
        return -1;
    }
    while (*p != '\0') ++p;
    while (p != fname && *p != '/') --p;
    if (p == fname || *(++p) == '\0') {
        MMLOGE("invalid path: %s\n", fname);
        return -1;
    }

    char name[1024];

#ifdef DUMP_RGB
    sprintf(name, "/data/%s.rd.%d_%u.%u_%ux%u.rgb", p, outFormat, startX, startY, w, h);
    DataDump dumper(name);
    dumper.dump(data->getBuffer(), data->getActualSize());
#endif

    sprintf(name, "/data/%s.rd.%d_%u.%u_%ux%u.jpg", p, outFormat, startX, startY, w, h);
    uint32_t jpegW, jpegH;
    mm_status_t ret = RGB::saveAsJpeg(data,
                    outFormat,
                    w,
                    h,
                    1,
                    1,
                    false,
                    name,
                    jpegW,
                    jpegH);
    if (ret != MM_ERROR_SUCCESS) {
        MMLOGE("failed to sav as jpeg\n");
        RegionDecoder::destroy(d);
        return 0;
    }

    MMLOGI("success\n");
    d->unprepare();
    RegionDecoder::destroy(d);
    return 0;
}

int main(int argc, char * argv[])
{
    MMLOGI("hello region decoder\n");
    for ( int i = 0; i < TEST_FILES_COUNT; ++i ) {
        MMLOGI("testing file: %s\n", TEST_FILES[i]);
        doTest(TEST_FILES[i], RGB::kFormatRGB32);
        doTest(TEST_FILES[i], RGB::kFormatRGB24);
        doTest(TEST_FILES[i], RGB::kFormatRGB565);
    }
    MMLOGI("bye\n");
    return 0;
}

