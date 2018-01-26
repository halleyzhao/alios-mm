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

#ifndef __imagedecoder_imp_H
#define __imagedecoder_imp_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_buffer.h>
#include <multimedia/rgb.h>

namespace YUNOS_MM {

class ImageDecoderImp {
public:
    ImageDecoderImp() {}
    virtual ~ImageDecoderImp() {}

public:
    virtual MMBufferSP decode(/* [in] */const char * uri,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height);

    virtual MMBufferSP decode(/* [in] */const MMBufferSP & srcBuf,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height);

    virtual MMBufferSP decode(/* [in] */const uint8_t * srcBuf,
                        /*[in]*/size_t srcBufSize,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height) = 0;

    MM_DISALLOW_COPY(ImageDecoderImp)
    DECLARE_LOGTAG()
};

class SoftImageDecoder : public ImageDecoderImp {
public:
    SoftImageDecoder(){}
    virtual ~SoftImageDecoder(){}

    MM_DISALLOW_COPY(SoftImageDecoder)
};

}

#endif


