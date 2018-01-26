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

#include <multimedia/regiondecoder.h>
#include "regiondecoder_jpg.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(RegionDecoder)

#define CREATEDECODER(_t) do {\
    MMLOGI("mime: %s, %s\n", mime, #_t);\
    try {\
        JPEGRegionDecoder * d = new _t();\
        return d;\
    } catch (...) {\
        MMLOGE("no mem\n");\
        return NULL;\
    }\
}while(0)

/*static */RegionDecoder * RegionDecoder::create(const char * mime)
{
/*
    const char * p = path;
    if (!p || *p == '\0') {
        MMLOGE("invalid path\n");
        return NULL;
    }
    while (*p != '\0') ++p;
    while (p != path && *p != '.') --p;
    if (p == path || *(++p) == '\0') {
        MMLOGE("invalid path: %s\n", path);
        return NULL;
    }
*/
    if (!strcasecmp(mime, MEDIA_MIMETYPE_IMAGE_JPEG)) {
        CREATEDECODER(JPEGRegionDecoder);
    }

    MMLOGE("unsupported mime: %s\n", mime);
    return NULL;
}

/*static */void RegionDecoder::destroy(RegionDecoder * regionDecoder)
{
    MMLOGV("+\n");
    if (!regionDecoder) return;

    delete regionDecoder;
}

}
