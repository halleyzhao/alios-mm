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
#include <string.h>

#include <pthread.h>
#include <sys/time.h>
#include <ctype.h>
#include <math.h>
#include <glib.h>
#include <gtest/gtest.h>

#include <OMX_Video.h>
#include <MetadataBufferType.h>

#include "multimedia/mm_debug.h"
#include "multimedia/media_meta.h"
#include "multimedia/media_attr_str.h"
#include "mmwakelocker.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avtime.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
#ifdef __cplusplus
}
#endif

//#include "graphic_buffer.h"
#include "media_codec.h"

#include "native_surface_help.h"

MM_LOG_DEFINE_MODULE_NAME("MCRECORDERTEST")

using namespace YunOSMediaCodec;
#define USE_FFMPEG_ENCODER 1
#define NO_ERROR 0
#define BAD_VALUE -1

static const uint32_t kMinBitRate = 100000;         // 0.1Mbps
static const uint32_t kMaxBitRate = 100 * 1000000;  // 100Mbps
static const uint32_t kMaxTimeLimitSec = 10;       // 3 minutes

// Command-line parameters.
static bool gRotate = false;                // rotate 90 degrees
static int32_t gVideoWidth = 1280;            // default width+height
static int32_t gVideoHeight = 720;
static int32_t gBitRate = 4000000;         // 4Mbps
static int32_t gTimeLimitSec = kMaxTimeLimitSec;

// Set by signal handler to stop recording.
static MediaCodec *g_videoEncoder = NULL;
static int g_encorderFrameCnt = 0;
static uint32_t g_sourceFormat = 0;

#define STREAM_FRAME_RATE 25 /* 25 images/s */

using namespace YUNOS_MM;
static Lock mMsgThrdLock;
static Condition *mMsgCond = NULL;
static bool gExit = false;

#if !USE_FFMPEG_ENCODER
static FILE *g_fp = NULL;
#endif

typedef enum {
    VideoEncoderOneFrame = 0,
    VideoEncoderTryAgain,
    VideoEncoderIgnore,
    VideoEncoderUnknown,
    VideoEncoderEOS = 100,
} VideoEncoderResult;

typedef enum {
    OWNED_BY_US,
    OWNED_BY_COMPONENT,
    OWNED_BY_NATIVEWINDOW,
} VideoBufferStatePriv;
#if 0
static const char*sBufferState[] =
{
    "OWNED_BY_US",
    "OWNED_BY_COMPONENT",
    "OWNED_BY_NATIVEWINDOW"
};
#endif

// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
    AVFormatContext *oc;
    char *data;
} OutputStream;


struct BufferInfoPriv {
    MMNativeBuffer *mANB;
    VideoBufferStatePriv mState;
    uint32_t mCount;
};


static int32_t gCountTick = 0;
static int gInputBufferCount = -1;
static WindowSurface *gNativeWindow = NULL;
static std::vector<BufferInfoPriv> gBufferInfoVec;
static std::vector<MMNativeBuffer *>gBufferTrack;
static std::list<MMNativeBuffer *>gANBSrcBufferList;

static MediaMetaSP createVideoFormatForEncorder() {
    MediaMetaSP format = MediaMeta::create();

    format->setString(MEDIA_ATTR_MIME, MEDIA_MIMETYPE_VIDEO_AVC);
    format->setInt32(MEDIA_ATTR_WIDTH, gVideoWidth);
    format->setInt32(MEDIA_ATTR_HEIGHT, gVideoHeight);

    format->setInt32(MEDIA_ATTR_STORE_META_INPUT, 1);
    format->setInt32(MEDIA_ATTR_STORE_META_OUTPUT, 0);
    format->setInt32(MEDIA_ATTR_PREPEND_SPS_PPS, 0);
    format->setInt32(MEDIA_ATTR_COLOR_FORMAT, 0x7f000789);
    format->setInt64(MEDIA_ATTR_REPEAT_FRAME_DELAY_US, -1);
    format->setInt32(MEDIA_ATTR_BIT_RATE, gBitRate);
    format->setInt32(MEDIA_ATTR_STRIDE, gVideoWidth);
    format->setInt32(MEDIA_ATTR_SLICE_HEIGHT, gVideoHeight);
    format->setInt32(MEDIA_ATTR_BITRATE_MODE, (int32_t)OMX_Video_ControlRateVariable);
    format->setFloat(MEDIA_ATTR_FRAME_RATE, 30.0f);

    format->setInt32(MEDIA_ATTR_I_FRAME_INTERVAL, 10);
    format->setInt32(MEDIA_ATTR_AVC_PROFILE, (int32_t)OMX_VIDEO_AVCProfileBaseline);
    format->setInt32(MEDIA_ATTR_AVC_LEVEL, (int32_t)OMX_VIDEO_AVCLevel4);

    PRINTF("videoFormat created \n");
    return format;
}

