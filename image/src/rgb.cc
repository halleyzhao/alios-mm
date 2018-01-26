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
#include <unistd.h>
#include "turbojpeg.h"

#include <multimedia/rgb.h>
#include "utils.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>
//#define RGB_TIME_PROFILING
#ifdef RGB_TIME_PROFILING
#include <multimedia/media_monitor.h>
#endif

namespace YUNOS_MM {

MM_LOG_DEFINE_MODULE_NAME("RGB")

DEFINE_LOGTAG(RGB)
#ifdef RGB_TIME_PROFILING
static TimeCostStatics sSaveJpegTimeCost("MSTC_SaveJpeg");
static TimeCostStatics sSaveJpegZoomTimeCost("MSTC_Zoom");
#define CALCULATE_TIME_COST(tc) AutoTimeCost atc(tc)
#else
#define CALCULATE_TIME_COST(...)
#endif

#define OPEN_WRITE_FILE_MAX_RETRY 5
#define OPEN_WRITE_FILE_RETRY_INT 200000

#define OPEN_WRITE_FILE(_file, _fname) do {\
    int retryTimes = OPEN_WRITE_FILE_MAX_RETRY;\
    while (retryTimes-- > 0) {\
        if ((_file = fopen(_fname, "wb")) == NULL) {\
            MMLOGE("failed to open %s, err: %d(%s), retry\n", _fname, errno, strerror(errno));\
            usleep(OPEN_WRITE_FILE_RETRY_INT);\
            continue;\
        }\
        MMLOGV("success open: %s\n", _fname);\
        break;\
    }\
}while(0)


/*static */RGB::RGB32 RGB::getRGBValue(const uint8_t * buf,
                        Format format,
                        uint32_t width,
                        uint32_t x,
                        uint32_t y)
{
    return getRGBValue(getRGBAddr(buf, format, width, x, y),
                        format);
}

/*static */RGB::RGB32 RGB::getRGBValue(const uint8_t * addr,
                        Format format)
{
    if (!addr) {
        RGB32 rgb;
        memset(&rgb,0,4);
        return rgb;
    }

    RGB32 rgb;
    switch (format) {
        case kFormatRGB32:
            {
                const uint8_t * p = addr;
                rgb.blue = *(p++);
                rgb.green = *(p++);
                rgb.red = *(p++);
                rgb.alpha = *(p++);
            }
            break;
        case kFormatRGB24:
            {
                const uint8_t * p = addr;
                rgb.blue = *(p++);
                rgb.green = *(p++);
                rgb.red = *(p++);
                rgb.alpha = ALPHA_DEF_VAL;
            }
            break;
        case kFormatRGB565:
            {
                RGB565_2_32_S(&rgb, addr);
            }
            break;
        default:
            {
                RGB32 rgb;
                memset(&rgb,0,4);
                return rgb;
            }
    }

    return rgb;
}

/*static */const uint8_t * RGB::getRGBAddr(const uint8_t * buf,
                        Format format,
                        uint32_t width,
                        uint32_t x,
                        uint32_t y)
{
    if (!buf) {
        return NULL;
    }

    switch (format) {
        case kFormatRGB32:
            return buf + y * width * 4 + x * 4;
        case kFormatRGB24:
            return buf + y * width * 3 + x * 3;
        case kFormatRGB565:
            return buf + y * width * 2 + x * 2;
        default:
            return NULL;
    }
}

static MMBufferSP convertToRGB24(RGB::Format srcFormat,
                                            uint32_t width,
                                            uint32_t height,
                                            const MMBufferSP & srcRGB)
{
    MMLOGV("+\n");
    uint32_t dstSize = width * height * 3;
    MMBufferSP dataRet = MMBuffer::create(dstSize);
    if (!dataRet) {
        MMLOGE("no mem\n");
        return dataRet;
    }

    switch (srcFormat) {
        case RGB::kFormatRGB32:
            {
                uint32_t total = width * height;
                const uint8_t * p = srcRGB->getBuffer();
                uint8_t * q = dataRet->getWritableBuffer();
                for (uint32_t i = 0; i < total; ++i) {
                    *(q++) = *(p++);
                    *(q++) = *(p++);
                    *(q++) = *(p++);
                    ++p;
                }
                dataRet->setActualSize(dstSize);
                return dataRet;
            }
        case RGB::kFormatRGB24:
            return srcRGB;
        case RGB::kFormatRGB565:
        default:
            MMLOGE("src format %d not supported\n", srcFormat);
            return MMBufferSP((MMBuffer*)NULL);
    }
}

