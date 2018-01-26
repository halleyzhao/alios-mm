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
#include <multimedia/exif.h>
#include <gtest/gtest.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include "multimedia/mm_debug.h"

static const char * TEST_FILES[] = {
//    "/usr/bin/ut/res/img/jpg.jpg",
    "/usr/bin/ut/res/img/jpg.jpg",
};

static const char * TEST_SAVEAS_FILES[] = {
//    "/usr/bin/ut/res/img/jpg.jpg",
    "/usr/bin/ut/res/img/jpg1.jpg",
};

static const int TEST_FILES_COUNT = sizeof(TEST_FILES) / sizeof(char *);

static const char * TMP_FILE_PATH = "/data";

MM_LOG_DEFINE_MODULE_NAME("ExifTest")

using namespace YUNOS_MM;

#define GET_ITEM(_tag) do {\
    std::string str;\
    mm_status_t ret = exif->getTag(_tag, str);\
    EXPECT_NE(ret, MM_ERROR_UNSUPPORTED);\
    if (ret == MM_ERROR_SUCCESS)\
        MMLOGI("value check: %s, %s\n", #_tag, str.c_str());\
    else\
        MMLOGI("value check: %s, not got\n", #_tag);\
}while(0)

#define GET_INT_ITEM(_tag) do {\
    int i;\
    mm_status_t ret = exif->getIntTag(_tag, i);\
    EXPECT_NE(ret, MM_ERROR_UNSUPPORTED);\
    if (ret == MM_ERROR_SUCCESS)\
        MMLOGI("value check: %s, %d\n", #_tag, i);\
    else\
        MMLOGI("value check: %s, not got\n", #_tag);\
}while(0)

#define GET_RATIONAL_ITEM(_tag) do {\
    int i, j;\
    mm_status_t ret = exif->getRationalTag(_tag, i, j);\
    EXPECT_NE(ret, MM_ERROR_UNSUPPORTED);\
    if (ret == MM_ERROR_SUCCESS)\
        MMLOGI("value check: %s, %d/%d\n", #_tag, i, j);\
    else\
        MMLOGI("value check: %s, not got\n", #_tag);\
}while(0)

static int doTest(const char * fname, const char * saveAs)
{
    ExifSP exif = Exif::create();
    if (!exif) {
        MMLOGE("failed to create exif\n");
        EXPECT_EQ(1,0);
        return -1;
    }

    if (exif->open(fname) != MM_ERROR_SUCCESS) {
        MMLOGE("failed to open %s\n", fname);
        EXPECT_EQ(1,0);
        return -1;
    }

    GET_ITEM(Exif::kTagType_GPS_LATITUDE_REF);
    GET_ITEM(Exif::kTagType_GPS_LATITUDE);
    GET_ITEM(Exif::kTagType_GPS_LONGITUDE_REF);
    GET_ITEM(Exif::kTagType_GPS_LONGITUDE);
    GET_INT_ITEM(Exif::kTagType_GPS_ALTITUDE_REF);
    GET_RATIONAL_ITEM(Exif::kTagType_GPS_ALTITUDE);
    GET_RATIONAL_ITEM(Exif::kTagType_GPS_TIMESTAMP);
    GET_ITEM(Exif::kTagType_GPS_PROCESSING_METHOD);
    GET_ITEM(Exif::kTagType_GPS_DATESTAMP);
    GET_ITEM(Exif::kTagType_MODEL);
    GET_INT_ITEM(Exif::kTagType_ORIENTATION);
    GET_ITEM(Exif::kTagType_DATETIME);
    GET_ITEM(Exif::kTagType_MAKE);
    GET_INT_ITEM(Exif::kTagType_IMAGE_WIDTH);
    GET_INT_ITEM(Exif::kTagType_IMAGE_LENGTH);
    GET_INT_ITEM(Exif::kTagType_ISO);
    GET_RATIONAL_ITEM(Exif::kTagType_EXPOSURE_TIME);
    GET_RATIONAL_ITEM(Exif::kTagType_APERTURE);
    GET_INT_ITEM(Exif::kTagType_FLASH);
    GET_RATIONAL_ITEM(Exif::kTagType_FOCAL_LENGTH);
    GET_INT_ITEM(Exif::kTagType_WHITE_BALANCE);

    
/*
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
*/
    EXPECT_EQ(1,1);
    return 0;
}


class ExifTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(ExifTest, exifTest) {
    MMLOGI("hello exif\n");

    for ( int i = 0; i < TEST_FILES_COUNT; ++i ) {
        MMLOGI("testing file: %s\n", TEST_FILES[i]);
        doTest(TEST_FILES[i], TEST_SAVEAS_FILES[i]);
    }

    MMLOGI("bye\n");
}