#if 0
static int loadYUVDataAutoGenerator(int width, int height,
        uint8_t *Y_start, int Y_pitch,
        uint8_t *U_start, int U_pitch,
        uint8_t *V_start, int V_pitch,
        int UV_interleave) {

    static int row_shift = 0;
    int block_width = width/256*8; // width/32 /8 * 8; 32 blocks each row, and make 8 alignment.
    int row;
    //int alpha = 0;

    if (block_width<8)
        block_width = 8;

    /* copy Y plane */
    for (row=0;row<height;row++) {
        uint8_t *Y_row = Y_start + row * Y_pitch;
        int jj, xpos, ypos;

        ypos = (row / block_width) & 0x1;

        for (jj=0; jj<width; jj++) {
            xpos = ((row_shift + jj) / block_width) & 0x1;
            if (xpos == ypos)
                Y_row[jj] = 0xeb;
            else
                Y_row[jj] = 0x10;
        }
    }

    /* copy UV data */
    for( row =0; row < height/2; row++) {
        if (UV_interleave) {
            uint8_t *UV_row = U_start + row * U_pitch;
            memset (UV_row,0x80,width);
        } else {
            uint8_t *U_row = U_start + row * U_pitch;
            uint8_t *V_row = V_start + row * V_pitch;

            memset (U_row,0x80,width/2);
            memset (V_row,0x80,width/2);
        }
    }

    row_shift += (block_width/8);   // minimual block_width is 8, it is safe for increasement

    /*
    if (getenv("AUTO_UV") == NULL)
        EXIT_AND_RETURN(0);

    if (getenv("AUTO_ALPHA"))
        alpha = 0;
    else
        alpha = 70;

    YUV_blend_with_pic(width,height,
            Y_start, Y_pitch,
            U_start, U_pitch,
            V_start, V_pitch,
            UV_interleave, alpha);
    */
    return 0;
}
#else
/* static */ int loadYUVDataAutoGenerator(void* ptr, int format, int width, int height, int x_stride, int y_stride)
{
    static int row_shift = 0;
    int block_width = width/256*8; // width/32 /8 * 8; 32 blocks each row, and make 8 alignment.
    int row;
    //int alpha = 0;

    uint8_t *y_start = NULL;
    uint8_t *u_start = NULL;
    uint8_t *v_start = NULL;
    int y_pitch = 0;
    int u_pitch = 0;
    int v_pitch = 0;

    y_start = (uint8_t *)ptr;
    y_pitch = x_stride;

    if (block_width<8)
        block_width = 8;

    /* copy Y plane */
    for (row=0;row<height;row++) {
        uint8_t *Y_row = y_start + row * y_pitch;
        int jj, xpos, ypos;

        ypos = (row / block_width) & 0x1;

        for (jj=0; jj<width; jj++) {
            xpos = ((row_shift + jj) / block_width) & 0x1;
            if (xpos == ypos)
                Y_row[jj] = 0xeb;
            else
                Y_row[jj] = 0x10;
        }
    }

    u_start = v_start = (uint8_t *)ptr + y_stride * x_stride;
    u_pitch = v_pitch = x_stride;

    for( row =0; row < height/2; row++) {
        /* copy UV data */
        uint8_t *UV_row = u_start + row * u_pitch;
        memset (UV_row,0x80,width);
    }


    /*
    if (getenv("AUTO_UV") == NULL)
        EXIT_AND_RETURN(0);

    if (getenv("AUTO_ALPHA"))
        alpha = 0;
    else
        alpha = 70;

    YUV_blend_with_pic(width,height,
            Y_start, Y_pitch,
            U_start, U_pitch,
            V_start, V_pitch,
            UV_interleave, alpha);
    */
    return 0;
}

#endif

static bool fillInputBuffer(MMNativeBuffer *anb) {
    void *ptr = NULL;
    uint32_t h_stride = gVideoWidth;
    uint32_t v_stride = gVideoHeight;

    Rect rect;
    rect.top = 0;
    rect.left = 0;
    rect.right = h_stride;
    rect.bottom = h_stride;


#if defined(__MM_YUNOS_CNTRHAL_BUILD__)
    uint32_t usage = ALLOC_USAGE_PREF(SW_READ_OFTEN) | ALLOC_USAGE_PREF(SW_WRITE_OFTEN);

    #ifndef YUNOS_ENABLE_UNIFIED_SURFACE
    yunos::libgui::mapBuffer(anb, usage, rect, &ptr);
    #else
    YunAllocator &allocator(YunAllocator::get());
    NativeWindowBuffer *nativeBuffer = static_cast<NativeWindowBuffer*>(anb);
    allocator.lock(nativeBuffer->target,
        ALLOC_USAGE_PREF(SW_READ_OFTEN),
        rect, &ptr);
    #endif

#endif
    DEBUG("ptr: %p, gVideoWidth: %d, h_stride: %d, gVideoHeight: %d\n", ptr, gVideoWidth, h_stride, gVideoHeight);

    uint8_t *y_start, *u_start, *v_start;
    int y_pitch, u_pitch, v_pitch;

    y_start = (uint8_t *)ptr;
    u_start = (uint8_t *)ptr + h_stride * v_stride;
    v_start = u_start + h_stride * v_stride / 4;
    y_pitch = h_stride;
    u_pitch = h_stride/2;
    v_pitch = h_stride/2;

#if 0
    loadYUVDataAutoGenerator(gVideoWidth, gVideoHeight,
                         y_start, y_pitch,
                         u_start, u_pitch,
                         v_start, v_pitch, false);
#else
    loadYUVDataAutoGenerator(ptr, g_sourceFormat, gVideoWidth, gVideoHeight, gVideoWidth, gVideoHeight);
#endif

#if defined(__MM_YUNOS_CNTRHAL_BUILD__)
    #ifndef YUNOS_ENABLE_UNIFIED_SURFACE
    yunos::libgui::unmapBuffer(anb);
    #else
    allocator.unlock(nativeBuffer->target);
    #endif
#endif
    return true;
}