static MMBufferSP convertToRGB565(RGB::Format srcFormat,
                                            uint32_t width,
                                            uint32_t height,
                                            const MMBufferSP & srcRGB)
{
    MMLOGV("+\n");
    uint32_t dstSize = width * height * 2;
    MMBufferSP dataRet = MMBuffer::create(dstSize);
    if (!dataRet) {
        MMLOGE("no mem\n");
        return dataRet;
    }

    switch (srcFormat) {
        case RGB::kFormatRGB32:
            {
                uint32_t total = width * height;
                const uint8_t * p = srcRGB->getBuffer();
                uint8_t * q = dataRet->getWritableBuffer();
                for (uint32_t i = 0; i < total; ++i) {
                    RGB_2_565(*(p+2), *(p+1), *p, q);
                    p += 4;
                }
                dataRet->setActualSize(dstSize);
                return dataRet;
            }
        case RGB::kFormatRGB565:
            return srcRGB;
        case RGB::kFormatRGB24:
            {
                uint32_t total = width * height;
                const uint8_t * p = srcRGB->getBuffer();
                uint8_t * q = dataRet->getWritableBuffer();
                for (uint32_t i = 0; i < total; ++i) {
                    RGB_2_565(*(p+2), *(p+1), *p, q);
                    p += 3;
                }
                dataRet->setActualSize(dstSize);
                return dataRet;
            }
        default:
            MMLOGE("src format %d not supported\n", srcFormat);
            return MMBufferSP((MMBuffer*)NULL);
    }
}

static MMBufferSP convertToRGB32(RGB::Format srcFormat,
                                            uint32_t width,
                                            uint32_t height,
                                            const MMBufferSP & srcRGB)
{
    MMLOGI("srcFormat: %d, width: %u, height: %u\n", srcFormat, width, height);
    uint32_t dstSize = width * height * 4;
    MMBufferSP dataRet = MMBuffer::create(dstSize);
    if (!dataRet) {
        MMLOGE("no mem\n");
        return dataRet;
    }

    switch (srcFormat) {
        default:
        case RGB::kFormatRGB32:
            return srcRGB;
        case RGB::kFormatRGB24:
            {
                uint32_t total = width * height;
                const uint8_t * p = srcRGB->getBuffer();
                uint8_t * q = dataRet->getWritableBuffer();
                for (uint32_t i = 0; i < total; ++i) {
                    *(q++) = *(p++);
                    *(q++) = *(p++);
                    *(q++) = *(p++);
                    *(q++) = ALPHA_DEF_VAL;
                }
                dataRet->setActualSize(dstSize);
                return dataRet;
            }
        case RGB::kFormatRGB565:
            {
                uint32_t total = width * height;
                const uint8_t * p = srcRGB->getBuffer();
                uint8_t * q = dataRet->getWritableBuffer();
                for (uint32_t i = 0; i < total; ++i) {
                    RGB565_2_32(q, p);
                }
                dataRet->setActualSize(dstSize);
                return dataRet;
            }
    }
}

/*static */MMBufferSP RGB::convertFormat(Format srcFormat,
                                            Format dstFormat,
                                            uint32_t width,
                                            uint32_t height,
                                            const MMBufferSP & srcRGB)
{
    MMLOGI("srcFormat: %d, dstFormat: %d, width: %u, height: %u\n", srcFormat, dstFormat, width, height);
    if (!srcRGB) {
        MMLOGE("no data provided\n");
        return srcRGB;
    }
    if (srcFormat == dstFormat) {
        MMLOGV("the same\n");
        return srcRGB;
    }

    switch (dstFormat) {
        case kFormatRGB32:
            return convertToRGB32(srcFormat, width, height, srcRGB);
        case kFormatRGB24:
            return convertToRGB24(srcFormat, width, height, srcRGB);
        case kFormatRGB565:
            return convertToRGB565(srcFormat, width, height, srcRGB);
        default:
            MMLOGE("dest format %d not supported\n", dstFormat);
            return MMBufferSP((MMBuffer*)NULL);
    }
    MMLOGV("-\n");
}


inline double bilinear(double a, double b, int uv, int u1v, int uv1, int u1v1)
{
    return (double) (uv*(1-a)*(1-b)+u1v*a*(1-b)+uv1*b*(1-a)+u1v1*a*b);
}

