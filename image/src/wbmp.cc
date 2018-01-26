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

#include <multimedia/wbmp.h>
#include "utils.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

MM_LOG_DEFINE_MODULE_NAME("WBMP")
DEFINE_LOGTAG(WBMP)

static size_t getuintvar(const uint8_t * buf, size_t size, uint32_t & var)
{
    MMASSERT(buf != NULL);
    MMLOGV("size: %zu\n", size);
    size_t consumed = 0;
    const uint8_t * p = buf;
    var = 0;
    while (consumed < size) {
        ++consumed;
        if ((*p) & 0x80) {
            var = (var << 7) | ((*p) & 0x7f);
            ++p;
            continue;
        }
        
        var = (var << 7) | (*p);
        break;
    }

    if (consumed == size) {
        MMLOGE("no latest\n");
        return 0;
    }

    return consumed;
}

static void decodecOneByte(uint8_t byte, int want, RGB::RGB32 * p)
{
    //MMLOGV("byte: 0x%x, want: %d\n", byte, want);
    MMASSERT(want <= 8);
    uint8_t mask = 0x80;
    for (int i = 0; i < want; ++i) {
        p->alpha = ALPHA_DEF_VAL;
        if (mask & byte) {
            p->blue = p->green = p->red = 255;
        } else {
            p->blue = p->green = p->red = 0;
        }
        ++p;

        mask >>= 1;
    }
}

static MMBufferSP decodePixels(const uint8_t * buf,
                    uint32_t width,
                    uint32_t height)
{
    MMLOGV("width: %d, height: %d\n", width, height);
    MMBufferSP data;
    MMASSERT(buf != NULL);

    size_t bufSize = width * height * sizeof(RGB::RGB32);
    data = MMBuffer::create(bufSize);
    if (!data) {
        MMLOGE("no mem\n");
        return data;
    }

    RGB::RGB32 * p = (RGB::RGB32*)data->getWritableBuffer() + width * (height - 1);
    const uint8_t * q = buf;
    int wTo = width >> 3;
    int addi = width % 8;
    for (int i = (int)height - 1; i >= 0; --i) {
        //MMLOGV("decoding: %d\n", i);
        int j;
        p = (RGB::RGB32*)data->getWritableBuffer() + width * i;
        for (j = 0; j < wTo; ++j) {
            decodecOneByte(*q, 8, p);
            p += 8;
            ++q;
        }

        //MMLOGV("decoding addi: %d\n", i);
        if (addi > 0) {
            decodecOneByte(*q, addi, p);
            ++q;
            //p += addi;
        }
    }

    data->setActualSize(bufSize);
    return data;
}


/*static */MMBufferSP WBMP::decode(const char * path,
                    RGB::Format outFormat,
                    uint32_t & width,
                    uint32_t & height)
{
    MMBufferSP data = MMBufferSP((MMBuffer*)NULL);
    FILE * fp = NULL;

    if (!path) {
        MMLOGE("invalid args\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    MMLOGV("path: %s, outFormat: %d\n",
        path, outFormat);

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
                    outFormat,
                    width,
                    height);
}

/*static */MMBufferSP WBMP::decode(const uint8_t * buf,
                    size_t bufSize,
                    RGB::Format outFormat,
                    uint32_t & width,
                    uint32_t & height)
{
    if (!buf) {
        MMLOGE("invalid args\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    MMBufferSP data = MMBufferSP((MMBuffer*)NULL);

    size_t used = 0;
    const uint8_t * p = buf;
    if (p[0] != 0 || p[1] != 0) {
        MMLOGE("unsupported file format. 0: 0x%x, 1: 0x%x\n", p[0], p[1]);
        return MMBufferSP((MMBuffer*)NULL);
    }

    used += 2;
    p += 2;
    if (used + 2 >= bufSize) {
        MMLOGE("buffer is too small\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    size_t consumed = getuintvar(p, bufSize - used, width);
    if (consumed == 0) {
        MMLOGE("invalid buffer for width\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    used += consumed;
    p += consumed;

    if (used + 2 >= bufSize) {
        MMLOGE("buffer is too small\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    consumed = getuintvar(p, bufSize - used, height);
    if (consumed == 0) {
        MMLOGE("invalid file for height\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    used += consumed;
    p += consumed;

    MMLOGV("width: %u, height: %u\n", width, height);
    if (width == 0 || height == 0) {
        MMLOGE("invalid file for size\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    size_t wBytes = (width & 0x7) ? ((width >> 3) + 1) : (width >> 3);
    size_t bufNeed = wBytes * height;
    MMLOGV("wBytes: %zu, bufNeed: %zu\n", wBytes, bufNeed);
    data = decodePixels(p, width, height);
    if (outFormat != RGB::kFormatRGB32) {
        return RGB::convertFormat(RGB::kFormatRGB32,
                                outFormat,
                                width,
                                height,
                                data);
    }
    return data;
}

/*static */MMBufferSP WBMP::decode(const MMBufferSP & buf,
                    RGB::Format outFormat,
                    uint32_t & width,
                    uint32_t & height)
{
    if (!buf) {
        MMLOGE("invalid args\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    return decode(buf->getBuffer(),
                        buf->getActualSize(),
                        outFormat,
                        width,
                        height);
}


/*static */mm_status_t WBMP::saveAsJpeg(const char * path,
                    const char * jpegPath,
                    uint32_t & jpegWidth,
                    uint32_t & jpegHeight)
{
    uint32_t w,h;
    MMBufferSP data = decode(path,
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



}