static int initWindowForEncoder() {
    int err = -1;
    gNativeWindow = createSimpleSurface(gVideoWidth, gVideoHeight);
    if (!gNativeWindow)
        return err;

    VERBOSE("WINDOW_API(set_buffers_geometry)\n");
    err = WINDOW_API(set_buffers_dimensions)(
              GET_ANATIVEWINDOW(gNativeWindow),
              gVideoWidth,
              gVideoHeight);
    if (err != 0) {
        PRINTF("WINDOW_API(set_buffers_dimensions) failed: (%d)\n", err);
        return -1;
    }

    g_sourceFormat = mm_getVendorDefaultFormat(false);
    #if defined(YUNOS_BOARD_sprd)
    g_sourceFormat = 0x11; //nv21
    #endif
    err = WINDOW_API(set_buffers_format)(
              GET_ANATIVEWINDOW(gNativeWindow),
              g_sourceFormat);
    if (err != 0) {
        PRINTF("WINDOW_API(set_buffers_format) failed: (%d)\n", err);
        return -1;
    }
    INFO("WINDOW_API(set_buffers_format) done, g_sourceFormat 0x%0x\n", g_sourceFormat);

#if defined(__MM_YUNOS_CNTRHAL_BUILD__)
    #if defined(__PHONE_BOARD_QCOM__)
        err = mm_setSurfaceUsage (gNativeWindow, ALLOC_USAGE_PREF(HW_CAMERA_WRITE));
    #elif defined(YUNOS_BOARD_sprd)
        err = mm_setSurfaceUsage (gNativeWindow, ALLOC_USAGE_PREF(HW_VIDEO_ENCODER));
    #endif
#endif
    if (err != 0) {
        PRINTF("WINDOW_API(set_usage) failed: (%d)\n", err);
        return -1;
    }
    INFO("WINDOW_API(set_usage) done\n");


    err = WINDOW_API(set_buffer_count)(
              GET_ANATIVEWINDOW(gNativeWindow), gInputBufferCount);
    if (err != 0) {
        PRINTF("WINDOW_API(set_buffer_count) failed: (%d)\n", err);
        return -1;
    }
    VERBOSE("WINDOW_API(set_buffer_count) done\n");

    gBufferTrack.reserve(gInputBufferCount);
    for (int i = 0; i < gInputBufferCount; i++) {
        MMNativeBuffer *buf = NULL;
        err = mm_dequeueBufferWait(gNativeWindow, &buf);
        if (err != NO_ERROR) {
            PRINTF("WINDOW_API(dequeue_dequeueBuffer) failed: (%d)\n", err);
            return -1;
        }

       // size_t in_size = 0;
        if (!fillInputBuffer(buf)) {
            return -1;
        }

        BufferInfoPriv bufferInfo;
        bufferInfo.mANB = buf;
        bufferInfo.mState = OWNED_BY_US;
        bufferInfo.mCount = 0;
        gBufferInfoVec.push_back(bufferInfo);

        gBufferTrack[i] = NULL;

        INFO("graphic w h stride format usage handle are %d %d %d 0x%x 0x%x %p\n",
             buf->width,
             buf->height,
             buf->stride,
             buf->format,
#ifndef YUNOS_ENABLE_UNIFIED_SURFACE
             buf->usage,
             buf->handle);
#else
             buf->flags,
             buf->target);
#endif

    }

    //need cancel 2 buffers
    err = mm_cancelBuffer(gNativeWindow,  gBufferInfoVec[0].mANB, -1);

    err |= mm_cancelBuffer(gNativeWindow, gBufferInfoVec[1].mANB, -1);

    if (err != 0) {
        WARNING("can not return buffer to native window");
        return -1;
    }

    gBufferInfoVec[0].mState = OWNED_BY_NATIVEWINDOW;
    gBufferTrack[0] = NULL;
    gBufferInfoVec[1].mState = OWNED_BY_NATIVEWINDOW;
    gBufferTrack[1] = NULL;

    //gBufferTrack.reserve(gInputBufferCount);

    return 0;
}
#if 0
static buffer_handle_t getMediaBufferHandle(uint8_t *buffer) {
    // need to convert to char* for pointer arithmetic and then
    // copy the byte stream into our handle
    buffer_handle_t bufferHandle;
    memcpy(&bufferHandle, buffer + 4, sizeof(buffer_handle_t));
    return bufferHandle;
}
#endif
static void passMetadataBuffer(uint8_t *buffer,
                               buffer_handle_t bufferHandle) {
    OMX_U32 type = kMetadataBufferTypeGrallocSource;
    memcpy(buffer, &type, 4);
    memcpy(buffer + 4, &bufferHandle, sizeof(buffer_handle_t));

}
#if 0
static void dumpBufferInfo() {
    for (int i = 0; i < gInputBufferCount; i++) {
        DEBUG("index %d, mANB %p, mCount %d, mState %s, bufferTrack %p\n",
            i, gBufferInfoVec[i].mANB, gBufferInfoVec[i].mCount, sBufferState[gBufferInfoVec[i].mState],
            gBufferTrack[i]);
    }
}
#endif
static int32_t findANBIndex(MMNativeBuffer *anb) {
    int32_t index = -1;
    for (int i = 0; i < gInputBufferCount; i++) {
        if (anb == gBufferInfoVec[i].mANB) {
            index = i;
            break;
        }
    }
    return index;
}

