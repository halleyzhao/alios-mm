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

#ifndef __bmp_H
#define __bmp_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_buffer.h>
#include <multimedia/rgb.h>

namespace YUNOS_MM {

class BMP {
public:
    static MMBufferSP decode(const char * path,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        RGB::Format outFormat,
                        uint32_t & width,
                        uint32_t & height);

    static MMBufferSP decode(const MMBufferSP & srcBuf,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        RGB::Format outFormat,
                        uint32_t & width,
                        uint32_t & height);

    static MMBufferSP decode(const uint8_t * srcBuf,
                        size_t srcBufSize,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        RGB::Format outFormat,
                        uint32_t & width,
                        uint32_t & height);

    static mm_status_t saveAsJpeg(const char * path,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        const char * jpegPath,
                        uint32_t & jpegWidth,
                        uint32_t & jpegHeight);

    static mm_status_t parseDimension(const uint8_t * srcBuf,
                        size_t srcBufSize,
                        uint32_t & width,
                        uint32_t & height);
    static mm_status_t parseDimension(const MMBufferSP & srcBuf,
                        uint32_t & width,
                        uint32_t & height);
    static mm_status_t parseDimension(const char * path,
                        uint32_t & width,
                        uint32_t & height);

    MM_DISALLOW_COPY(BMP)
    DECLARE_LOGTAG()
};


}

#endif
