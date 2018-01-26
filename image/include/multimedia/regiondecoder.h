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

#ifndef __regiondecoder_H
#define __regiondecoder_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_buffer.h>
#include <multimedia/rgb.h>
#include <multimedia/media_attr_str.h>

namespace YUNOS_MM {

class RegionDecoder {
public:
    static RegionDecoder * create(const char * mime);
    static void destroy(RegionDecoder * regionDecoder);

public:
    virtual bool prepare(const char * path, uint32_t & width, uint32_t & height) = 0;
    virtual MMBufferSP decode(RGB::Format outFormat,
                            uint32_t startX,
                            uint32_t startY,
                            uint32_t width,
                            uint32_t height) = 0;
    virtual void unprepare() = 0;

public:
    virtual ~RegionDecoder(){}

protected:
    RegionDecoder(){}

    MM_DISALLOW_COPY(RegionDecoder)
    DECLARE_LOGTAG()
};


}

#endif