static int32_t findBufferOwnedByUs() {
    int32_t size = 0;

    for (int i = 0; i < gInputBufferCount; i++) {
        if (gBufferInfoVec[i].mState == OWNED_BY_US) {
            size++;
        }
    }

    return size;
}

static void* sourceReadThread() {
    PRINTF("sourceRead thread enter\n");

    while (1) {
        {
            MMAutoLock locker(mMsgThrdLock);
            bool found = false;
            //int32_t index = -1;


            while (!found) {
                if (gExit) {
                    INFO("sourceReadThread exit\n");
                    return NULL;
                }
                int index = 0;
                uint32_t maxCount = 0xffffffff;
                for (int i = 0; i < gInputBufferCount; i++) {
                    //INFO("index %d, state %d, mCount %d\n", i, gBufferInfoVec[i].mState, gBufferInfoVec[i].mCount);
                    if ((gBufferInfoVec[i].mState == OWNED_BY_US)
                        && (gBufferInfoVec[i].mCount < maxCount)) {
                        index = i;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    //int64_t timeout = 2 * mDuration;
                    //ASSERT(0 && "should not be here\n");
                    //usleep(10*1000);
                    mMsgCond->wait();
                    continue;
                } else {
                    MMNativeBuffer *anb = gBufferInfoVec[index].mANB;
                    int ret = mm_queueBuffer(gNativeWindow, anb, -1);
                    if (ret != 0) {
                        ERROR("queueBuffer failed: %s (%d)", strerror(-ret), -ret);
                        return NULL;
                    }
                    INFO("queueBuffer: index %d\n", index);
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
                    gNativeWindow->finishSwap();
#endif
                    gBufferInfoVec[index].mState = OWNED_BY_NATIVEWINDOW;
                }
            }
        }

        MMNativeBuffer *anb = NULL;
        int ret;
        ret = mm_dequeueBufferWait(gNativeWindow, &anb);
        if (ret != 0) {
            ERROR("WINDOW_API(dequeue_Buffer) failed: (%d)\n", ret);
            return NULL;
        }
        if (!fillInputBuffer(anb)) {
            return NULL;
        }


        {
            MMAutoLock locker(mMsgThrdLock);
            //check buffer valid
            int i = 0;
            for (i = 0; i < gInputBufferCount; i++) {
                if (anb == gBufferInfoVec[i].mANB) {
                    MMASSERT(gBufferInfoVec[i].mState == OWNED_BY_NATIVEWINDOW);
                    INFO("getMediaBuffer index %d\n", i);
                    break;
                }
            }

            if (i == gInputBufferCount) {
                ERROR("getMediaBuffer invalid MMNativeBuffer %p", anb);
                return NULL;
            }

            //need lock
            gANBSrcBufferList.push_back(anb);
            if (gANBSrcBufferList.size() == 1) {
                mMsgCond->broadcast();
            }
            //usleep(1);
        }

    }
    return NULL;
}

static int fillVideoFrame(int64_t endWhenUs) {
    size_t in_size =0;
    uint8_t *in_buffer = NULL;

    size_t inputBufferIndex = -1;
    int inputBufferDequeRet;


    int timeout_count = 0;
    inputBufferDequeRet = g_videoEncoder->dequeueInputBuffer(&inputBufferIndex, 10*1000);
    while (inputBufferDequeRet < 0) {
        inputBufferDequeRet = g_videoEncoder->dequeueInputBuffer(&inputBufferIndex, 10*1000);
        if (timeout_count++ > 100) {
            PRINTF("timeout on encoder->dequeueInputBuffer\n");
            return VideoEncoderUnknown;
        }
    }

    if (inputBufferDequeRet >= 0) {
        in_buffer =g_videoEncoder->getInputBuffer(inputBufferIndex , &in_size);
        if (in_buffer == NULL) {
            PRINTF( "get InputBuffer fail\n");
            return VideoEncoderUnknown;
        }

    }

    if (in_buffer == NULL) {
        PRINTF( "get InputBuffer fail\n");
        return VideoEncoderUnknown;
    }


    uint32_t flags = 0;
    if (av_gettime() > endWhenUs) {
        PRINTF("set eos flags\n");
        flags |= MediaCodec::BUFFER_FLAG_EOS;
        gExit = true;
    }


    bool found = false;
    int i = 0;
    MMNativeBuffer *anb = NULL;
    while(!found){
        MMAutoLock locker(mMsgThrdLock);
        for (i = 0; i < gInputBufferCount; i++) {
            if (gBufferTrack[inputBufferIndex] == NULL
                && gBufferInfoVec[i].mState == OWNED_BY_US) {
                found = true;
                break;
            }
            if (gBufferInfoVec[i].mANB == gBufferTrack[inputBufferIndex]) {
                MMASSERT(gBufferInfoVec[i].mState == OWNED_BY_COMPONENT);
                found = true;
                break;
            }
        }

        if (!found) {
            INFO("found no buffer, enter wait\n");
            return VideoEncoderUnknown;
        } else {

            INFO("index %d, mANB %p, state %d, mBufferTrack %p ",
                 i, gBufferInfoVec[i].mANB, gBufferInfoVec[i].mState, gBufferTrack[inputBufferIndex]);
        }


        gBufferInfoVec[i].mState = OWNED_BY_US;
        if (findBufferOwnedByUs() == 1) {
            mMsgCond->broadcast();
        }

        while (gANBSrcBufferList.empty()) {
            mMsgCond->wait();
        }
        anb = *gANBSrcBufferList.begin();
        gANBSrcBufferList.pop_front();

        int32_t index = findANBIndex(anb);
        if (index < 0) {
            MMASSERT(0 && "findANBIndex failed\n");
            return VideoEncoderUnknown;
        }
        gBufferInfoVec[index].mState = OWNED_BY_COMPONENT;
        gBufferInfoVec[index].mCount = ++gCountTick;

        gBufferTrack[inputBufferIndex] = anb;//track buffer

        break;
    }

#ifndef YUNOS_ENABLE_UNIFIED_SURFACE
    passMetadataBuffer(in_buffer, anb->handle);
#else
    passMetadataBuffer(in_buffer, ((gb_wrapper_t*)(anb->target->handle))->handle);
#endif

    size_t bufferSize = sizeof(buffer_handle_t) + 4;


    int64_t timeUs = -1;
    static bool sIsFirstFrame = true;
    static int64_t sFirstFrameTimeUs = av_gettime();
    static int32_t sCount = 0;


    if (sIsFirstFrame) {
        sIsFirstFrame = false;
        timeUs = 0;
    } else {
        timeUs = av_gettime() - sFirstFrameTimeUs;
    }

    timeUs = sCount++ * 1000*1000ll/30;

    inputBufferDequeRet = g_videoEncoder->queueInputBuffer(inputBufferIndex, 0, bufferSize ,timeUs , flags);
    if (inputBufferDequeRet < 0) {
        PRINTF("error in g_videoEncoder->queueInputBuffer");
        return VideoEncoderUnknown;
    }

    DEBUG("queueInputBuffer, index %d, inputBuffer %p\n", i, in_buffer);

    if (flags & MediaCodec::BUFFER_FLAG_EOS) {
        return VideoEncoderEOS;
    }
    return 0;

}

static int write_frame_video(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt) {
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;

    //PRINTF("0x%0x, 0x%0x, index %d time_base 0x%0x 0x%0x",
    //    time_base->num, time_base->den, st->index, st->time_base.num, st->time_base.den);

    /* Write the compressed frame to the media file. */
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

static VideoEncoderResult encoderVideoOnce(OutputStream *os) {
    AVFormatContext *oc = os->oc;

    VideoEncoderResult result = VideoEncoderUnknown;
    size_t  offset,size;
    int64_t presentationTimeUs;
    uint32_t flags = 0; //-1 is not correct
    size_t outIndex = -1;
    int outRet;

    outRet = g_videoEncoder->dequeueOutputBuffer(&outIndex,&offset,&size,
             &presentationTimeUs,&flags,10000);

    int32_t newWidth =0 , newHeight = 0;
    const char *newMime = NULL;


    uint8_t *out_data = NULL;
    size_t out_size = -1;

    switch (outRet) {
    case MediaCodec::MEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED: {
        PRINTF( "INFO_OUTPUT_BUFFERS_CHANGED \n");
        //outputBuffers = decoder.getOutputBuffers();
        result = VideoEncoderIgnore;
        break;
    }
    case MediaCodec::MEDIACODEC_INFO_FORMAT_CHANGED: {
        MediaMetaSP format;
        format = g_videoEncoder->getOutputFormat();
        if (!format) {
            PRINTF("no format \n");
            break;
        }

        format->getString(MEDIA_ATTR_MIME, newMime);
        format->getInt32(MEDIA_ATTR_WIDTH, newWidth);
        format->getInt32(MEDIA_ATTR_HEIGHT, newHeight);

        PRINTF("New format: newMime=%s, newWidth=%u, newHeight=%u\n",
               newMime,newWidth,newHeight);

        uint8_t *data;
        int32_t size;
        if (format->getByteBuffer(MEDIA_ATTR_EXTRADATA0, data, size)) {
            hexDump(data, size, 16);
        }
        if (format->getByteBuffer(MEDIA_ATTR_EXTRADATA1, data, size)) {
            hexDump(data, size, 16);
        }

        result = VideoEncoderIgnore;
        break;
    }
    case MediaCodec::MEDIACODEC_INFO_TRY_AGAIN_LATER: {
        // DEBUG( "dequeueOutputBuffer timed out!\n");
        result = VideoEncoderTryAgain;
        break;
    }
    case MediaCodec::MEDIACODEC_OK: {
        if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) != 0) {
            // ignore this -- we passed the CSD into MediaMuxer when
            // we got the format change notification
            PRINTF("Got codec config buffer (%u bytes)\n", size);
#if 0
            size = 0;
#endif
        }

        if (size != 0) {
            out_size = -1;
            out_data = g_videoEncoder->getOutputBuffer(outIndex, &out_size);
            if (out_data == NULL) {
                result = VideoEncoderUnknown;
                PRINTF("getOutputBuffer failed\n");
                break;
            }
            MMASSERT(out_size == size);

#if USE_FFMPEG_ENCODER
            if (os->data == NULL) {
                int bufferSize = 1024*1024;
                os->data = new char[bufferSize];
                memset(os->data, 0, bufferSize);
            }

            AVPacket pkt;
            memset(&pkt, 0, sizeof(pkt));
            av_init_packet(&pkt);

            int ret = -1;
            pkt.stream_index  = os->st->index;
            pkt.data = (uint8_t*)os->data;
            MMASSERT(pkt.data != NULL);

            memcpy(pkt.data, out_data, size);
            pkt.size = size;
            pkt.pts = g_encorderFrameCnt;


            ret = write_frame_video(oc, &os->st->codec->time_base, os->st, &pkt);
            if (ret < 0) {
                PRINTF("Error while writing video frame, ret %d\n", ret);
                result = VideoEncoderUnknown;
                break;
            }
#else
            if (g_fp) {
                int count0 = fwrite(&size, sizeof(int), 1, g_fp);
                if (count0 == 0) {
                    PRINTF("Error while writing video frame size\n");
                    result = VideoEncoderUnknown;
                    break;
                }
                int count = fwrite(out_data, size, 1, g_fp);
                if (count == 0) {
                    PRINTF("Error while writing video frame\n");
                    result = VideoEncoderUnknown;
                    break;
                }
            }
#endif
        }


        if (g_videoEncoder->releaseOutputBuffer(outIndex, true) >= 0) {
            result = VideoEncoderOneFrame;
        } else {
            result = VideoEncoderUnknown;
            WARNING("releaseOutputBuffer failed\n");
        }

        if (++g_encorderFrameCnt % 30 == 0) {
            PRINTF("encoded frame count %d\n", g_encorderFrameCnt);
        }
        break;
    }
    default: {
        WARNING("unknown ret=%d from dequeueOutputBuffer()\n", outRet);
        result = VideoEncoderUnknown;
        break;
    }
    }

    // All frame are encoded, we can stop encoding now
    if (flags & MediaCodec::BUFFER_FLAG_EOS) {
        PRINTF("GET EOS, OutputBuffer BUFFER_FLAG_END_OF_STREAM \n");
        result = VideoEncoderEOS;
    }

    return result;
}

