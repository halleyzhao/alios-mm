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

#define USE_JPEGLIB
#ifdef USE_JPEGLIB
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "jpeglib.h"
#include <setjmp.h>
#else
#include "turbojpeg.h"
#endif
#include "jpeg_decoder.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(JpegDecoder)

struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */

    jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

void my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

#ifdef USE_JPEGLIB
MMBufferSP JpegDecoder::decode(/* [in] */const uint8_t * srcBuf,
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

    MMLOGV("creating cinfo\n");
    struct jpeg_decompress_struct srcInfo;
    jpeg_create_decompress(&srcInfo);

    MMLOGV("setting error\n");
    struct my_error_mgr jerr;
    srcInfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        MMLOGE("error occured\n");
        jpeg_destroy_decompress(&srcInfo);
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMLOGV("set src\n");
    jpeg_mem_src(&srcInfo, (unsigned char *)srcBuf, (unsigned long)srcBufSize);
    MMLOGV("read header\n");
    (void) jpeg_read_header(&srcInfo, TRUE);
    MMLOGV("starting decompress\n");

    srcInfo.scale_num = scaleNum;
    srcInfo.scale_denom = scaleDenom;

    bool needConvert = false;
    switch (outFormat) {
        case RGB::kFormatRGB32:
            srcInfo.out_color_space = JCS_EXT_RGBX;
            break;
        case RGB::kFormatRGB24:
            srcInfo.out_color_space = JCS_EXT_RGB;
            break;
        case RGB::kFormatRGB565:
            srcInfo.out_color_space = JCS_EXT_RGB;
            needConvert = true;
            break;
        default:
            MMLOGE("unknown outFormat: %d\n", outFormat);
            return MMBufferSP((MMBuffer*)NULL);
    }

    (void) jpeg_start_decompress(&srcInfo);
    MMLOGV("after calc: scale_num: %d, scale_denom: %d, image_width: %d, image_height: %d, output_width: %d, output_height: %d, output_components: %d, out_color_components: %d\n",
        srcInfo.scale_num, srcInfo.scale_denom,
        srcInfo.image_width, srcInfo.image_height,
        srcInfo.output_width, srcInfo.output_height,
        srcInfo.output_components, srcInfo.out_color_components);

    width = srcInfo.output_width;
    height = srcInfo.output_height;

    int row_stride = srcInfo.output_width * srcInfo.output_components;
    MMBufferSP outRGB = MMBuffer::create(row_stride * srcInfo.output_height);
    if (!outRGB) {
        MMLOGE("no mem\n");
        (void) jpeg_finish_decompress(&srcInfo);

        jpeg_destroy_decompress(&srcInfo);
        return MMBufferSP((MMBuffer*)NULL);
    }
    outRGB->setActualSize(outRGB->getSize());

    uint8_t * dstBuf = outRGB->getWritableBuffer();

    JSAMPARRAY buffer = (*srcInfo.mem->alloc_sarray)
                ((j_common_ptr) &srcInfo, JPOOL_IMAGE, row_stride, 1);
    while (srcInfo.output_scanline < srcInfo.output_height) {
        (void) jpeg_read_scanlines(&srcInfo, buffer, 1);
        memcpy(dstBuf, buffer[0], row_stride);
        dstBuf += row_stride;
    }

    (void) jpeg_finish_decompress(&srcInfo);

    jpeg_destroy_decompress(&srcInfo);

    if (needConvert) {
        MMLOGV("convert to 565\n");
        MMBufferSP converted = RGB::convertFormat(RGB::kFormatRGB24,
            RGB::kFormatRGB565,
            width,
            height,
            outRGB);

        return converted;
    }

    return outRGB;
}


#else

MMBufferSP JpegDecoder::decode(/* [in] */const uint8_t * srcBuf,
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

    tjhandle handle = tjInitDecompress();
    if (!handle) {
        MMLOGE("Failed to init tj: %s\n", tjGetErrorStr());
        return MMBufferSP((MMBuffer*)NULL);
    }

    int h, w, subsamp;
    if (tjDecompressHeader2(handle,
        (unsigned char*)srcBuf,
        srcBufSize,
        &h,
        &w,
        &subsamp) == -1) {
        MMLOGE("failed to dec header\n");
        tjDestroy(handle);
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMLOGV("got info: h: %d, w: %d, subsamp: %d\n", w, h, subsamp);

    tjscalingfactor sf = {(int)scaleNum, (int)scaleDenom};
    width = TJSCALED(w, sf);
    height = TJSCALED(h, sf);

    bool needConvert = false;
    int pixelFormat;
    switch (outFormat) {
        case RGB::kFormatRGB32:
            pixelFormat = TJPF_RGBX;
            break;
        case RGB::kFormatRGB24:
            pixelFormat = TJPF_RGB;
            break;
        default:
            pixelFormat = TJPF_RGBX;
            needConvert = true;
            break;
    }

    size_t dstSize = width * height * tjPixelSize[pixelFormat];
    MMBufferSP dstBuf = MMBuffer::create(dstSize);
    if (!dstBuf) {
        MMLOGE("no mem\n");
        tjDestroy(handle);
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMLOGV("decompress: width: %d, height: %d, pixelFormat: %d, dstSize: %u\n", width, height, pixelFormat, dstSize);

    if (tjDecompress2(handle,
        (unsigned char*)srcBuf,
        (unsigned long)srcBufSize,
        dstBuf->getWritableBuffer(),
        width,
        0,
        height,
        pixelFormat,
        TJFLAG_BOTTOMUP)) {
        MMLOGE("Failed to decode: %s\n", tjGetErrorStr());
        return MMBufferSP((MMBuffer*)NULL);
    }

    tjDestroy(handle);

    dstBuf->setActualSize(dstSize);

    if (needConvert) {
        MMLOGV("convert to 565\n");
        MMBufferSP converted = RGB::convertFormat(RGB::kFormatRGB32,
            RGB::kFormatRGB565,
            width,
            height,
            dstBuf);

        return converted;
    }

    return dstBuf;
}
#endif


}
