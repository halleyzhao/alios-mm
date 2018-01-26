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

#include <errno.h>

#include <setjmp.h>

#include "regiondecoder_jpg.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>


namespace YUNOS_MM {

MM_LOG_DEFINE_MODULE_NAME("REGION_DEC")

struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */

    jmp_buf setjmp_buffer;        /* for return to caller */
};

static void my_error_exit(j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    struct my_error_mgr * myerr = (struct my_error_mgr *) cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    (*cinfo->err->output_message) (cinfo);
    MMLOGE("error occured,msgcode[%d]\n", (cinfo)->err->msg_code);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}


static void initSource(j_decompress_ptr cinfo);
static boolean fillInputBuffer(j_decompress_ptr cinfo);
static void skipInputData(j_decompress_ptr cinfo, long num_bytes);
static void termSource(j_decompress_ptr);
static boolean seekInputData(j_decompress_ptr cinfo, long byte_offset);

DEFINE_LOGTAG(JPEGRegionDecoder::SourceManager)

JPEGRegionDecoder::SourceManager::SourceManager(FILE * f) : mFile(f)
{
    MMLOGV("+\n");
    MMASSERT(f != NULL);
    init_source = initSource;
    fill_input_buffer = fillInputBuffer;
    skip_input_data = skipInputData;
    resync_to_restart = jpeg_resync_to_restart;
    term_source = termSource;
    seek_input_data = seekInputData;
    MMLOGV("-\n");
}
JPEGRegionDecoder::SourceManager::~SourceManager(){}

static void initSource(j_decompress_ptr cinfo)
{
    MMLOGV("+\n");
    if (!cinfo) {
        MMLOGE("invalid params\n");
        return;
    }
    JPEGRegionDecoder::SourceManager * src = (JPEGRegionDecoder::SourceManager*)cinfo->src;
    src->next_input_byte = (const JOCTET*)src->mBuffer;
    src->bytes_in_buffer = 0;
    src->current_offset = 0;
    rewind(src->mFile);
    MMLOGV("-\n");
}

static boolean fillInputBuffer(j_decompress_ptr cinfo)
{
//    MMLOGV("+\n");
    if (!cinfo) {
        MMLOGE("invalid params\n");
        return FALSE;
    }

    JPEGRegionDecoder::SourceManager * src = static_cast<JPEGRegionDecoder::SourceManager*>(cinfo->src);
    if (!src) {
        MMLOGE("invalid src\n");
        return FALSE;
    }

    size_t bytes = fread(src->mBuffer, 1, JPEGRegionDecoder::SourceManager::kBufferSize, src->mFile);
    if (bytes == 0) {
        MMLOGE("read ret 0\n");
        return FALSE;
    }

    src->current_offset += bytes;
    src->next_input_byte = (const JOCTET*)src->mBuffer;
    src->bytes_in_buffer = bytes;
//    MMLOGV("current_offset: %zu, bytes: %zu\n", src->current_offset, bytes);
//    MMLOGV("-\n");
    return TRUE;
}

static void skipInputData(j_decompress_ptr cinfo, long num_bytes)
{
//    MMLOGV("num_bytes: %ld\n", num_bytes);
    if (!cinfo) {
        MMLOGE("invalid params\n");
        return;
    }

    JPEGRegionDecoder::SourceManager * src = static_cast<JPEGRegionDecoder::SourceManager*>(cinfo->src);
    if (!src) {
        MMLOGE("invalid src\n");
        return;
    }

    if (num_bytes <= (long)src->bytes_in_buffer) {
        src->next_input_byte += num_bytes;
        src->bytes_in_buffer -= num_bytes;
//        MMLOGV("reamin enough, direct move\n");
        return;
    }

    long toSkip = num_bytes - src->bytes_in_buffer;
//    MMLOGV("skip file: %ld\n", toSkip);
    if (fseek(src->mFile, toSkip, SEEK_CUR)) {
//        MMLOGE("failed seek by %ld, err: %s", toSkip, strerror(errno));
        cinfo->err->error_exit((j_common_ptr)cinfo);
        return;
    }

    src->current_offset += toSkip;
    src->next_input_byte = (const JOCTET*)src->mBuffer;
    src->bytes_in_buffer = 0;
//    MMLOGV("current_offset: %zu\n", src->current_offset);
}

static void termSource(j_decompress_ptr) {}

static boolean seekInputData(j_decompress_ptr cinfo, long byte_offset)
{
//    MMLOGV("byte_offset: %ld\n", byte_offset);
    if (!cinfo) {
        MMLOGE("invalid params\n");
        return FALSE;
    }

    JPEGRegionDecoder::SourceManager * src = static_cast<JPEGRegionDecoder::SourceManager*>(cinfo->src);
    if (!src) {
        MMLOGE("invalid src\n");
        return FALSE;
    }

    long sk;
    if ((size_t)byte_offset > src->current_offset) {
        sk = byte_offset - (long)src->current_offset;
//        MMLOGV("seek to large: %ld\n", sk);
    } else {
        rewind(src->mFile);
        sk = byte_offset;
//        MMLOGV("seek to small: %ld\n", sk);
    }

    if (fseek(src->mFile, sk, SEEK_CUR)) {
        MMLOGE("failed seek by %ld, err: %s", byte_offset - (long)src->current_offset, strerror(errno));
        cinfo->err->error_exit((j_common_ptr)cinfo);
        return FALSE;
    }

    src->current_offset = (size_t)byte_offset;
//    MMLOGV("current_offset: %zu\n", src->current_offset);
    src->next_input_byte = (const JOCTET*)src->mBuffer;
    src->bytes_in_buffer = 0;
//    MMLOGV("-\n");
    return TRUE;
}



