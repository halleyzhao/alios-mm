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

#include <png.h>
#include "png_decoder.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(PngDecoder)

struct PngSource {
    PngSource(const uint8_t * buf, size_t sz) : mBuf(buf), mSize(sz), mOffset(0)
    {}
    const uint8_t * mBuf;
    size_t mSize;
    int mOffset;
};

static void readcb(png_structp png_ptr, png_bytep data, png_size_t length)  
{
    PngSource * source = (PngSource*)png_get_io_ptr(png_ptr);
    if(source->mOffset + length <= source->mSize) {
        memcpy(data, source->mBuf + source->mOffset, length);
        source->mOffset += length;
        return;
    }

    png_error(png_ptr, "readcb failed");
}

MMBufferSP PngDecoder::decode(/* [in] */const uint8_t * srcBuf,
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

    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    int bit_depth, color_type, interlace_type;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
     if (png_ptr == NULL) {
        MMLOGE("failed create struct for\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        MMLOGE("failed create info structn");
        return MMBufferSP((MMBuffer*)NULL);
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        /* Free all of the memory associated with the png_ptr and info_ptr */
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        MMLOGE("read failed\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    PngSource pngSource(srcBuf, srcBufSize);
    png_set_read_fn(png_ptr, &pngSource,readcb);

    png_set_sig_bytes(png_ptr, sig_read);

    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
            &interlace_type, NULL, NULL);

    MMLOGV("width: %d, height: %d, bit_depth: %d, color_type: %d, interlace_type: %d\n",
            width, height, bit_depth, color_type, interlace_type);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    /**/
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0)
      png_set_tRNS_to_alpha(png_ptr);
    /**/

    png_read_update_info(png_ptr, info_ptr);

    size_t rowBytes = png_get_rowbytes(png_ptr, info_ptr);
    MMBufferSP outRGB = MMBuffer::create(rowBytes * height);
    if (!outRGB) {
        MMLOGE("no mem\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return MMBufferSP((MMBuffer*)NULL);
    }
    outRGB->setActualSize(outRGB->getSize());

    png_bytep row_pointers[height];
    uint8_t * p = outRGB->getWritableBuffer();
    for (png_uint_32 row = 0; row < height; row++) {
        row_pointers[row] = p;
        p += rowBytes;
    }

    png_read_image(png_ptr, row_pointers);

    png_read_end(png_ptr, info_ptr);

    MMLOGV("rowBytes: %u\n", rowBytes);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    RGB::Format pngFormat;
    if (rowBytes/width == 4) {
        pngFormat = RGB::kFormatRGB32;
    } else {
        pngFormat = RGB::kFormatRGB24;
    }

    MMBufferSP scaledBuf;
    if (scaleNum != scaleDenom) {
        MMLOGV("scale\n");
        uint32_t srcWidth = width;
        uint32_t srcHeight = height;
        if (RGB::zoom(outRGB,
                pngFormat,
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
        scaledBuf = outRGB;
    }

    MMBufferSP convertedBuf;
    if (pngFormat != outFormat) {
        convertedBuf = RGB::convertFormat(pngFormat,
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
