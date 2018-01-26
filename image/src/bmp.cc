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
#include "utils.h"

#include <multimedia/bmp.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

MM_LOG_DEFINE_MODULE_NAME("bmp")

#pragma pack(push,1)
struct BmpHeader {
    uint8_t magic[2];
    uint32_t fileSize;
    uint16_t creator1;
    uint16_t creator2;
    uint32_t offset;
};

struct BmpDibHeader {
    uint32_t headerSize;
    uint32_t width;
    uint32_t height;
    uint16_t nplanes;
    uint16_t depth;
    uint32_t compressType;
    uint32_t bmpByteSize;
    uint32_t hres;
    uint32_t vres;
    uint32_t ncolors;
    uint32_t nimpcolors;
};
#pragma pack(pop)



DEFINE_LOGTAG(BMP)

#define swapOrderU32(_val)  ((uint32_t) (((((uint32_t) (_val)) & (uint32_t) 0x000000ff) << 24) | ((((uint32_t) (_val)) & (uint32_t) 0x0000ff00) <<  8) | ((((uint32_t) (_val)) & (uint32_t) 0x00ff0000) >>  8) | ((((uint32_t) (_val)) & (uint32_t) 0xff000000) >> 24)) )

#define swapOrderU16(_val) ((uint16_t)((((uint16_t)_val) >> 8) |(((uint16_t)_val) << 8)))

#define WIDTHBYTES(bits) (((bits)+31)/32*4)

#define DEC_RETRIEVE_BEGIN_RGB32()\
    uint32_t widthDep = WIDTHBYTES(width * depth);\
    uint8_t * p = data->getWritableBuffer();\
    for (uint32_t i = 0; i < height; ++i) {\
        uint32_t curY = widthDep * i;\
        for (uint32_t j = 0; j < width; ++j) {

#define DEC_RETRIEVE_RGB32(_r, _g, _b, _a) \
            *(p++) = _b;\
            *(p++) = _g;\
            *(p++) = _r;\
            *(p++) = _a;

#define DEC_RETRIEVE_END_RGB32()\
        }\
    }

#define DEC_RETRIEVE_BEGIN_RGB24()\
    uint32_t widthDep = WIDTHBYTES(width * depth);\
    uint8_t * p = data->getWritableBuffer();\
    for (uint32_t i = 0; i < height; ++i) {\
        uint32_t curY = widthDep * i;\
        for (uint32_t j = 0; j < width; ++j) {

#define DEC_RETRIEVE_RGB24(_r, _g, _b) \
            *(p++) = _b;\
            *(p++) = _g;\
            *(p++) = _r;

#define DEC_RETRIEVE_END_RGB24()\
        }\
    }

#define DEC_RETRIEVE_BEGIN_RGB565()\
    uint32_t widthDep = WIDTHBYTES(width * depth);\
    uint8_t * p = data->getWritableBuffer();\
    for (uint32_t i = 0; i < height; ++i) {\
        uint32_t curY = widthDep * i;\
        for (uint32_t j = 0; j < width; ++j) {

#define DEC_RETRIEVE_RGB565(_r, _g, _b) \
            RGB_2_565(_r, _g, _b, p);

#define DEC_RETRIEVE_END_RGB565()\
        }\
    }

static void swapOrder(BmpDibHeader * dib)
{
    dib->depth = swapOrderU16(dib->depth);
    dib->headerSize = swapOrderU32(dib->headerSize);
    dib->nplanes = swapOrderU16(dib->nplanes);
    dib->compressType = swapOrderU32(dib->compressType);
    dib->nimpcolors = swapOrderU32(dib->nimpcolors);
    dib->hres = swapOrderU32(dib->hres);
    dib->vres = swapOrderU32(dib->vres);
    dib->height = swapOrderU32(dib->height);
    dib->ncolors = swapOrderU32(dib->ncolors);
    dib->width = swapOrderU32(dib->width);
    dib->bmpByteSize = swapOrderU32(dib->bmpByteSize);
}

static bool needSwap()
{
    uint16_t test = 0x1;
    if ( (*(uint8_t *)&test) != 0x1 ) {
        MMLOGV("need conv\n");
        return true;
    }

    return false;
}