static void* runEncoderThread(void *context) {
    PRINTF("video encoder thread enter\n");
    OutputStream *os = (OutputStream *)context;
    while (1) {
        VideoEncoderResult result = encoderVideoOnce(os);
        if (result == VideoEncoderEOS || result == VideoEncoderUnknown)
            break;

        usleep(10*1000);
    }

#if !USE_FFMPEG_ENCODER
    if (g_fp) {
        fclose(g_fp);
        g_fp = NULL;
    }
#endif
    PRINTF("video encoder thread exit, g_encorderFrameCnt %d record\n", g_encorderFrameCnt);
    return NULL;
}

static void close_stream_video(AVFormatContext *oc, OutputStream *ost) {
    avcodec_close(ost->st->codec);

    if (ost->data) {
        delete []ost->data;
        ost->data = NULL;
    }
}


/* Add an output stream. */
static void add_stream_video(OutputStream *ost, AVFormatContext *oc,
                             AVCodec **codec,
                             enum AVCodecID codec_id) {
    AVCodecContext *c;

    ost->st = avformat_new_stream(oc, *codec);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        return;
    }
    ost->st->id = oc->nb_streams-1;
    c = ost->st->codec;

    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    c->bit_rate = gBitRate;
    /* Resolution must be a multiple of two. */
    c->width    = gVideoWidth;
    c->height   = gVideoHeight;

    c->gop_size      = 12;

    ost->st->time_base.den = STREAM_FRAME_RATE;
    ost->st->time_base.num = 1;
    c->time_base       = ost->st->time_base;
}

