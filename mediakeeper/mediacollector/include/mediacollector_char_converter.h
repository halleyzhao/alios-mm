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

#ifndef __mediacollector_char_converter_H
#define __mediacollector_char_converter_H

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>

#include <multimedia/mediacollector.h>
#include <multimedia/mediametaretriever.h>

namespace YUNOS_MM {

class CharConverter {
public:
    enum {
        kCharSetNone               = 0,
        kCharSetShiftJIS           = (1 << 0),
        kCharSetGBK                = (1 << 1),
        kCharSetBig5               = (1 << 2),
        kCharSetEUCKR              = (1 << 3),
        kCharSetISO8859			= (1 << 4),
        kCharSetCP1251              = (1 << 5),
        kCharSetAcii = (1<<6),
        kCharSetUTF8 = (1<<7),
        kCharSetAll                = (kCharSetShiftJIS | kCharSetGBK | kCharSetBig5 | kCharSetEUCKR)
    };

public:
    static mm_status_t convert(uint32_t localeEncoding, const char * src, char *& dst);
    static uint32_t probEncoding(const char * s);
    static mm_status_t convert(uint32_t localeEncoding, uint32_t srcEncoding, const char * src, char *& dst);
    static mm_status_t convert(const char * srcenc, const char * dstenc, const char * src, char *& dst);
    static mm_status_t tryConvertUtf8ToLocal(uint32_t localeEncoding, const char * str);

    MM_DISALLOW_COPY(CharConverter)
    DECLARE_LOGTAG()
};


}
#endif
