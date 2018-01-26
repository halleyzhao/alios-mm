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

#ifndef __regiondecoder_jpg_H
#define __regiondecoder_jpg_H

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "jpeglib.h"
#include "jerror.h"
#include "jpegint.h"
#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_buffer.h>
#include <multimedia/rgb.h>
#include <multimedia/regiondecoder.h>

namespace YUNOS_MM {

class JPEGRegionDecoder : public RegionDecoder{
public:
    JPEGRegionDecoder();
    ~JPEGRegionDecoder();

public:
    virtual bool prepare(const char * path, uint32_t & width, uint32_t & height);
    virtual MMBufferSP decode(RGB::Format outFormat,
                            uint32_t startX,
                            uint32_t startY,
                            uint32_t width,
                            uint32_t height);
    virtual void unprepare();

private:
    void clean();

    bool initInfo(const char * path);
    void destroyInfo();
    bool buildHuffman();
    void destroyHuffman();

public:
    struct SourceManager : public jpeg_source_mgr {
        SourceManager(FILE * f);
        ~SourceManager();

        enum {
            kBufferSize = 4096 
        };

        char mBuffer[kBufferSize];
        FILE * mFile;

        MM_DISALLOW_COPY(SourceManager)
        DECLARE_LOGTAG()
    };

private:
    FILE * mFile;
    jpeg_decompress_struct mCInfo;
    bool mCInfoCreated;
    huffman_index mHuffmanIndex;
    bool mHuffmanIndexCreated;
    SourceManager * mSourceManager;

    MM_DISALLOW_COPY(JPEGRegionDecoder)
    DECLARE_LOGTAG()
};


}

#endif