static int recordVideo(const char* output_url) {
    //AVStream *video_st = NULL , *audio_st = NULL;
    int ret = 0;

    MediaMetaSP format;
    pthread_t video_encoder_thread = 0;
    pthread_t source_read_thread = 0;

    OutputStream video_st;
    memset(&video_st, 0, sizeof(video_st));
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *video_codec = NULL;

    int encode_video = 0;

    int64_t startWhenUs = -1;
    int64_t endWhenUs = -1;


    PRINTF("test output url  = %s \n",output_url);
#if !USE_FFMPEG_ENCODER
    g_fp = fopen(output_url, "wb");
    if (g_fp == NULL) {
        VERBOSE("fail to open file (%s) to dump frames!!", output_url);
        PRINTF("open dump file fail\n");
        return -1;
    }

#else
    av_register_all();

    /* Autodetect the output format from the name. doutput_urlefault is MPEG. */
    fmt = av_guess_format(NULL, output_url, NULL);
    if (!fmt) {
        PRINTF("Could not deduce output format from file extension: using MPEG.\n");
        fmt = av_guess_format("mpeg", NULL, NULL);
    }
    if (!fmt) {
        PRINTF("Could not find suitable output format\n");
        return -1;
    }

    /* Allocate the output media context. */
    oc = avformat_alloc_context();
    if (!oc) {
        PRINTF( "Memory error\n");
        return -1;
    }
    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", output_url);

    //force to set AV_CODEC_ID_H264
    fmt->video_codec = AV_CODEC_ID_H264;
    //PRINTF("fmt->video_codec %d\n",fmt->video_codec);
    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream_video(&video_st, oc, &video_codec, fmt->video_codec);
        encode_video = 1;
    }

    video_st.data = NULL;
    video_st.oc = oc;
    //av_log_set_level(AV_LOG_DEBUG);

    av_dump_format(oc, 0, output_url, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&oc->pb, output_url, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open '%s'\n", output_url);
            return 1;
        }
    }

    //PRINTF("priv_data %p\n",oc->priv_data);
    //PRINTF("num %d, den %d\n",oc->streams[0]->time_base.num, oc->streams[0]->time_base.den);


    ret = avformat_write_header(oc, NULL);
    if (ret != 0) {
        printf("error avformat_write_header, ret %d", ret);
        return ret;
    }

    //PRINTF("priv_data %p\n",oc->priv_data);
    //PRINTF("num %d, den %d\n",oc->streams[0]->time_base.num, oc->streams[0]->time_base.den);

