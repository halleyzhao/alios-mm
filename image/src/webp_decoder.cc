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

#include <webp/decode.h>
#include "webp_decoder.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(WebpDecoder)

/*static */void WebpDecoder::decodedDeleteFunc(uint8_t * buf)
{
    MMLOGV("+\n");
    if (buf) {
        free(buf);
        buf = NULL;
    }
}

MMBufferSP WebpDecoder::decode(/* [in] */const uint8_t * srcBuf,
                    /*[in]*/size_t srcBufSize,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    if (!srcBuf || scaleDenom == 0 || scaleNum == 0) {
        MMLOGE("invalid param: srcBuf: %p, scaleNum: %u, scaleDenom: %u\n", srcBuf, scaleNum, scaleDenom);
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMLOGV("srcBufSize: %u, scaleNum: %u, scaleDenom: %u, outFormat: %d\n",
        srcBufSize,
        scaleNum,
        scaleDenom,
        outFormat);

    int w,h;
    uint8_t * webpBuf;
    RGB::Format usedFormat;
    int decodecCount;
    switch (outFormat) {
        case RGB::kFormatRGB32:
            usedFormat = RGB::kFormatRGB32;
            decodecCount = 4;
            webpBuf = WebPDecodeRGBA(srcBuf, srcBufSize, &w, &h);
            break;
        default:
            usedFormat = RGB::kFormatRGB24;
            decodecCount = 3;
            webpBuf = WebPDecodeRGB(srcBuf, srcBufSize, &w, &h);
            break;
    }

    if (!webpBuf) {
        MMLOGE("failed to decode\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    width = w;
    height = h;

    MMLOGV("decoded: w: %d, h: %d\n", w, h);
    MMBufferSP decodedBuf = MMBuffer::fromBuffer(webpBuf, w * h * decodecCount, decodedDeleteFunc);

    MMBufferSP scaledBuf;
    if (scaleNum != scaleDenom) {
        MMLOGV("scale\n");
        uint32_t srcWidth = width;
        uint32_t srcHeight = height;
        if (RGB::zoom(decodedBuf,
                usedFormat,
                srcWidth,
                srcHeight,
                scaleNum,
                scaleDenom,
                scaledBuf,
                width,
                height) != MM_ERROR_SUCCESS) {
            MMLOGE("failed to zoom\n");
            return MMBufferSP((MMBuffer*)NULL);
        }
    } else {
        scaledBuf = decodedBuf;
    }

    MMBufferSP convertedBuf;
    if (usedFormat != outFormat) {
        convertedBuf = RGB::convertFormat(usedFormat,
                outFormat,
                width,
                height,
                scaledBuf);
    } else {
        convertedBuf = scaledBuf;
    }

    return convertedBuf;
}

}
