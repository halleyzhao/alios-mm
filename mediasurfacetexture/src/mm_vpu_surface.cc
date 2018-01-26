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


#include <unistd.h>
#include <assert.h>

#include <mm_vpu_surface.h>
#include <multimedia/mm_debug.h>


// 32/24 are Padding Amounts from TI <<H264_Decoder_HDVICP2_UserGuide>>

#define MAX_BUFFER_COUNT 64

#define NEED_REALLOC_CHECK4(comp1, comp2, comp3, comp4, val1, val2, val3, val4) \
    if (comp1 != val1)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp2 != val2)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp3 != val3)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp4 != val4)                                                          \
        mNeedRealloc = true;                                                    \
    if (!mBufferInfo.empty() && mNeedRealloc)                                   \
        INFO("configure changes, will re-allocate buffers");

#define NEED_REALLOC_CHECK2(comp1, comp2, val1, val2) \
    NEED_REALLOC_CHECK4(comp1, comp2, 0, 0, val1, val2, 0, 0)

#define NEED_REALLOC_CHECK1(comp1, val1) \
    NEED_REALLOC_CHECK4(comp1, 0, 0, 0, val1, 0, 0, 0)

MM_LOG_DEFINE_MODULE_NAME("WlDrmSurf");

namespace YUNOS_MM
{

#define FUNC_TRACK() FuncTracker tracker(MM_LOG_TAG, __FUNCTION__, __LINE__)
// #define FUNC_TRACK()

// debug use only
static uint32_t s_ReleaseBufCount = 0;
static uint32_t s_FrameCBCount = 0;


MMVpuSurface::MMVpuSurface()
    : mBufferCnt(0),
      mFormat(0),
      mUsage(0),
      mWidth(0),
      mHeight(0),
      mPaddedWidth(0),
      mPaddedHeight(0),
      mTransform(0),
      mCropX(0),
      mCropY(0),
      mCropW(0),
      mCropH(0),
      mCondition(mLock),
      mQueueBufCond(mLock),
      mNeedRealloc(false),
      mCreateDevice(false) {
    FUNC_TRACK();

}

MMVpuSurface::~MMVpuSurface() {
    FUNC_TRACK();

}

int MMVpuSurface::createVpuSurface() {
    FUNC_TRACK();

    return 0;
}

void MMVpuSurface::destroyVpuSurface() {
    FUNC_TRACK();

}


bool MMVpuSurface::createNativeBuffer() {
    FUNC_TRACK();


    return true;
}

void MMVpuSurface::destroyNativeBuffer(int i) {
    FUNC_TRACK();


}

int MMVpuSurface::set_buffers_dimensions(uint32_t width, uint32_t height) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK2(mWidth, mHeight, width, height)

    mWidth = width;
    mHeight = height;



    return 0;
}

int MMVpuSurface::set_buffers_format(uint32_t format) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK1(mFormat, format)

    INFO("set format %x", format);
    mFormat = format;

    return 0;
}

int MMVpuSurface::set_buffers_transform(uint32_t transform) {
    FUNC_TRACK();

    INFO("set transform rotation %d", transform);
    mTransform = transform;

    return 0;
}

int MMVpuSurface::set_crop(int x, int y, int w, int h) {
    FUNC_TRACK();

    INFO("set crop %d %d %d %d", x, y, w, h);

    mCropX = x;
    mCropY = y;
    mCropW = w;
    mCropH = h;

    return 0;
}

int MMVpuSurface::set_usage(uint32_t usage) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK1(mUsage, usage)

    INFO("set usage %x", usage);
    mUsage = usage;


    INFO("get %dx%d after padding %dx%d", mWidth, mHeight, mPaddedWidth, mPaddedHeight);
    return 0;
}

int MMVpuSurface::query(uint32_t query, int *value) {
    FUNC_TRACK();
    return 0;
}

int MMVpuSurface::set_buffer_count(uint32_t count) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK1(mBufferCnt, count)

    mBufferCnt = count;

    return 0;
}

int MMVpuSurface::dequeue_buffer_and_wait(MMNativeBuffer **buffer) {
    FUNC_TRACK();
    MMAutoLock lock(mLock);


    return 0;
}

int MMVpuSurface::cancel_buffer(MMNativeBuffer *buf, int fd) {
    FUNC_TRACK();
    MMAutoLock lock(mLock);

    return 0;
}

int MMVpuSurface::queueBuffer(MMNativeBuffer *buf, int fd) {
    FUNC_TRACK();
    MMAutoLock lock(mLock);


    return 0;
}

int MMVpuSurface::BufferToIndex(MMNativeBuffer* buffer) {
    std::map<MMNativeBuffer*, int>::iterator it = mBufferToIdxMap.find(buffer);
    if (it != mBufferToIdxMap.end()) {
        return it->second;
    }

    return -1;
}

int MMVpuSurface::mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr) {
    FUNC_TRACK();
    if (!buf) {
        return -1;
    }
    return 0;
}

int MMVpuSurface::unmapBuffer(MMNativeBuffer *buf) {
    FUNC_TRACK();
    if (!buf) {
        return -1;
    }

    return 0;
}

void MMVpuSurface::dumpBufferInfo() {
    MMAutoLock lock(mLock);
    dumpBufferInfo_l();
}

void MMVpuSurface::dumpBufferInfo_l() {
    FUNC_TRACK();

}

} // end of YUNOS_MM