mm_status_t doZoom32(const uint8_t * srcBuf,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    MMBufferSP & dstBuf,
                    uint32_t & dstWidth,
                    uint32_t & dstHeight)
{
    MMLOGV("+\n");
    if (!srcBuf ||
            scaleNum == 0 ||
            scaleDenom == 0 ||
            srcWidth == 0 ||
            srcHeight == 0) {
        MMLOGE("invalid args\n");
        return MM_ERROR_INVALID_PARAM;
    }

    double factor = (double)scaleNum / (double)scaleDenom;
    dstWidth = srcWidth * factor;
    dstHeight = srcHeight * factor;
    double iFactor = (double)scaleDenom / (double)scaleNum;
    double iFactor4 = (double)iFactor / (double)4;

    size_t dstSize = dstWidth * dstHeight * 4;
    dstBuf = MMBuffer::create(dstSize);
    if (!dstBuf) {
        MMLOGE("failed to create dst buf\n");
        return MM_ERROR_NO_MEM;
    }

    MMLOGV("srcWidth: %u, srcHeight: %u, scaleNum: %u, scaleDenom: %u, dstWidth: %u, dstHeight: %u, factor: %f, iFactor: %f\n",
            srcWidth,
            srcHeight,
            scaleNum,
            scaleDenom,
            dstWidth,
            dstHeight,
            factor,
            iFactor
            );

    uint32_t i, j;
    const uint8_t * src = srcBuf;
    uint8_t * dst = dstBuf->getWritableBuffer();

    uint8_t srcSize = srcWidth * srcHeight;
    uint32_t srcWidth4 = srcWidth * 4;
    for (i = 0; i < dstHeight; ++i) {
        for (j=0; j < dstWidth * 4; j += 4) {
            int ii, jj;
            double u, v;
            v = i * iFactor;
            u =  j * iFactor4;
            ii = (int)(v);
            jj = (int)(u);

            uint32_t uvPos = ii * srcWidth4 + jj * 4;
            uint32_t u1vPos = uvPos + 4;
            uint32_t uv1Pos = uvPos + srcWidth4;
            uint32_t u1v1Pos = uv1Pos + 4;

            if (u1v1Pos > srcSize) {
                const uint8_t * uv = &src[uvPos];
                *(dst++) = *(uv++);
                *(dst++) = *(uv++);
                *(dst++) = *(uv++);
            } else {
                const uint8_t * uv = &src[uvPos];
                const uint8_t * u1v = &src[u1vPos];
                const uint8_t * uv1 = &src[uv1Pos];
                const uint8_t * u1v1 = &src[u1v1Pos];
                *(dst++) = (uint8_t)bilinear(u - jj, v - ii, *(uv++), *(u1v++), *(uv1++), *(u1v1++));
                *(dst++) = (uint8_t)bilinear(u - jj, v - ii, *(uv++), *(u1v++), *(uv1++), *(u1v1++));
                *(dst++) = (uint8_t)bilinear(u - jj, v - ii, *(uv++), *(u1v++), *(uv1++), *(u1v1++));
            }
            *(dst++) = src[uvPos];
        }
    }

    dstBuf->setActualSize(dstSize);
    MMLOGV("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t doZoom24(const uint8_t * srcBuf,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    MMBufferSP & dstBuf,
                    uint32_t & dstWidth,
                    uint32_t & dstHeight)
{
    MMLOGV("+\n");
    if (!srcBuf ||
            scaleNum == 0 ||
            scaleDenom == 0 ||
            srcWidth == 0 ||
            srcHeight == 0) {
        MMLOGE("invalid args\n");
        return MM_ERROR_INVALID_PARAM;
    }

    double factor = (double)scaleNum / (double)scaleDenom;
    dstWidth = srcWidth * factor;
    dstHeight = srcHeight * factor;
    double iFactor = (double)scaleDenom / (double)scaleNum;
    double iFactor3 = (double)iFactor / (double)3;

    size_t dstSize = dstWidth * dstHeight * 3;
    dstBuf = MMBuffer::create(dstSize);
    if (!dstBuf) {
        MMLOGE("failed to create dst buf\n");
        return MM_ERROR_NO_MEM;
    }

    MMLOGV("srcWidth: %u, srcHeight: %u, scaleNum: %u, scaleDenom: %u, dstWidth: %u, dstHeight: %u, factor: %f, iFactor: %f\n",
            srcWidth,
            srcHeight,
            scaleNum,
            scaleDenom,
            dstWidth,
            dstHeight,
            factor,
            iFactor
            );

    uint32_t i, j;
    const uint8_t * src = srcBuf;
    uint8_t * dst = dstBuf->getWritableBuffer();

    uint8_t srcSize = srcWidth * srcHeight;
    uint32_t srcWidth3 = srcWidth * 3;

    for (i = 0; i < dstHeight; ++i) {
        for (j = 0; j < dstWidth * 3; j += 3) {
            int ii, jj;
            double u, v;
            v = i * iFactor;
            u =  j * iFactor3;
            ii = (int)(v);
            jj = (int)(u);

            uint32_t uvPos = ii * srcWidth3 + jj * 3;
            uint32_t u1vPos = uvPos + 3;
            uint32_t uv1Pos = uvPos + srcWidth3;
            uint32_t u1v1Pos = uv1Pos + 3;

            if (u1v1Pos > srcSize) {
                const uint8_t * uv = &src[uvPos];
                *(dst++) = *(uv++);
                *(dst++) = *(uv++);
                *(dst++) = *(uv++);
            } else {
                const uint8_t * uv = &src[uvPos];
                const uint8_t * u1v = &src[u1vPos];
                const uint8_t * uv1 = &src[uv1Pos];
                const uint8_t * u1v1 = &src[u1v1Pos];
                *(dst++) = (uint8_t)bilinear(u - jj, v - ii, *(uv++), *(u1v++), *(uv1++), *(u1v1++));
                *(dst++) = (uint8_t)bilinear(u - jj, v - ii, *(uv++), *(u1v++), *(uv1++), *(u1v1++));
                *(dst++) = (uint8_t)bilinear(u - jj, v - ii, *(uv++), *(u1v++), *(uv1++), *(u1v1++));
            }
        }
    }

    dstBuf->setActualSize(dstSize);
    MMLOGV("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t doZoom565(const uint8_t * srcBuf,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    MMBufferSP & dstBuf,
                    uint32_t & dstWidth,
                    uint32_t & dstHeight)
{
    MMLOGV("+\n");
    if (!srcBuf ||
            scaleNum == 0 ||
            scaleDenom == 0 ||
            srcWidth == 0 ||
            srcHeight == 0) {
        MMLOGE("invalid args\n");
        return MM_ERROR_INVALID_PARAM;
    }

    double factor = (double)scaleNum / (double)scaleDenom;
    dstWidth = srcWidth * factor;
    dstHeight = srcHeight * factor;
    double iFactor = (double)scaleDenom / (double)scaleNum;
    double iFactor2 = (double)iFactor / (double)2;

    size_t dstSize = dstWidth * dstHeight * 2;
    dstBuf = MMBuffer::create(dstSize);
    if (!dstBuf) {
        MMLOGE("failed to create dst buf\n");
        return MM_ERROR_NO_MEM;
    }

    MMLOGV("srcWidth: %u, srcHeight: %u, scaleNum: %u, scaleDenom: %u, dstWidth: %u, dstHeight: %u, factor: %f, iFactor: %f\n",
            srcWidth,
            srcHeight,
            scaleNum,
            scaleDenom,
            dstWidth,
            dstHeight,
            factor,
            iFactor
            );

    uint32_t i, j;
    const uint8_t * src = srcBuf;
    uint8_t * dst = dstBuf->getWritableBuffer();

    uint8_t srcSize = srcWidth * srcHeight;
    uint32_t srcWidth2 = srcWidth * 2;

    for (i = 0; i < dstHeight; ++i) {
        for (j = 0; j < dstWidth * 2; j += 2) {
            int ii, jj;
            double u, v;
            v = i * iFactor;
            u =  j * iFactor2;
            ii = (int)(v);
            jj = (int)(u);

            uint32_t uvPos = ii * srcWidth2 + jj * 2;
            uint32_t u1vPos = uvPos + 2;
            uint32_t uv1Pos = uvPos + srcWidth2;
            uint32_t u1v1Pos = uv1Pos + 2;

            if (u1v1Pos > srcSize) {
                const uint8_t * uv = &src[uvPos];
                *(dst++) = *(uv++);
                *(dst++) = *(uv++);
            } else {
                RGB::RGB32 uvRGB, u1vRGB, uv1RGB, u1v1RGB;
                RGB565_2_32_S(&uvRGB, &src[uvPos]);
                RGB565_2_32_S(&u1vRGB, &src[u1vPos]);
                RGB565_2_32_S(&uv1RGB, &src[uv1Pos]);
                RGB565_2_32_S(&u1v1RGB, &src[u1v1Pos]);
                uint8_t b = (uint8_t)bilinear(u - jj, v - ii, uvRGB.blue, u1vRGB.blue, uv1RGB.blue, u1v1RGB.blue);
                uint8_t g = (uint8_t)bilinear(u - jj, v - ii, uvRGB.green, u1vRGB.green, uv1RGB.green, u1v1RGB.green);
                uint8_t r = (uint8_t)bilinear(u - jj, v - ii, uvRGB.red, u1vRGB.red, uv1RGB.red, u1v1RGB.red);
                RGB_2_565(r, g, b, dst);
            }
        }
    }

    dstBuf->setActualSize(dstSize);
    MMLOGV("-\n");
    return MM_ERROR_SUCCESS;
}


/*static */mm_status_t RGB::zoom(const MMBufferSP & srcBuf,
                    Format format,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    MMBufferSP & dstBuf,
                    uint32_t & dstWidth,
                    uint32_t & dstHeight)
{
    return zoom(srcBuf->getBuffer(),
                    format,
                    srcWidth,
                    srcHeight,
                    scaleNum,
                    scaleDenom,
                    dstBuf,
                    dstWidth,
                    dstHeight);
}

/*static */mm_status_t RGB::zoom(const uint8_t *srcBuf,
                    Format format,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    MMBufferSP & dstBuf,
                    uint32_t & dstWidth,
                    uint32_t & dstHeight)
{
    CALCULATE_TIME_COST(sSaveJpegZoomTimeCost);
    switch (format) {
        case kFormatRGB32:
            MMLOGV("RGB32\n");
            return doZoom32(srcBuf,
                            srcWidth,
                            srcHeight,
                            scaleNum,
                            scaleDenom,
                            dstBuf,
                            dstWidth,
                            dstHeight);
        case kFormatRGB24:
            MMLOGV("RGB24\n");
            return doZoom24(srcBuf,
                            srcWidth,
                            srcHeight,
                            scaleNum,
                            scaleDenom,
                            dstBuf,
                            dstWidth,
                            dstHeight);
        case kFormatRGB565:
            MMLOGV("RGB565\n");
            return doZoom565(srcBuf,
                            srcWidth,
                            srcHeight,
                            scaleNum,
                            scaleDenom,
                            dstBuf,
                            dstWidth,
                            dstHeight);
        default:
            MMLOGE("invalid format: %d\n", format);
            return MM_ERROR_INVALID_PARAM;
    }
}

/*static */mm_status_t RGB::rotate(const MMBufferSP & srcBuf,
                    Format format,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    int rotateAngle, // clockwise
                    MMBufferSP & dstBuf,
                    uint32_t * dstWidth/* = NULL*/,
                    uint32_t * dstHeight/* = NULL*/)
{
    return rotate(srcBuf->getBuffer(),
                    format,
                    srcWidth,
                    srcHeight,
                    rotateAngle,
                    dstBuf,
                    dstWidth,
                    dstHeight);
}

/*static */mm_status_t RGB::rotate(const uint8_t *srcBuf,
                    Format format,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    int rotateAngle, // clockwise
                    MMBufferSP & dstBuf,
                    uint32_t * dstWidth/* = NULL*/,
                    uint32_t * dstHeight/* = NULL*/)
{
    if (!srcBuf) {
            MMLOGE("invalid buf: %d\n", format);
            return MM_ERROR_INVALID_PARAM;
    }
    int components;
    switch (format) {
        case kFormatRGB32:
            components = 4;
            break;
        case kFormatRGB24:
            components = 3;
            break;
        case kFormatRGB565:
            components = 2;
            break;
        default:
            MMLOGE("invalid format: %d\n", format);
            return MM_ERROR_INVALID_PARAM;
    }

    uint32_t stride = srcWidth * components;
    dstBuf = MMBuffer::create(stride * srcHeight);
    if (!dstBuf) {
        MMLOGE("no mem\n");
        return false;
    }
    dstBuf->setActualSize(dstBuf->getSize());
    MMLOGV("w: %u, h: %u, comp: %u, size: %u\n", srcWidth, srcHeight, components, dstBuf->getSize());
    switch (rotateAngle) {
        case 90:
        case -270:
            {
                MMLOGV("90\n");
                const uint8_t * src_p = srcBuf;
                uint8_t * dst_p = dstBuf->getWritableBuffer();
                int a = 0;
                for (int i = 0; i < (int)srcWidth; ++i, a += components) {
                    int b = (int)(srcHeight - 1) * stride;
                    for (int j = (int)srcHeight - 1; j >= 0; --j, b -= stride) {
                        memcpy(dst_p, &src_p[b + a], components);
                        dst_p += components;
                    }
                }

                if (dstWidth)
                    *dstWidth = srcHeight;
                if (dstHeight)
                    *dstHeight = srcWidth;
            }
            MMLOGV("90 over\n");
            return MM_ERROR_SUCCESS;

        case 270:
        case -90:
            {
                MMLOGV("270\n");
                const uint8_t * src_p = srcBuf;
                uint8_t * dst_p = dstBuf->getWritableBuffer();
                int a = (int)(srcWidth - 1) * components;
                for (int i = (int)srcWidth - 1; i >= 0; --i, a -= components) {
                    int b = 0;
                    for (int j = 0; j < (int)srcHeight; ++j, b += stride) {
                        memcpy(dst_p, &src_p[b + a], components);
                        dst_p += components;
                    }
                }

                if (dstWidth)
                    *dstWidth = srcHeight;
                if (dstHeight)
                    *dstHeight = srcWidth;
            }
            return MM_ERROR_SUCCESS;

        case 180:
        case -180:
            {
                MMLOGV("180\n");
                const uint8_t * src_p = srcBuf;
                uint8_t * dst_p = dstBuf->getWritableBuffer();
                int a = (int)(srcHeight - 1) * stride;
                for (int32_t i = srcHeight - 1; i >= 0; --i, a -= stride) {
                    int32_t b = (srcWidth - 1) * components;
                    for (int32_t j = srcWidth - 1; j >= 0; --j, b -= components) {
                        memcpy(dst_p, &src_p[b + a], components);
                        dst_p += components;
                    }
                }

                if (dstWidth)
                    *dstWidth = srcWidth;
                if (dstHeight)
                    *dstHeight = srcHeight;
            }
            return MM_ERROR_SUCCESS;

        default:
            MMLOGE("invalid angle: %d\n", format);
            return MM_ERROR_INVALID_PARAM;
    }
}

/*static */mm_status_t RGB::saveAsJpeg(const MMBufferSP & rgb,
                    Format format,
                    uint32_t width,
                    uint32_t height,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    bool bottomUp,
                    const char * jpegPath,
                    uint32_t & jpegWidth,
                    uint32_t & jpegHeight)
{
    return saveAsJpeg(rgb->getBuffer(),
                    format,
                    width,
                    height,
                    scaleNum,
                    scaleDenom,
                    bottomUp, 
                    jpegPath,
                    jpegWidth,
                    jpegHeight);
}

/*static */mm_status_t RGB::saveAsJpeg(const uint8_t * rgb,
                    Format format,
                    uint32_t width,
                    uint32_t height,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    bool bottomUp,
                    const char * jpegPath,
                    uint32_t & jpegWidth,
                    uint32_t & jpegHeight)
{
    CALCULATE_TIME_COST(sSaveJpegTimeCost);

    MMLOGV("format: %d, width: %u, height: %u, scaleNum: %u, scaleDenom: %u, bottomUp: %d, jpegPath: %s\n",
                format,width, height, scaleNum, scaleDenom, bottomUp, jpegPath);
    if (!rgb || !jpegPath || scaleNum == 0 || scaleDenom == 0) {
        MMLOGE("Invalid args\n");
        return MM_ERROR_INVALID_PARAM;
    }

    MMBufferSP scaled;
    if (scaleNum == scaleDenom) {
        MMLOGV("no scale\n");
        int count;
        switch (format) {
            case RGB::kFormatRGB32:
                count = 4;
                break;
            case RGB::kFormatRGB24:
                count = 3;
            default:
                count = 2;
        }
        scaled = MMBuffer::fromBuffer((uint8_t*)rgb, width * height * count);
        jpegWidth = width;
        jpegHeight = height;
    } else {
        MMLOGV("scale, zoom first\n");
        if (zoom(rgb,
                    format,
                    width,
                    height,
                    scaleNum,
                    scaleDenom,
                    scaled,
                    jpegWidth,
                    jpegHeight) != MM_ERROR_SUCCESS
                    || !scaled) {
            MMLOGE("failed to scale\n");
            return MM_ERROR_OP_FAILED;
        }
    }

    MMLOGV("fromat: %d, width: %u, height: %u, dstW: %u, dstH: %u, num: %u, den: %u, jpegPath: %s\n",
        format,
        width,
        height,
        jpegWidth,
        jpegHeight,
        scaleNum,
        scaleDenom,
        jpegPath);

    TJPF colorfomatTJ;
    switch (format) {
        case kFormatRGB32:
            colorfomatTJ = TJPF_RGBX;
            break;
        case kFormatRGB24:
            colorfomatTJ = TJPF_RGB;
            break;
        default:
            MMBufferSP data = convertFormat(format,
                                kFormatRGB32,
                                jpegWidth,
                                jpegHeight,
                                scaled);
            if (!data) {
                MMLOGE("failed to convert\n");
                return MM_ERROR_OP_FAILED;
            }
            scaled = data;
            colorfomatTJ = TJPF_RGBX;
            break;
    }

    MMLOGV("init tj\n");
    tjhandle handle = tjInitCompress();
    if (!handle) {
        MMLOGE("failed to init tj\n");
        return MM_ERROR_NO_MEM;
    }

    uint8_t * jpegBuf = NULL;
    unsigned long jpegBufSize = 0;
    if (tjCompress2(handle,
                scaled->getWritableBuffer(),
                jpegWidth,
                0,
                jpegHeight,
                colorfomatTJ,
                &jpegBuf,
                &jpegBufSize,
                0,
                100,
                bottomUp ? TJFLAG_BOTTOMUP : 0) == -1) {
        tjDestroy(handle);
        handle = NULL;
        MMLOGE("failed to compress\n");
        return MM_ERROR_OP_FAILED;
    }
    MMLOGV("tjCompress2 success\n");

    FILE * thumbFile;
    OPEN_WRITE_FILE(thumbFile, jpegPath);
    if (!thumbFile) {
        MMLOGE("can't open %s\n", jpegPath);
        tjFree(jpegBuf);
        tjDestroy(handle);
        handle = NULL;
        return MM_ERROR_OP_FAILED;
    }

    if (fwrite(jpegBuf, 1, jpegBufSize, thumbFile) != jpegBufSize) {
        MMLOGE("failed to write\n");
        tjFree(jpegBuf);
        tjDestroy(handle);
        handle = NULL;
        fclose(thumbFile);
        remove(jpegPath);
        return MM_ERROR_OP_FAILED;
    }

    tjFree(jpegBuf);
    tjDestroy(handle);
    handle = NULL;
    fclose(thumbFile);

    return MM_ERROR_SUCCESS;
}

/*static */mm_status_t RGB::saveAsJpeg(const MMBufferSP & rgb,
                    Format format,
                    uint32_t width,
                    uint32_t height,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    bool bottomUp,
                    int rotateAngle,
                    const char * jpegPath,
                    uint32_t & jpegWidth,
                    uint32_t & jpegHeight)
{
    return saveAsJpeg(rgb->getBuffer(),
                    format,
                    width,
                    height,
                    scaleNum,
                    scaleDenom,
                    bottomUp, 
                    rotateAngle,
                    jpegPath,
                    jpegWidth,
                    jpegHeight);
}

/*static */mm_status_t RGB::saveAsJpeg(const uint8_t * rgb,
                    Format format,
                    uint32_t width,
                    uint32_t height,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    bool bottomUp,
                    int rotateAngle,
                    const char * jpegPath,
                    uint32_t & jpegWidth,
                    uint32_t & jpegHeight)
{
    if (rotateAngle == 0) {
        return saveAsJpeg(rgb,
                        format,
                        width,
                        height,
                        scaleNum,
                        scaleDenom,
                        bottomUp, 
                        jpegPath,
                        jpegWidth,
                        jpegHeight);
    }

    MMBufferSP rotated;
    uint32_t rotatedW, rotatedH;
    if (rotate(rgb,
            format,
            width,
            height,
            rotateAngle,
            rotated,
            &rotatedW,
            &rotatedH) != MM_ERROR_SUCCESS) {
        return MM_ERROR_OP_FAILED;
    }

    return saveAsJpeg(rotated,
                format,
                rotatedW,
                rotatedH,
                scaleNum,
                scaleDenom,
                bottomUp, 
                jpegPath,
                jpegWidth,
                jpegHeight);

}
















#if 0
inline uint32_t bilinear(uint32_t dstW, int32_t dstH, int32_t stepX, int32_t stepY, int uv, int u1v, int uv1, int u1v1)
{
/*    return (uv * (dstW-stepX)*(dstH-stepY) +
                u1v * stepX * (dstH - stepY) +
                uv1 * (dstW - stepX) * stepY +
                u1v1 * stepX * stepY);*/
    uint32_t i = (uv * (dstW-stepX)*(dstH-stepY) +
                u1v * stepX * (dstH - stepY) +
                uv1 * (dstW - stepX) * stepY +
                u1v1 * stepX * stepY);
    //MMLOGV("color: %u -> %u\n", i, i / dstW / dstH);
    i += (dstW*dstH/2);
    i /=  (dstW* dstH);
    MMASSERT(i < 256);
    return i;
}

mm_status_t doZoom32(const uint8_t * srcBuf,
                    uint32_t srcWidth,
                    uint32_t srcHeight,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    MMBufferSP & dstBuf,
                    uint32_t & dstWidth,
                    uint32_t & dstHeight)
{
    MMLOGV("+%u\n", time(0));
    if (!srcBuf ||
            scaleNum == 0 ||
            scaleDenom == 0 ||
            srcWidth == 0 ||
            srcHeight == 0) {
        MMLOGE("invalid args\n");
        return MM_ERROR_INVALID_PARAM;
    }

    double factor = (double)scaleNum / (double)scaleDenom;
    dstWidth = srcWidth * factor;
    dstHeight = srcHeight * factor;

    size_t dstSize = dstWidth * dstHeight * 4;
    MMLOGV("dstSize: %zu\n", dstSize);
    dstBuf = MMBuffer::create(dstSize);
    if (!dstBuf) {
        MMLOGE("failed to create dst buf\n");
        return MM_ERROR_NO_MEM;
    }

    MMLOGV("srcWidth: %u, srcHeight: %u, scaleNum: %u, scaleDenom: %u, dstWidth: %u, dstHeight: %u\n",
            srcWidth,
            srcHeight,
            scaleNum,
            scaleDenom,
            dstWidth,
            dstHeight
            );

    uint32_t i, j;
    const uint8_t * src = srcBuf;
    uint8_t * dst = dstBuf->getWritableBuffer();

    uint32_t srcx = 0, srcy = 0;
    uint32_t stepx = 0, stepy = 0;
    uint32_t srcw1 = srcWidth - 1;
    uint32_t srch1 = srcHeight - 1;
    uint32_t dstw1 = dstWidth - 1;
    uint32_t dsth1 = dstHeight - 1;
    for (i = 0; i < dstHeight; ++i) {
        for (j=0; j < dstWidth * 4; j += 4) {
            const uint8_t * uv = &src[srcy * srcWidth * 4 + srcx];
            const uint8_t * u1v = &src[srcy * srcWidth * 4 + srcx + 4];
            const uint8_t * uv1 = &src[(srcy + 1) * srcWidth * 4 + srcx];
            const uint8_t * u1v1 = &src[(srcy + 1) * srcWidth * 4 + srcx + 4];

            *(dst++) = (uint8_t)bilinear(dstWidth,
                                    dstHeight,
                                    stepx,
                                    stepy,
                                    *(uv++),
                                    *(u1v++),
                                    *(uv1++),
                                    *(u1v1++));
            *(dst++) = (uint8_t)bilinear(dstWidth,
                                    dstHeight,
                                    stepx,
                                    stepy,
                                    *(uv++),
                                    *(u1v++),
                                    *(uv1++),
                                    *(u1v1++));
            *(dst++) = (uint8_t)bilinear(dstWidth,
                                    dstHeight,
                                    stepx,
                                    stepy,
                                    *(uv++),
                                    *(u1v++),
                                    *(uv1++),
                                    *(u1v1++));
            *(dst++) = 0;

            stepx += srcw1;
            while (stepx > dstw1) {
                stepx -= dstw1;
                srcx += 4;
            }
        }
        stepx = 0;
        srcx = 0;
        stepy += srch1;
        while (stepy > dsth1) {
            stepy -= dsth1;
            srcy++;
        }
    }

    dstBuf->setActualSize(dstSize);
    MMLOGV("-%u\n", time(0));
    return MM_ERROR_SUCCESS;
}
#endif

}
