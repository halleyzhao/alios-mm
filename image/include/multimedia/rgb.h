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

#ifndef __rgb_H
#define __rgb_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_buffer.h>

namespace YUNOS_MM {

class RGB {
public:
#pragma pack(push,1)
    struct RGB32 {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
        uint8_t alpha;
    };
#pragma pack(pop)

    enum Format {
        /* 4 bytes: BGRABGRA... */
        kFormatRGB32,
        /* 3 bytes: BGRBGR... */
        kFormatRGB24,
        /* 2 bytes: B(5 bits)G(6 bits)R(5 bits)B(5 bits)G(6 bits)R(5 bits)... */
        kFormatRGB565
    };

    static RGB32 getRGBValue(const uint8_t * buf,
                            Format format,
                            uint32_t width,
                            uint32_t x,
                            uint32_t y);

    static RGB32 getRGBValue(const uint8_t * addr,
                            Format format);

    static const uint8_t * getRGBAddr(const uint8_t * buf,
                            Format format,
                            uint32_t width,
                            uint32_t x,
                            uint32_t y);

    static MMBufferSP convertFormat(Format srcFormat,
                                                Format dstFormat,
                                                uint32_t width,
                                                uint32_t height,
                                                const MMBufferSP & srcRGB);

    static mm_status_t zoom(const MMBufferSP & srcBuf,
                        Format format,
                        uint32_t srcWidth,
                        uint32_t srcHeight,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        MMBufferSP & dstBuf,
                        uint32_t & dstWidth,
                        uint32_t & dstHeight);

    static mm_status_t zoom(const uint8_t *srcBuf,
                        Format format,
                        uint32_t srcWidth,
                        uint32_t srcHeight,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        MMBufferSP & dstBuf,
                        uint32_t & dstWidth,
                        uint32_t & dstHeight);

    static mm_status_t rotate(const MMBufferSP & srcBuf,
                        Format format,
                        uint32_t srcWidth,
                        uint32_t srcHeight,
                        int rotateAngle, // clockwise
                        MMBufferSP & dstBuf,
                        uint32_t * dstWidth = NULL,
                        uint32_t * dstHeight = NULL);
    static mm_status_t rotate(const uint8_t *srcBuf,
                        Format format,
                        uint32_t srcWidth,
                        uint32_t srcHeight,
                        int rotateAngle, // clockwise
                        MMBufferSP & dstBuf,
                        uint32_t * dstWidth = NULL,
                        uint32_t * dstHeight = NULL);

    static mm_status_t saveAsJpeg(const MMBufferSP & rgb,
                        Format format,
                        uint32_t width,
                        uint32_t height,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        bool bottomUp,
                        const char * jpegPath,
                        uint32_t & jpegWidth,
                        uint32_t & jpegHeight);

    static mm_status_t saveAsJpeg(const uint8_t * rgb,
                        Format format,
                        uint32_t width,
                        uint32_t height,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        bool bottomUp,
                        const char * jpegPath,
                        uint32_t & jpegWidth,
                        uint32_t & jpegHeight);

    static mm_status_t saveAsJpeg(const MMBufferSP & rgb,
                        Format format,
                        uint32_t width,
                        uint32_t height,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        bool bottomUp,
                        int rotateAngle,
                        const char * jpegPath,
                        uint32_t & jpegWidth,
                        uint32_t & jpegHeight);

    static mm_status_t saveAsJpeg(const uint8_t * rgb,
                        Format format,
                        uint32_t width,
                        uint32_t height,
                        uint32_t scaleNum,
                        uint32_t scaleDenom,
                        bool bottomUp,
                        int rotateAngle,
                        const char * jpegPath,
                        uint32_t & jpegWidth,
                        uint32_t & jpegHeight);


    MM_DISALLOW_COPY(RGB)
    DECLARE_LOGTAG()
};

}

#endif