#endif

    g_videoEncoder = MediaCodec::CreateByType("video/avc" ,true/*encoder*/);
    if (g_videoEncoder ==NULL) {
        PRINTF("fail to create video encoder!\n");
        ret = -1;
        goto finish;
    }


    format=createVideoFormatForEncorder();
    if (g_videoEncoder->configure(format, NULL, MediaCodec::kFlagIsEncoder) < 0) {
        PRINTF("fail to configure encoder!\n");
        ret = -1;
        goto finish;
    }


    if (g_videoEncoder->start()< 0) {
        PRINTF("Can't start video encoder!\n");
        ret =-1;
        goto finish;
    }

    {
        //gInputBufferCount = g_videoEncoder->getInputBufferCount();
        gInputBufferCount = 16;
        int outBufCount = g_videoEncoder->getOutputBufferCount();
        const char *codecName = g_videoEncoder->getName();
        PRINTF("inBufCount = %d, outBufCount =%d, codecName=%s\n",
               gInputBufferCount,outBufCount,codecName);
    }


    if (initWindowForEncoder() < 0) {
        PRINTF("initWindowForEncoder failed\n");
        ret = -1;
        goto finish;
    }

    startWhenUs = av_gettime();
    endWhenUs = startWhenUs + gTimeLimitSec * 1000ll * 1000ll;

    //gFrameCondition(mFrameLock);

    mMsgCond = new Condition(mMsgThrdLock);

    if (!source_read_thread) {
        pthread_create(&source_read_thread, NULL, (void *(*)(void *))sourceReadThread, NULL);
    }


    if (!video_encoder_thread) {
        pthread_create(&video_encoder_thread, NULL, (void *(*)(void *))runEncoderThread, &video_st);
    }


    while (1) {
        /* select the stream to encode */
        encode_video = fillVideoFrame(endWhenUs);
        if (encode_video == VideoEncoderEOS || encode_video == VideoEncoderUnknown) {
            //PRINTF("fillVideoFrame failed\n");
            break;
        }

        usleep(10*1000);
    }

    usleep(1*1000*1000);

#if USE_FFMPEG_ENCODER
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);
#endif

finish:
    if (g_videoEncoder !=NULL) {
#if 1
        PRINTF("flush encoder\n");
        if (g_videoEncoder->flush() < 0) {
            PRINTF("video encoder flush fail \n");
            ret =-1;
        }
#endif
        PRINTF("stop encoder\n");
        if (g_videoEncoder->stop() < 0) {
            PRINTF("video encoder stop fail \n");
            ret =-1;
        }
        PRINTF("release encoder\n");
        if (g_videoEncoder->release() < 0) {
            PRINTF("video encoder release fail \n");
            ret =-1;
        }
    }

    usleep(1000000);