/*static */MMBufferSP BMP::decode(const char * path,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    RGB::Format outFormat,
                    uint32_t & width,
                    uint32_t & height)
{
    FILE * fp = NULL;

    if (!path) {
        MMLOGE("invalid args\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    MMLOGV("path: %s, scaleNum: %u, scaleDenom: %u, outFormat: %d\n",
        path, scaleNum, scaleDenom, outFormat);

    fp = fopen(path, "rb");
    if (!fp) {
        MMLOGE("failed to open file %s\n", path);
        return MMBufferSP((MMBuffer*)NULL);
    }

    fseek(fp, 0, SEEK_END);
    long fSize = ftell(fp);
    MMLOGV("file size: %ld\n", fSize);
    if (fSize <= 0) {
        MMLOGE("failed to ftell or file size is 0: %s: %s(%d)", path, errno, strerror(errno));
        fclose(fp);
        return MMBufferSP((MMBuffer*)NULL);
    }
    fseek(fp, 0, SEEK_SET);

    MMBufferSP buf;
    buf = MMBuffer::create(fSize);
    if (!buf) {
        MMLOGE("failed to create buffer\n");
        fclose(fp);
        return MMBufferSP((MMBuffer*)NULL);
    }
    uint8_t * fBuf = buf->getWritableBuffer();

    size_t ret = fread(fBuf, 1, fSize, fp);
    if ( ret < (size_t)fSize ) {
        MMLOGE("file is too small\n");
        fclose(fp);
        return MMBufferSP((MMBuffer*)NULL);
    }

    fclose(fp);
    buf->setActualSize(fSize);
    return decode(buf,
                    scaleNum,
                    scaleDenom,
                    outFormat,
                    width,
                    height);
}

/*static */MMBufferSP BMP::decode(const MMBufferSP & srcBuf,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    RGB::Format outFormat,
                    uint32_t & width,
                    uint32_t & height)
{
    return decode(srcBuf->getBuffer(),
                    srcBuf->getActualSize(),
                    scaleNum,
                    scaleDenom,
                    outFormat,
                    width,
                    height);
}

static MMBufferSP getRGB32Data(uint16_t depth,
                    const uint8_t * palette,
                    const uint8_t * colors,
                    uint32_t width,
                    uint32_t height)
{
    MMASSERT(colors != NULL);

    size_t sz = width * height * 4;
    MMBufferSP data = MMBuffer::create(sz);
    if (!data) {
        MMLOGE("no mem\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    data->setActualSize(sz);

    if (depth == 1) {
        MMASSERT(palette != NULL);
        DEC_RETRIEVE_BEGIN_RGB32()
            int k = curY + j / 8;
            uint32_t mixIndex = (colors[k] >> (j  % 8)) & 0x1;
            DEC_RETRIEVE_RGB32(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2],
                                        palette[mixIndex + 3])
        DEC_RETRIEVE_END_RGB32()
    } else if (depth==2) {
        DEC_RETRIEVE_BEGIN_RGB32()
            int k = curY + j / 4;
            uint32_t mixIndex = (colors[k] >> ((j  % 4) * 2)) & 0x3;
            DEC_RETRIEVE_RGB32(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2],
                                        palette[mixIndex + 3])
        DEC_RETRIEVE_END_RGB32()
    } else if (depth == 4) {
        DEC_RETRIEVE_BEGIN_RGB32()
            int k = curY + j / 2;
            uint32_t mixIndex = (colors[k] >> ((j  % 2) * 4)) & 0xf;
            DEC_RETRIEVE_RGB32(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2],
                                        palette[mixIndex + 3])
        DEC_RETRIEVE_END_RGB32()
    } else if (depth == 8) {
        DEC_RETRIEVE_BEGIN_RGB32()
            int k = curY + j;
            uint32_t mixIndex = colors[k];
            DEC_RETRIEVE_RGB32(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2],
                                        palette[mixIndex + 3])
        DEC_RETRIEVE_END_RGB32()
    } else if (depth == 16) {
        DEC_RETRIEVE_BEGIN_RGB32()
            int k = curY + j * 2;
            uint32_t shortTemp = colors[k+1] << 8;
            uint32_t mixIndex = colors[k] + shortTemp;
            DEC_RETRIEVE_RGB32(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2],
                                        palette[mixIndex + 3])
        DEC_RETRIEVE_END_RGB32()
    } else if (depth == 24) {
        MMLOGV("24  color\n");
        DEC_RETRIEVE_BEGIN_RGB32()
            int k = curY + j * 3;
            DEC_RETRIEVE_RGB32(colors[k+0],
                                        colors[k+1],
                                        colors[k+2],
                                        ALPHA_DEF_VAL)
        DEC_RETRIEVE_END_RGB32()
    } else {
        MMLOGV("32 color\n");
        DEC_RETRIEVE_BEGIN_RGB32()
            int k = curY + j * 4;
            DEC_RETRIEVE_RGB32(colors[k+0],
                                        colors[k+1],
                                        colors[k+2],
                                        colors[k+3])
        DEC_RETRIEVE_END_RGB32()
    }

    return data;
}