DEFINE_LOGTAG(JPEGRegionDecoder)

JPEGRegionDecoder::JPEGRegionDecoder()
                                        : mFile(NULL),
                                        mCInfoCreated(false),
                                        mHuffmanIndexCreated(false),
                                        mSourceManager(NULL)
{
    MMLOGI("+\n");
    mCInfo = {0};
    mHuffmanIndex = {0};
    MMLOGV("-\n");
}

JPEGRegionDecoder::~JPEGRegionDecoder()
{
    MMLOGI("+\n");
    clean();
    MMLOGV("-\n");
}

void JPEGRegionDecoder::clean()
{
    MMLOGI("+\n");
    destroyHuffman();
    destroyInfo();
}

#define JPG_ERR_HDLR(_retCode)\
    MMLOGV("setting error hdlr\n");\
    struct my_error_mgr jerr;\
    mCInfo.err = jpeg_std_error(&jerr.pub);\
    jerr.pub.error_exit = my_error_exit;\
    if (setjmp(jerr.setjmp_buffer)) {\
        clean();\
        return _retCode;\
    }

bool JPEGRegionDecoder::initInfo(const char * path)
{
    MMLOGI("+\n");
    if (mCInfoCreated || mSourceManager || mFile) {
        MMLOGE("already in use, please call unprepare first\n");
        return false;
    }

    if ((mFile = fopen(path, "rb")) == NULL) {
        MMLOGE("can't open %s\n", path);
        return false;
    }

    try {
        mSourceManager = new SourceManager(mFile);
    } catch (...) {
        MMLOGE("no mem\n");
        fclose(mFile);
        mFile = NULL;
        return false;
    }

    MMLOGV("creating cinfo\n");
    jpeg_create_decompress(&mCInfo);
    mCInfoCreated = true;

    JPG_ERR_HDLR(false)

    mCInfo.src = mSourceManager;

    MMLOGV("read header\n");
    if (jpeg_read_header(&mCInfo, true) != JPEG_HEADER_OK) {
        MMLOGE("failed to read header\n");
        jpeg_destroy_decompress(&mCInfo);
        mCInfoCreated = false;
        MM_RELEASE(mSourceManager);
        fclose(mFile);
        mFile = NULL;
        return false;
    }

    MMLOGV("success\n");
    return true;
}

void JPEGRegionDecoder::destroyInfo()
{
    MMLOGV("destroy cinfo\n");
    if (mCInfoCreated) {
        jpeg_destroy_decompress(&mCInfo);
        mCInfoCreated = false;
    }
    if (mFile) {
        fclose(mFile);
        mFile = NULL;
    }
    MM_RELEASE(mSourceManager);
}

bool JPEGRegionDecoder::buildHuffman()
{
    MMASSERT(!mHuffmanIndexCreated);
    JPG_ERR_HDLR(false);

    MMLOGV("create huffman\n");
    jpeg_create_huffman_index(&mCInfo, &mHuffmanIndex);

    MMLOGV("build huffman\n");
    if (!(mHuffmanIndexCreated = jpeg_build_huffman_index(&mCInfo, &mHuffmanIndex))) {
        MMLOGE("failed to build huffman index\n");
        jpeg_destroy_huffman_index(&mHuffmanIndex);
        mHuffmanIndexCreated = false;
        return false;
    }

    return true;
}

void JPEGRegionDecoder::destroyHuffman()
{
    if (mHuffmanIndexCreated) {
        MMLOGV("jpeg_destroy_huffman_index\n");
        jpeg_destroy_huffman_index(&mHuffmanIndex);
        mHuffmanIndexCreated = false;
    }
}

bool JPEGRegionDecoder::prepare(const char * path, uint32_t & width, uint32_t & height)
{
    MMLOGI("+\n");
    if (!initInfo(path)) {
        clean();
        return false;
    }

    if (!buildHuffman()) {
        clean();
        return false;
    }

    destroyInfo();

    if (!initInfo(path)) {
        clean();
        return false;
    }

    MMLOGV("-\n");
    return true;
}

void JPEGRegionDecoder::unprepare()
{
    MMLOGV("+\n");
    clean();
    MMLOGV("-\n");
}