#if USE_FFMPEG_ENCODER
    close_stream_video(oc, &video_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_close(oc->pb);

    /* free the stream */
    avformat_free_context(oc);
#endif

    for (int i = 0; i < gInputBufferCount; i++) {
        if (gBufferInfoVec[i].mState == OWNED_BY_US)
            mm_cancelBuffer(gNativeWindow, gBufferInfoVec[i].mANB, -1);

        gBufferInfoVec[i].mState = OWNED_BY_NATIVEWINDOW;
        //mBufferInfo[i].pts = START_PTS;
    }
    delete mMsgCond;

    PRINTF("encoder to the end\n");
    return ret;
}
/*
 * Parses width and height from a string of the form "1280x720".
 *
 * Returns true if success , otherwise returns fail.
 */
static bool parseWidthHeight(const char* widthHeightStr, uint32_t* width,
                             uint32_t* height) {
    long w, h;
    char* endPos;

    // Base 10 must be specified , or "0x0" parsed in another way.
    w = strtol(widthHeightStr, &endPos, 10);
    if (*endPos != 'x' || *(endPos+1) == '\0' ||endPos == widthHeightStr) {
        // parse width fail due to invalid width chars ,or missing height or 'x'
        return false;
    }
    h = strtol(endPos + 1, &endPos, 10);
    if (*endPos != '\0') {
        // parse height fail due to invalid height chars
        return false;
    }

    *width = w;
    *height = h;
    return true;
}

static const char *size_str = NULL;
static uint32_t bitrate = 0;
static uint32_t timelimit = 0;
static gboolean roate = FALSE;
const char *fileName = NULL;

static GOptionEntry entries[] = {
    {"size", 's', 0, G_OPTION_ARG_STRING, &size_str, " Set the video size, e.g. \"1280x720\"", NULL},
    {"bit-rate", 'b', 0, G_OPTION_ARG_INT, &bitrate,  "Set the video bit rate, in megabits per second", NULL},
    {"time-limit", 't', 0, G_OPTION_ARG_INT, &timelimit, "Set the maximum recording time, in seconds.", NULL},
    {"rotate", 'r', 0, G_OPTION_ARG_NONE, &roate, "Rotate the output 90 degrees.", NULL},
    {NULL}
};

class MediacodecRecodTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(MediacodecRecodTest, mediacodecrecrdtest) {
        int err = recordVideo(fileName);
        EXPECT_GE(err , 0);
}
int main(int argc, char* const argv[]) {
    if (argc < 2) {
        PRINTF("Must specify output file \n");
        return 0;
    }
    PRINTF( "Recording continues until Ctrl-C is hit or the time limit is reached.\n");
    PRINTF("Default size is the device's main display resolution");
    PRINTF( "Default bit rate is %dMbps. \n",gBitRate / 1000000);
    PRINTF("Default / maximum time limit is %d. \n",gTimeLimitSec);


    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new(MM_LOG_TAG);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, TRUE);

    if (!g_option_context_parse(context, &argc, (char ***)&argv, &error)) {
            PRINTF("option parsing failed: %s\n", error->message);
            return -1;
    }
    g_option_context_free(context);

    if(size_str){
        PRINTF("set size %s\n", size_str);
        bool ret = parseWidthHeight(size_str, (uint32_t*)&gVideoWidth, (uint32_t*)&gVideoHeight);
        if(!ret){
             PRINTF("Invalid size '%s', must be width x height\n", size_str);
             return 2;
        }
        if (gVideoWidth == 0 || gVideoHeight == 0) {
             PRINTF("Invalid size %ux%u, width and height may not be zero\n", gVideoWidth, gVideoHeight);
             return 2;
        }
    }

    if(bitrate >0){
        PRINTF("set bitrate %d\n", bitrate);
        if (bitrate < kMinBitRate || bitrate > kMaxBitRate) {
              PRINTF("Bit rate %dbps outside acceptable range [%d,%d]\n", bitrate, kMinBitRate, kMaxBitRate);
              return 2;
        }
        gBitRate = bitrate;
    }

    if(timelimit >0){
        PRINTF("set timelimit %d\n", timelimit);
        if ( timelimit > kMaxTimeLimitSec) {
            PRINTF("Time limit %ds outside acceptable range [1,%d]\n", timelimit, kMaxTimeLimitSec);
                return 2;
        }
        gTimeLimitSec = timelimit;
    }

    if(roate){
         PRINTF("set gRotate \n");
         gRotate = roate;
    }


    // MediaMuxer tries to create the file in the constructor, but we don't
    // learn about the failure until muxer.start(), which returns a generic
    // error code without logging anything.  We attempt to create the file
    // now for better diagnostics.
    fileName = argv[optind];

    PRINTF("fileName %s, gVideoWidth %d, gVideoHeight %d, gBitRate %d, gTimeLimitSec %d gRotate %d\n",
           fileName, gVideoWidth, gVideoHeight, gBitRate, gTimeLimitSec, gRotate);

    AutoWakeLock awl;
    int ret;
    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
    } catch (...) {
        PRINTF("InitGoogleTest failed!");
        ret = -1;
    }

    if ( size_str )
        g_free(const_cast<char*>(size_str));

    return ret;

}