static MMBufferSP getRGB24Data(uint16_t depth,
                    const uint8_t * palette,
                    const uint8_t * colors,
                    uint32_t width,
                    uint32_t height)
{
    MMASSERT(colors != NULL);

    size_t sz = width * height * 3;
    MMBufferSP data = MMBuffer::create(sz);
    if (!data) {
        MMLOGE("no mem\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    data->setActualSize(sz);

    if (depth == 1) {
        MMASSERT(palette != NULL);
        DEC_RETRIEVE_BEGIN_RGB24()
            int k = curY + j / 8;
            uint32_t mixIndex = (colors[k] >> (j  % 8)) & 0x1;
            DEC_RETRIEVE_RGB24(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB24()
    } else if (depth==2) {
        DEC_RETRIEVE_BEGIN_RGB24()
            int k = curY + j / 4;
            uint32_t mixIndex = (colors[k] >> ((j  % 4) * 2)) & 0x3;
            DEC_RETRIEVE_RGB24(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB24()
    } else if (depth == 4) {
        DEC_RETRIEVE_BEGIN_RGB24()
            int k = curY + j / 2;
            uint32_t mixIndex = (colors[k] >> ((j  % 2) * 4)) & 0xf;
            DEC_RETRIEVE_RGB24(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB24()
    } else if (depth == 8) {
        DEC_RETRIEVE_BEGIN_RGB24()
            int k = curY + j;
            uint32_t mixIndex = colors[k];
            DEC_RETRIEVE_RGB24(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB24()
    } else if (depth == 16) {
        DEC_RETRIEVE_BEGIN_RGB24()
            int k = curY + j * 2;
            uint32_t shortTemp = colors[k+1] << 8;
            uint32_t mixIndex = colors[k] + shortTemp;
            DEC_RETRIEVE_RGB24(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB24()
    } else if (depth == 24) {
        MMLOGV("24  color\n");
        DEC_RETRIEVE_BEGIN_RGB24()
            int k = curY + j * 3;
            DEC_RETRIEVE_RGB24(colors[k+0],
                                        colors[k+1],
                                        colors[k+2])
        DEC_RETRIEVE_END_RGB24()
    } else {
        MMLOGV("32 color\n");
        DEC_RETRIEVE_BEGIN_RGB24()
            int k = curY + j * 4;
            DEC_RETRIEVE_RGB24(colors[k+0],
                                        colors[k+1],
                                        colors[k+2])
        DEC_RETRIEVE_END_RGB24()
    }

    return data;
}

static MMBufferSP getRGB565Data(uint16_t depth,
                    const uint8_t * palette,
                    const uint8_t * colors,
                    uint32_t width,
                    uint32_t height)
{
    MMASSERT(colors != NULL);

    size_t sz = width * height * 2;
    MMBufferSP data = MMBuffer::create(sz);
    if (!data) {
        MMLOGE("no mem\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    data->setActualSize(sz);

    if (depth == 1) {
        MMASSERT(palette != NULL);
        DEC_RETRIEVE_BEGIN_RGB565()
            int k = curY + j / 8;
            uint32_t mixIndex = (colors[k] >> (j  % 8)) & 0x1;
            DEC_RETRIEVE_RGB565(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB565()
    } else if (depth==2) {
        DEC_RETRIEVE_BEGIN_RGB565()
            int k = curY + j / 4;
            uint32_t mixIndex = (colors[k] >> ((j  % 4) * 2)) & 0x3;
            DEC_RETRIEVE_RGB565(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB565()
    } else if (depth == 4) {
        DEC_RETRIEVE_BEGIN_RGB565()
            int k = curY + j / 2;
            uint32_t mixIndex = (colors[k] >> ((j  % 2) * 4)) & 0xf;
            DEC_RETRIEVE_RGB565(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB565()
    } else if (depth == 8) {
        DEC_RETRIEVE_BEGIN_RGB565()
            int k = curY + j;
            uint32_t mixIndex = colors[k];
            DEC_RETRIEVE_RGB565(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB565()
    } else if (depth == 16) {
        DEC_RETRIEVE_BEGIN_RGB565()
            int k = curY + j * 2;
            uint32_t shortTemp = colors[k+1] << 8;
            uint32_t mixIndex = colors[k] + shortTemp;
            DEC_RETRIEVE_RGB565(palette[mixIndex],
                                        palette[mixIndex + 1],
                                        palette[mixIndex + 2])
        DEC_RETRIEVE_END_RGB565()
    } else if (depth == 24) {
        MMLOGV("24  color\n");
        DEC_RETRIEVE_BEGIN_RGB565()
            int k = curY + j * 3;
            DEC_RETRIEVE_RGB565(colors[k+0],
                                        colors[k+1],
                                        colors[k+2])
        DEC_RETRIEVE_END_RGB565()
    } else {
        MMLOGV("32 color\n");
        DEC_RETRIEVE_BEGIN_RGB565()
            int k = curY + j * 4;
            DEC_RETRIEVE_RGB565(colors[k+0],
                                        colors[k+1],
                                        colors[k+2])
        DEC_RETRIEVE_END_RGB565()
    }

    return data;
}


/*static */MMBufferSP BMP::decode(const uint8_t * srcBuf,
                    size_t srcBufSize,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    RGB::Format outFormat,
                    uint32_t & width,
                    uint32_t & height)
{
    if (!srcBuf || scaleNum == 0 || scaleDenom == 0) {
        MMLOGE("invalid param\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    const uint8_t * palette = NULL;
    MMBufferSP data = MMBufferSP((MMBuffer*)NULL);
    const uint8_t * srcp = srcBuf;
    size_t usedSize = 0;

    MMLOGV("scaleNum: %u, scaleDenom: %u, outFormat: %d\n",
        scaleNum, scaleDenom, outFormat);

    size_t headerSize = sizeof(BmpHeader);
    size_t BmpDibheaderSize = sizeof(BmpDibHeader);
    size_t allHdrSize = BmpDibheaderSize + headerSize;
    if (srcBufSize <= allHdrSize) {
        MMLOGE("invalid size\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    BmpHeader * hdr = (BmpHeader*)srcp;
    if ( hdr->magic[0] != 0x42 || hdr->magic[1] != 0x4D ) {
        MMLOGE("not a bmp\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    BmpDibHeader * dib = (BmpDibHeader*)(srcp + headerSize);
    if ( needSwap() ) {
        swapOrder(dib);
    }
    srcp += allHdrSize;
    usedSize += allHdrSize;

    width = dib->width;
    height = dib->height;

    MMLOGV("width: %u, height: %u, num: %u, den: %u\n",
        width,
        height,
        scaleNum,
        scaleDenom);

    if (dib->depth < 24) {
        size_t paletteCount = long(double(dib->depth) * double(dib->depth));
        MMLOGV("Color palette count: %zu\n", paletteCount);
        size_t paletteSize = paletteCount * 4;
        if (srcBufSize <= usedSize + paletteSize) {
            MMLOGE("invalid buffer size\n");
            return MMBufferSP((MMBuffer*)NULL);
        }
        palette = srcp;
        srcp += paletteSize;
        usedSize += paletteSize;
    }

    int l_width = WIDTHBYTES(width * dib->depth);
    size_t nData = height * l_width;
    if (srcBufSize < nData + usedSize) {
        MMLOGE("invalid data area\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    const uint8_t * colors = srcp;

    switch (outFormat) {
        case RGB::kFormatRGB32:
            data = getRGB32Data(dib->depth,
                            palette,
                            colors,
                            width,
                            height);
            break;
        case RGB::kFormatRGB24:
            data = getRGB24Data(dib->depth,
                            palette,
                            colors,
                            width,
                            height);
            break;
        case RGB::kFormatRGB565:
            data = getRGB565Data(dib->depth,
                            palette,
                            colors,
                            width,
                            height);
            break;
        default:
            break;
    }

    if (scaleNum != scaleDenom) {
        MMBufferSP data1;
        if (RGB::zoom(data,
                        outFormat,
                        width,
                        height,
                        scaleNum,
                        scaleDenom,
                        data1,
                        width,
                        height) != MM_ERROR_SUCCESS) {
            MMLOGE("failed to zoom\n");
            return MMBufferSP((MMBuffer*)NULL);
        }
        return data1;
    }

    return data;
}

/*static */mm_status_t BMP::saveAsJpeg(const char * path,
                    uint32_t scaleNum,
                    uint32_t scaleDenom,
                    const char * jpegPath,
                    uint32_t & jpegWidth,
                    uint32_t & jpegHeight)
{
    uint32_t w,h;
    MMBufferSP data = decode(path,
                                        scaleNum,
                                        scaleDenom,
                                        RGB::kFormatRGB32,
                                        w,
                                        h);
    if (!data) {
        MMLOGE("failed to decode\n");
        return MM_ERROR_OP_FAILED;
    }

    return RGB::saveAsJpeg(data,
                        RGB::kFormatRGB32,
                        w,
                        h,
                        1,
                        1,
                        true,
                        jpegPath,
                        jpegWidth,
                        jpegHeight);
}


/*static */mm_status_t BMP::parseDimension(const uint8_t * srcBuf,
                    size_t srcBufSize,
                    uint32_t & width,
                    uint32_t & height)
{
    if (!srcBuf) {
        MMLOGE("invalid param\n");
        return MM_ERROR_INVALID_PARAM;
    }

    if (srcBufSize < sizeof(BmpHeader) + sizeof(BmpDibHeader)) {
        MMLOGE("buffer is too short\n");
        return MM_ERROR_INVALID_PARAM;
    }

    BmpDibHeader * dib = (BmpDibHeader*)(srcBuf + sizeof(BmpHeader));
    if ( needSwap() ) {
        swapOrder(dib);
    }

    width = dib->width;
    height = dib->height;

    return MM_ERROR_SUCCESS;
}

/*static */mm_status_t BMP::parseDimension(const MMBufferSP & srcBuf,
                    uint32_t & width,
                    uint32_t & height)
{
    return parseDimension(srcBuf->getBuffer(), srcBuf->getActualSize(), width, height);
}

/*static */mm_status_t BMP::parseDimension(const char * path,
                    uint32_t & width,
                    uint32_t & height)
{
    FILE * fp = NULL;
    if (!path) {
        MMLOGE("invalid args\n");
        return MM_ERROR_INVALID_PARAM;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        MMLOGE("failed to open file %s\n", path);
        return MM_ERROR_IO;
    }

    size_t reqSize = sizeof(BmpHeader) + sizeof(BmpDibHeader);
    MMBufferSP buf;
    buf = MMBuffer::create(reqSize);
    if (!buf) {
        MMLOGE("failed to create buffer\n");
        fclose(fp);
        return MM_ERROR_NO_MEM;
    }
    uint8_t * fBuf = buf->getWritableBuffer();

    size_t ret = fread(fBuf, 1, reqSize, fp);
    fclose(fp);
    if ( ret < reqSize ) {
        MMLOGE("file is too small\n");
        return MM_ERROR_INVALID_PARAM;
    }
    buf->setActualSize(ret);

    return parseDimension(buf, width, height);
}

}