MMBufferSP JPEGRegionDecoder::decode(RGB::Format outFormat,
                        uint32_t startX,
                        uint32_t startY,
                        uint32_t width,
                        uint32_t height)
{
    MMLOGI("outFormat: %d, startX: %u, startY: %u, width: %u, height: %u\n",
            outFormat, startX, startY, width, height);
    if (!mCInfoCreated) {
        MMLOGE("call prepare first\n");
        return MMBufferSP((MMBuffer*)NULL);
    }

    mCInfo.do_fancy_upsampling = 0;
    mCInfo.do_block_smoothing = 0;

    switch (outFormat) {
        case RGB::kFormatRGB32:
            mCInfo.out_color_space = JCS_EXT_RGBX;
            break;
        case RGB::kFormatRGB24:
        case RGB::kFormatRGB565:
            mCInfo.out_color_space = JCS_EXT_RGB;
            break;
        default:
            MMLOGE("invalid outformat: %d\n", outFormat);
            return MMBufferSP((MMBuffer*)NULL);
    }

//    MMLOGV("starting tile decompress\n");
    if (!jpeg_start_tile_decompress(&mCInfo)) {
        MMLOGE("failed to start tile decompress\n");
        return MMBufferSP((MMBuffer*)NULL);
    }
    MMLOGV("image_width: %d, image_height: %d, output_width: %d, output_height: %d, output_components: %d, out_color_space: %d\n",
        mCInfo.image_width, mCInfo.image_height,
        mCInfo.output_width, mCInfo.output_height,
        mCInfo.output_components, mCInfo.out_color_space);

    uint32_t srcWidth = mCInfo.output_width;
    uint32_t srcHeight = mCInfo.output_height;

    if (startX >= srcWidth || startY >= srcHeight || startX + width > srcWidth || startY + height > srcHeight) {
        MMLOGI("invalid param: outFormat: %d, startX: %u, startY: %u, width: %u, height: %u\n",
                outFormat, startX, startY, width, height);
        return MMBufferSP((MMBuffer*)NULL);
    }

    JPG_ERR_HDLR(MMBufferSP((MMBuffer*)NULL));

//    MMLOGV("init read tile scanline\n");
    uint32_t expectStartX = startX;
    uint32_t expectStartY = startY;
    uint32_t expectWidth = width;
    uint32_t expectHeight = height;

    jpeg_init_read_tile_scanline(&mCInfo, &mHuffmanIndex, (int*)&startX, (int*)&startY, (int*)&width, (int*)&height);
    MMLOGV("after init read tile scanline, startX: %u, startY: %u, width: %u, height: %u\n", startX, startY, width, height);
    if (expectWidth > width || expectHeight > height || expectStartX < startX || expectStartY < startY) {
        MMLOGE("invalid param. expectStartX: %u, expectStartY: %u, expectWidth: %u, expectHeight: %u, startX: %u, startY: %u, width: %u, height: %u",
            expectStartX, expectStartY, expectWidth, expectHeight, startX, startY, width, height);
        jpeg_finish_decompress(&mCInfo);
        return MMBufferSP((MMBuffer*)NULL);
    }

    size_t rowSize = width * mCInfo.output_components;
    size_t bufSize = rowSize * height;
    MMLOGV("bufSize: %zu, rowSize: %zu\n", bufSize, rowSize);
    MMBufferSP buf = MMBuffer::create(bufSize);
    if (!buf) {
        MMLOGE("no mem, need: %zu\n", bufSize);
        jpeg_finish_decompress(&mCInfo);
        return buf;
    }

    uint32_t readedLines = 0;
    JSAMPLE* rowptr = (JSAMPLE*)buf->getWritableBuffer();
//    MMLOGV("rowptr: %p\n", rowptr);
    uint32_t curLineIndex = startY;
    uint32_t dx;
    if (expectStartX > startX) {
        dx = (expectStartX - startX) * mCInfo.output_components;
    } else {
        dx = 0;
    }
    uint32_t expectedRowSize = expectWidth * mCInfo.output_components;
    while (readedLines < height) {
//        MMLOGV("jpeg_read_tile_scanline, readedLines: %u, height: %u\n", readedLines, height);
        int cur = jpeg_read_tile_scanline(&mCInfo, &mHuffmanIndex, &rowptr);
        if (cur == 0) {
            MMLOGE("read scanline failed\n");
            jpeg_finish_decompress(&mCInfo);
            return MMBufferSP((MMBuffer*)NULL);
        }
        readedLines += cur;
//        MMLOGV("curline: %u, readed line: %d\n", cur, readedLines);
        if (curLineIndex++ < expectStartY) {
//            MMLOGV("skip line: %u\n", curLineIndex);
            continue;
        }

        if (expectStartX > startX) {
//            MMLOGV("move: dx: %u, size: %u\n", dx, expectedRowSize);
            memmove(rowptr, rowptr + dx, expectedRowSize);
            rowptr += expectedRowSize;
        } else {
            rowptr += rowSize;
        }
//        MMLOGV("rowptr: %p\n", rowptr);
    }

    buf->setActualSize(expectWidth * expectHeight);
    MMLOGV("dec over\n");

    jpeg_finish_decompress(&mCInfo);

    if (outFormat == RGB::kFormatRGB565) {
        MMLOGV("convert to 565\n");
        return RGB::convertFormat(RGB::kFormatRGB24,
                                    RGB::kFormatRGB565,
                                    expectWidth,
                                    expectHeight,
                                    buf);
    }

    return buf;
}

}
