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

#include <stdio.h>
#include "turbojpeg.h"

#include <multimedia/media_attr_str.h>
#include <multimedia/rgb.h>
#include "utils.h"

#include "jpeg_decoder.h"
#include "png_decoder.h"
#include "gif_decoder.h"
#include "webp_decoder.h"
#include "multimedia/bmp.h"
#include "multimedia/wbmp.h"

#include <multimedia/image.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(ImageDecoder)
MM_LOG_DEFINE_MODULE_NAME("ImageDecoder")


class BmpDecoder : public SoftImageDecoder {
public:
    BmpDecoder() {}
    ~BmpDecoder() {}

public:
    virtual MMBufferSP decode(/* [in] */const char * uri,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height) {
        return BMP::decode(uri, scaleNum, scaleDenom, outFormat, width, height);
    }

    virtual MMBufferSP decode(/* [in] */const MMBufferSP & srcBuf,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height) {
        return BMP::decode(srcBuf, scaleNum, scaleDenom, outFormat, width, height);
    }

    virtual MMBufferSP decode(/* [in] */const uint8_t * srcBuf,
                        /*[in]*/size_t srcBufSize,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height) {
        return BMP::decode(srcBuf, srcBufSize, scaleNum, scaleDenom, outFormat, width, height);
    }

private:
    MM_DISALLOW_COPY(BmpDecoder)
};

class WBmpDecoder : public SoftImageDecoder {
public:
    WBmpDecoder() {}
    ~WBmpDecoder() {}

public:
    virtual MMBufferSP decode(/* [in] */const char * uri,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height) {
        return WBMP::decode(uri, outFormat, width, height);
    }

    virtual MMBufferSP decode(/* [in] */const MMBufferSP & srcBuf,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height) {
        return WBMP::decode(srcBuf, outFormat, width, height);
    }

    virtual MMBufferSP decode(/* [in] */const uint8_t * srcBuf,
                        /*[in]*/size_t srcBufSize,
                        /* [in] */uint32_t scaleNum,
                        /* [in] */uint32_t scaleDenom,
                        /* [in] */RGB::Format outFormat,
                        /* [out] */uint32_t & width,
                        /* [out] */uint32_t & height) {
        return WBMP::decode(srcBuf, scaleDenom, outFormat, width, height);
    }

private:
    MM_DISALLOW_COPY(WBmpDecoder)
};


#define MIME_ITEM(_mime, _type) do {\
    if (!strcasecmp(mime, _mime)) {\
        MMLOGV("%s\n", #_mime);\
        return new _type();\
    }\
}while(0)


static ImageDecoderImp * createDecoderByMime(const char * mime)
{
    MIME_ITEM(MEDIA_MIMETYPE_IMAGE_JPEG, JpegDecoder);
    MIME_ITEM(MEDIA_MIMETYPE_IMAGE_BMP, BmpDecoder);
    MIME_ITEM(MEDIA_MIMETYPE_IMAGE_WBMP, WBmpDecoder);
    MIME_ITEM(MEDIA_MIMETYPE_IMAGE_PNG, PngDecoder);
    MIME_ITEM(MEDIA_MIMETYPE_IMAGE_GIF, GifDecoder);
    MIME_ITEM(MEDIA_MIMETYPE_IMAGE_WEBP, WebpDecoder);

    MMLOGE("unsupported mime: %s\n", mime);
    return NULL;
}

#define EXT_ITEM(_ext, _type) do {\
    if (!strcasecmp(extension, _ext)) {\
        MMLOGV("%s\n", #_ext);\
        return new _type();\
    }\
}while(0)

static ImageDecoderImp * createDecoderByExtension(const char * extension)
{
    EXT_ITEM("jpg", JpegDecoder);
    EXT_ITEM("jpeg", JpegDecoder);
    EXT_ITEM("bmp", BmpDecoder);
    EXT_ITEM("wbmp", WBmpDecoder);
    EXT_ITEM("png", PngDecoder);
    EXT_ITEM("gif", GifDecoder);
    EXT_ITEM("webp", WebpDecoder);

    MMLOGE("unsupported extension: %s\n", extension);
    return NULL;
}


/*static */MMBufferSP ImageDecoder::decode(/* [in] */const char * uri,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    const char * extension = strrchr(uri, '.');
    if (!extension) {
        MMLOGE("unsupported uri: %s\n", uri);
        return MMBufferSP((MMBuffer*)NULL);
    }

    ImageDecoderImp * decoder;
    try {
        decoder = createDecoderByExtension(extension + 1);
    } catch (...) {
        MMLOGE("no mem\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMBufferSP buf = decoder->decode(uri,
        scaleNum,
        scaleDenom,
        outFormat,
        width,
        height);

    delete decoder;
    return buf;
}

/*static */MMBufferSP ImageDecoder::decode(/* [in] */const MMBufferSP & srcBuf,
                    /* [in] */const char * mime,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    ImageDecoderImp * decoder;
    try {
        decoder = createDecoderByMime(mime);
    } catch (...) {
        MMLOGE("no mem\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMBufferSP buf = decoder->decode(srcBuf,
        scaleNum,
        scaleDenom,
        outFormat,
        width,
        height);

    delete decoder;
    return buf;
}

/*static */MMBufferSP ImageDecoder::decode(/* [in] */const uint8_t * srcBuf,
                    /*[in]*/size_t srcBufSize,
                    /* [in] */const char * mime,
                    /* [in] */uint32_t scaleNum,
                    /* [in] */uint32_t scaleDenom,
                    /* [in] */RGB::Format outFormat,
                    /* [out] */uint32_t & width,
                    /* [out] */uint32_t & height)
{
    ImageDecoderImp * decoder;
    try {
        decoder = createDecoderByMime(mime);
    } catch (...) {
        MMLOGE("no mem\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMBufferSP buf = decoder->decode(srcBuf,
        srcBufSize,
        scaleNum,
        scaleDenom,
        outFormat,
        width,
        height);

    delete decoder;
    return buf;
}

}
