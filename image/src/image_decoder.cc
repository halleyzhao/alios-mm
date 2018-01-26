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

#include "image_decoder.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(ImageDecoderImp)

MMBufferSP ImageDecoderImp::decode(/* [in] */const char * uri,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    MMLOGV("uri: %s, scaleNum: %u, scaleDenom: %u, outFormat: %d\n", uri, scaleNum, scaleDenom);
    FILE * fp = NULL;
    fp = fopen(uri, "rb");
    if (!fp) {
        MMLOGE("failed to open file %s\n", uri);
        return MMBufferSP((MMBuffer*)NULL);
    }

    fseek(fp, 0, SEEK_END);
    size_t fSize = ftell(fp);
    MMLOGV("file size: %ld\n", fSize);
    fseek(fp, 0, SEEK_SET);

    if (fSize == 0) {
        MMLOGI("empty file: %s\n", uri);
        fclose(fp);
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMBufferSP buf;
    if (fSize > 0)
        buf = MMBuffer::create(fSize);
    if (!buf) {
        MMLOGE("failed to create buffer\n");
        fclose(fp);
        return MMBufferSP((MMBuffer*)NULL);
    }

    size_t ret = fread(buf->getWritableBuffer(), 1, fSize, fp);
    fclose(fp);
    if (ret != fSize) {
        MMLOGE("failed to read file\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    return decode(buf->getBuffer(),
                    fSize,
                    scaleNum,
                    scaleDenom,
                    outFormat,
                    width,
                    height);
}

MMBufferSP ImageDecoderImp::decode(/* [in] */const MMBufferSP & srcBuf,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    return decode(srcBuf->getBuffer(),
                    srcBuf->getActualSize(),
                    scaleNum,
                    scaleDenom,
                    outFormat,
                    width,
                    height);
}

}
