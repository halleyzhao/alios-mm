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

#ifndef __yunos_mm_exif_H
#define __yunos_mm_exif_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_buffer.h>

namespace YUNOS_MM {

class Exif;
typedef MMSharedPtr<Exif> ExifSP;

class Exif {
public:
    static ExifSP create();

public:
    virtual ~Exif() {}

public:
    // tag names
    enum TagType{
        // type: string
        kTagType_GPS_LATITUDE_REF = 0x0001,
        // type: string, like 31, 12, 54.284
        kTagType_GPS_LATITUDE = 0x0002,
        // type: string, like 121, 31, 48.382
        kTagType_GPS_LONGITUDE_REF = 0x0003,
        // type: string
        kTagType_GPS_LONGITUDE = 0x0004,
        // type: int
        kTagType_GPS_ALTITUDE_REF = 0x0005,
        // type: rational
        kTagType_GPS_ALTITUDE = 0x0006,
        // type: rational
        kTagType_GPS_TIMESTAMP = 0x0007,
        // type: string
        kTagType_GPS_PROCESSING_METHOD = 0x001b,
        // type: string
        kTagType_GPS_DATESTAMP = 0x001d,

        // type: string
        kTagType_MODEL = 0x0110,
        // type: int
        kTagType_ORIENTATION = 0x0112,
        // type: string
        kTagType_DATETIME = 0x0132,
        // type: string
        kTagType_MAKE = 0x010f,
        // type: int
        kTagType_IMAGE_WIDTH = 0x1001,
        // type: int
        kTagType_IMAGE_LENGTH = 0x1002,
        // type: int
        kTagType_ISO = 0x8827,
        // type: rational
        kTagType_EXPOSURE_TIME = 0x829a,
        // type: rational
        kTagType_APERTURE = 0x9202,
        // type: int
        kTagType_FLASH = 0x9209,
        // type: rational
        kTagType_FOCAL_LENGTH = 0x920a,
        // type: int
        kTagType_WHITE_BALANCE = 0xa403,
    };

    virtual mm_status_t open(const char * uri) = 0;
    virtual mm_status_t open(int fd, int64_t offset, int64_t length) = 0;
    virtual mm_status_t save() = 0;
    virtual mm_status_t saveAs(const char * path) = 0;
    virtual mm_status_t close(bool save = false) = 0;
    virtual bool isDirty() const = 0;

    virtual mm_status_t getTag(TagType tag, std::string & value) = 0;
    virtual mm_status_t getIntTag(TagType tag, int & value) = 0;
    virtual mm_status_t getDoubleTag(TagType tag, double & value) = 0;
    virtual mm_status_t getRationalTag(TagType tag, int & numerator, int & denominator) = 0;

    virtual mm_status_t setTag(TagType tag, const char * value) = 0;
    virtual mm_status_t setIntTag(TagType tag, int value) = 0;
    virtual mm_status_t setDoubleTag(TagType tag, double value) = 0;
    virtual mm_status_t setRationalTag(TagType tag, int numerator, int denominator) = 0;

    virtual mm_status_t removeTag(TagType tag) = 0;


protected:
    Exif() {}

    MM_DISALLOW_COPY(Exif)
    DECLARE_LOGTAG()
};


}

#endif
