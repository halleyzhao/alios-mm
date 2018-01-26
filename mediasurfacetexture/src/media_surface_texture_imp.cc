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

#include "media_surface_texture_imp.h"
#include "media_surface_utils.h"
//#include "media_codec.h"

#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
#include <NativeWindowBuffer.h>
#endif
#include "multimedia/mm_debug.h"

namespace YunOSMediaCodec {

MM_LOG_DEFINE_MODULE_NAME("MC-MST");

//#define VERBOSE INFO

#define FENTER1() DEBUG(">>>\n")
#define FLEAVE1() do {DEBUG(" <<<\n"); return;}while(0)
#define FLEAVE_WITH_CODE1(_code) do {DEBUG("<<<(status: %d)\n",(_code)); return (_code);}while(0)

#define FENTER() VERBOSE(">>>\n")
#define FLEAVE() do {VERBOSE(" <<<\n"); return;}while(0)
#define FLEAVE_WITH_STATUS(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)
#define FLEAVE_WITH_CODE(_code) do {VERBOSE("<<<(status: %d)\n",(_code)); return (_code);}while(0)
#define CHECK_POINTER(_p) do { if ( !(_p) ) {ERROR("invalid params\n"); FLEAVE_WITH_STATUS(MM_ERROR_INVALID_PARAM);}}while(0)

#define NEED_REALLOC_CHECK4(comp1, comp2, comp3, comp4, val1, val2, val3, val4) \
    if (comp1 != val1)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp2 != val2)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp3 != val3)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp4 != val4)                                                          \
        mNeedRealloc = true;                                                    \
    if (!mAllocBuffers.empty() && mNeedRealloc)                                 \
        INFO("configure changes, will re-allocate buffers");

#define NEED_REALLOC_CHECK2(comp1, comp2, val1, val2) \
    NEED_REALLOC_CHECK4(comp1, comp2, 0, 0, val1, val2, 0, 0)

#define NEED_REALLOC_CHECK1(comp1, val1) \
    NEED_REALLOC_CHECK4(comp1, 0, 0, 0, val1, 0, 0, 0)

#define MAX_BUFFER_COUNT 64

#define SURFACE_FUNC_START() \
    int ret = -1;            \
    try {

//MediaCodec::kFlagDisplayDisconnected;
#define MSTDisplayDisconnected 2048

#define SURFACE_FUNC_END()                                   \
    }                                                        \
    catch (int err) {                                        \
        ERROR("catch expectation from surface 0x%x", err);   \
        flags |= MSTDisplayDisconnected;                     \
    }                                                        \
    return ret;

//#define GET_NATIVEWINDOW(windowSurface) (static_cast<YNativeSurface *>(windowSurface))

MediaSurfaceTextureImp::MediaSurfaceTextureImp()
    : mBufferCount(0),
    mFormat(0),
    mUsage(0),
    mScalingMode(0),
    mWidth(0),
    mHeight(0),
    mTransform(0),
    mNeedRealloc(false),
    mAsyncMode(false),
    mFilledBuffer(NULL),
    mPrevFilledBuffer(NULL),
    mSurface(NULL),
    mSurfaceWrapper(NULL),
    mIsShow(false),
    mIsShowAsync(false),
    mIsFirstTexture(false),
    mLastSurfaceBufferIdx(-1),
    mCondition(mLock),
    mListener(NULL),
    mCanceledBufferCount(0),
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    mCheckGpuSync(false),
#endif
    mBuffersInCodec(0),
    mSurfaceName(NULL) {

    FENTER1();

    memset(&mCrop, 0, sizeof(mCrop));

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    mCheckGpuSync = mm_check_env_str("mm.mst.check.gpu.sync",
                                     "MM_MST_CHECK_GPU_SYNC");
#endif

    FLEAVE1();
}

MediaSurfaceTextureImp::~MediaSurfaceTextureImp() {
    DEBUG("~MediaSurfaceTextureImp");
    mCondition.broadcast();
    DEBUG("mAllocBuffersSP.size(): %d", mAllocBuffersSP.size());
    mAllocBuffersSP.clear();
    DEBUG("~MediaSurfaceTextureImp done");

    if (mSurfaceName)
        delete [] mSurfaceName;
    if (mSurfaceWrapper)
        delete mSurfaceWrapper;
}

void MediaSurfaceTextureImp::returnAcquiredBuffers() {

    FENTER1();
    MMAutoLock lock(mLock);

    int size = mBuffersAcquired.size();

    INFO("returnAcquiredBuffers buffers size is %d\n", size);
    while(!mBuffersAcquired.empty()) {
        MMNativeBuffer* anb = *mBuffersAcquired.begin();;
        INFO("return not used buffer %p\n", anb);

        if (anb) {
            returnBuffer_lock(anb);
            //mBuffersAcquired.erase(it++);
        }
    }

    if (mFilledBuffer)
        returnBuffer_lock(mFilledBuffer);

    if (mPrevFilledBuffer)
        returnBuffer_lock(mPrevFilledBuffer);

    mFilledBuffer = NULL;
    mPrevFilledBuffer = NULL;

    FLEAVE1();
}

int MediaSurfaceTextureImp::setBufferCount(int cnt, uint32_t &flags) {
    FENTER();
    NEED_REALLOC_CHECK1(mBufferCount, cnt)
    checkSurface();

    int ret = 0;
    INFO("cnt %d, mBufferCount %d\n", cnt, mBufferCount);

    mBufferCount = cnt;
    mCanceledBufferCount = 0;
    mBuffersInCodec = 0;
    mNeedRealloc = true;
    mAsyncMode = false;

    returnAcquiredBuffers();

    if (mSurfaceWrapper) {

        ret = mSurfaceWrapper->set_buffer_count(cnt, flags);
#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
        static_cast<yunos::libgui::Surface *>(mSurface)->setMode(false);
#if !defined(__PHONE_BOARD_QCOM__) && !defined(YUNOS_BOARD_qemu)
        static_cast<yunos::libgui::Surface *>(mSurface)->setBufferTransformMode(transform_full_fill);
#endif
#endif
        mAsyncMode = true;
    }

    FLEAVE_WITH_CODE(ret);
}

int MediaSurfaceTextureImp::setBufferTransform(int transform, uint32_t &flags) {
    FENTER();
    //NEED_REALLOC_CHECK1(mTransform, transform)

    int ret = 0;
    mTransform = transform;
    //mCanceledBufferCount = 0;
    //mBuffersInCodec = 0;

    if (mSurfaceWrapper) {
        ret = mSurfaceWrapper->set_buffers_transform(transform, flags);
    }

    FLEAVE_WITH_CODE(ret);
}

int MediaSurfaceTextureImp::setBufferCrop(int left, int top, int right, int bottom, uint32_t &flags) {
    FENTER();
    /* NEED_REALLOC_CHECK4(mRect.left, mRect.left, mRect.right, mRect.bottom, left, top, right, bottom) */

    int ret = 0;
    mCrop.left = left;
    mCrop.top = top;
    mCrop.right = right;
    mCrop.bottom = bottom;

    if (mSurfaceWrapper) {
        Rect crop;
        crop.left = left;
        crop.right = right;
        crop.top = top;
        crop.bottom = bottom;

        //ret = native_set_crop(GET_NATIVEWINDOW(mSurface), &crop);
        ret = mSurfaceWrapper->set_crop(&crop, flags);
    }
    //mCanceledBufferCount = 0;

    FLEAVE_WITH_CODE(ret);
}

int MediaSurfaceTextureImp::setBuffersFormat(int format, uint32_t &flags) {
    FENTER();
    NEED_REALLOC_CHECK1(mFormat, format)
    checkSurface();

    int ret = 0;
    mFormat = format;
    mCanceledBufferCount = 0;
    mBuffersInCodec = 0;

    returnAcquiredBuffers();

    if (mSurfaceWrapper) {
        ret = mSurfaceWrapper->set_buffers_format(format, flags);
    }

    FLEAVE_WITH_CODE(ret);
}

int MediaSurfaceTextureImp::setBuffersDimensions(int width, int height, uint32_t &flags) {
    FENTER();
    NEED_REALLOC_CHECK2(mWidth, mHeight, width, height)
    checkSurface();

    int ret = 0;
    mWidth = width;
    mHeight = height;
    mCanceledBufferCount = 0;
    mBuffersInCodec = 0;

    returnAcquiredBuffers();

    if (mSurfaceWrapper) {
        ret = mSurfaceWrapper->set_buffers_dimensions(width, height, flags);
    }

    FLEAVE_WITH_CODE(ret);
}

int MediaSurfaceTextureImp::mapBuffer(MMNativeBuffer* buffer,
                                   int usage,
                                   Rect rect,
                                   void **ptr) {
    FENTER();

    if (buffer == NULL) {
        ERROR("MMNativeBuffer is NULL");
        FLEAVE_WITH_CODE(-1);
    }

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__

#else
    NativeWindowBuffer *nativeBuffer = static_cast<NativeWindowBuffer*>(buffer);

#ifndef YUNOS_ENABLE_UNIFIED_SURFACE
    if (nativeBuffer)
        nativeBuffer->lock(ALLOC_USAGE_PREF(SW_READ_OFTEN), rect, ptr);
#else
    YunAllocator &allocator(YunAllocator::get());
    allocator.lock(nativeBuffer->target,
        ALLOC_USAGE_PREF(SW_READ_OFTEN),
        rect, ptr);
#endif
#endif
    FLEAVE_WITH_CODE(0);

}

int MediaSurfaceTextureImp::unmapBuffer(MMNativeBuffer* buffer) {
    FENTER();

    if (buffer == NULL) {
        ERROR("MMNativeBuffer is NULL");
        FLEAVE_WITH_CODE(-1);
    }

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__

#else
    NativeWindowBuffer *nativeBuffer = static_cast<NativeWindowBuffer*>(buffer);

#ifndef YUNOS_ENABLE_UNIFIED_SURFACE
    nativeBuffer->unlock();
#else
    YunAllocator &allocator(YunAllocator::get());
    allocator.unlock(nativeBuffer->target);
#endif
#endif

    FLEAVE_WITH_CODE(0);
}

int MediaSurfaceTextureImp::setUsage(int usage, uint32_t &flags) {
    FENTER();
    NEED_REALLOC_CHECK1(mUsage, usage)
    checkSurface();

    int ret = 0;

    mUsage = usage;
    mCanceledBufferCount = 0;
    mBuffersInCodec = 0;

    if (mSurfaceWrapper) {
        ret = mSurfaceWrapper->set_usage(usage, flags);
    }

    FLEAVE_WITH_CODE(ret);
}

int MediaSurfaceTextureImp::dequeueBuffer(MMNativeBuffer **buffer, int *fenceFd, uint32_t &flags) {
    FENTER();
    MMAutoLock lock(mLock);

    *buffer = NULL;

    if (mBufferCount < 0 || mBufferCount > MAX_BUFFER_COUNT) {
        ERROR("invalid buffer count %d", mBufferCount);
        FLEAVE_WITH_CODE(-1);
    }

    if (mNeedRealloc) {
        INFO("need realloc buffers");
        mNeedRealloc = false;

        syncBufferQueues_locked(flags);

        if (mAvailableBuffers.size() != mAllocBuffers.size())
            WARNING("client still own some buffers, still destroy");

        INFO("mAvailableBuffers size is %d\n", mAvailableBuffers.size());
        // continuiously cancel buffer to surface can result in crash,
        // looks like libgui bug, so we avoid this by calling freeBuffer API
        while (mSurfaceWrapper && !mAvailableBuffers.empty()) {
            MMNativeBuffer* buf = mAvailableBuffers.front();

            int ret = mSurfaceWrapper->cancel_buffer(buf, -1, flags);
            if (ret)
                WARNING("fail to cancel buffer to gui surface, w/ ret %d", ret);

            WARNING("realloc buffers: cancel buffer %p\n", buf);

            mAvailableBuffers.pop_front();
        }

        mAllocBuffers.clear();
        mBufferToIdxMap.clear();
        mAllocBuffersSP.clear();
        mAvailableBuffers.clear();
    }

    if (mAllocBuffers.size() < (uint32_t)mBufferCount) {
        checkSurface();
        if (mSurfaceWrapper) {
            //native_obtain_buffer_and_wait(GET_NATIVEWINDOW(mSurface), buffer);
            mSurfaceWrapper->dequeue_buffer_and_wait(buffer, flags);
            INFO("allocate anb from Surface, anb %p\n", *buffer);
        } else {
#if defined(__MM_YUNOS_LINUX_BSP_BUILD__)
            ERROR("no allocator");
            FLEAVE_WITH_CODE(-1);
#else
            NativeWindowBuffer *nwb;

            int err = -1;
            nwb = new NativeWindowBuffer(mWidth, mHeight, mFormat, mUsage);
            err = nwb->bufInit();

            if (err) {
                ERROR("fail to allocate native window buffer");
                delete nwb;
                nwb = NULL;
                FLEAVE_WITH_CODE1(err);
            }
            //sharedPointer.reset(nwb);
            NativeWindowBufferSP sharedPointer(nwb, releaseNWB);
            mAllocBuffersSP.push_back(sharedPointer);
            INFO("mAllocBuffersSP size is %d", mAllocBuffersSP.size());
            *buffer = nwb;
            // surface incStrong for MMNativeBuffer, so does mst
#if defined(__MM_YUNOS_YUNHAL_BUILD__) || defined(YUNOS_ENABLE_UNIFIED_SURFACE)
            (*buffer)->base.incRef(&((*buffer)->base));
#else
            (*buffer)->incStrong(*buffer);
#endif
            INFO("create NativeWindowBuffer for mst: %p, anb: %p, nwb: %p\n", this, *buffer, nwb);
#endif
        }

        int index = (int)mAllocBuffers.size();
        mAllocBuffers.push_back(*buffer);
        mBufferToIdxMap[*buffer] = index;

        mBuffersInCodec++;
        FLEAVE_WITH_CODE(0);
    }

    if (mSurfaceWrapper && mIsShow) {
        //native_obtain_buffer_and_wait(GET_NATIVEWINDOW(mSurface), buffer);
        mSurfaceWrapper->dequeue_buffer_and_wait(buffer, flags);
        mBuffersInCodec++;
        FLEAVE_WITH_CODE(0);
    }

    int32_t cntOwnBySurface  = 0;
    cntOwnBySurface = mBufferCount - mAvailableBuffers.size() - mBuffersAcquired.size() -  mCanceledBufferCount - mBuffersInCodec;
    if (mFilledBuffer)
        cntOwnBySurface--;

    if (mPrevFilledBuffer)
        cntOwnBySurface--;

    // keep one canceled buffer is necessary for mSurface
    if (mSurfaceWrapper && (mCanceledBufferCount > 1)) {
        //native_obtain_buffer_and_wait(GET_NATIVEWINDOW(mSurface), buffer);
        mSurfaceWrapper->dequeue_buffer_and_wait(buffer, flags);
        INFO("WindowSurface hide, dequeueBuffer from Surface, mCanceledBufferCount %d\n", mCanceledBufferCount);
        mCanceledBufferCount--;
        mBuffersInCodec++;
        INFO("MST buf cnt %d, available %d, filledBufferPtr %p %p, acquire %d, cancel %d, codec %d, surface %d",
              mBufferCount, mAvailableBuffers.size(), mFilledBuffer, mPrevFilledBuffer, mBuffersAcquired.size(), mCanceledBufferCount, mBuffersInCodec, cntOwnBySurface);
        FLEAVE_WITH_CODE(0);
    }

    if (mSurfaceWrapper && cntOwnBySurface > 1) {
        //native_obtain_buffer_and_wait(GET_NATIVEWINDOW(mSurface), buffer);
        mSurfaceWrapper->dequeue_buffer_and_wait(buffer, flags);
        INFO("WindowSurface hide, dequeueBuffer from Surface\n");
        // mCanceledBufferCount--;
        mBuffersInCodec++;
        INFO("MST buf cnt %d, available %d, filledBufferPtr %p %p, acquire %d, cancel %d, codec %d, surface %d",
              mBufferCount, mAvailableBuffers.size(), mFilledBuffer, mPrevFilledBuffer, mBuffersAcquired.size(), mCanceledBufferCount, mBuffersInCodec, cntOwnBySurface);
        FLEAVE_WITH_CODE(0);
    }

    //syncBufferQueues_locked(flags);

    if (mAvailableBuffers.empty()) {
        INFO("no buffer to dequeue now, buffer cnt %d, available %d, filledBufferPtr %p %p, acquire %d, cancel %d, codec %d",
              mBufferCount, mAvailableBuffers.size(), mFilledBuffer, mPrevFilledBuffer, mBuffersAcquired.size(), mCanceledBufferCount, mBuffersInCodec);
        mCondition.wait();
    }

    INFO("MST buf cnt %d, available %d, filledBufferPtr %p %p, acquire %d, cancel %d, codec %d",
          mBufferCount, mAvailableBuffers.size(), mFilledBuffer, mPrevFilledBuffer, mBuffersAcquired.size(), mCanceledBufferCount, mBuffersInCodec);

    if (!mAvailableBuffers.empty()) {
        *buffer = mAvailableBuffers.front();
#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
        NativeWindowBuffer *wnb = static_cast<NativeWindowBuffer*>(*buffer);
        wnb->sync_wait_release_fence();
#endif
        mAvailableBuffers.pop_front();
        mBuffersInCodec++;
    } else
        ERROR("no buffer available, return null");


    FLEAVE_WITH_CODE(0);
}

int MediaSurfaceTextureImp::queueBuffer(MMNativeBuffer* buffer, int fenceFd, uint32_t &flags) {
    FENTER();
    DEFINE_STATIC_TIDYLOG();
    int index = -1;
    {
        MMAutoLock lock(mLock);

        std::list<MMNativeBuffer*>::iterator it = mAvailableBuffers.begin();
        for (; it != mAvailableBuffers.end(); it++) {
            if (*it == buffer) {
                ERROR("invalid queue operation on buffer %p, already owned by surface", buffer);
                FLEAVE_WITH_CODE(-1);
            }
        }

        if (mFilledBuffer) {
            /* web video usually has surface */
            if (mSurfaceWrapper) {
                INFO("previous filled buffer %p is not consumed, drop it!", mFilledBuffer);
                mAvailableBuffers.push_back(mFilledBuffer);
            } else { // case for VR
                if (mPrevFilledBuffer)
                    mAvailableBuffers.push_back(mPrevFilledBuffer);
                mPrevFilledBuffer = mFilledBuffer;
            }
            mFilledBuffer = NULL;
        }

        syncBufferQueues_locked(flags);

        if (mAsyncMode && mSurface) {
#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
            static_cast<yunos::libgui::Surface *>(mSurface)->setMode(true);
#endif
            mAsyncMode = false;
        }

        if (mSurfaceWrapper && mIsShow) {
            SURFACE_FUNC_START()

            //ret = mSurface->submitBuffer((yunos::libgui::BaseNativeSurfaceBuffer*)buffer,
                                     //fenceFd);
            ret = mSurfaceWrapper->queueBuffer(buffer, fenceFd, flags);

            mBuffersInCodec--;
            //mLastSurfaceBufferIdx = BufferToIndex(buffer);
            // disable "last surface buffer", in case of keeping switching quickly,
            // last buffer is acuqired but not return
            mLastSurfaceBufferIdx = -1;
            SURFACE_FUNC_END()

        } else {
            mFilledBuffer = buffer;
            index = BufferToIndex(buffer);
        }

        mBuffersInCodec--;
    }

    if (index >= 0 && mListener) {
        PRINT_TIDY_LOG();
        DEBUG("texture update %d", index);
        mListener->onMessage(0, index, 0);
    }

    FLEAVE_WITH_CODE(0);
}

int MediaSurfaceTextureImp::BufferToIndex(MMNativeBuffer* buffer) {
    std::map<MMNativeBuffer*, int>::iterator it = mBufferToIdxMap.find(buffer);
    if (it != mBufferToIdxMap.end()) {
        return it->second;
    }

    return -1;
}

int MediaSurfaceTextureImp::cancelBuffer(MMNativeBuffer* buffer, int fenceFd, uint32_t &flags) {
    FENTER();
    MMAutoLock lock(mLock);

    std::list<MMNativeBuffer*>::iterator it = mAvailableBuffers.begin();
    for (; it != mAvailableBuffers.end(); it++) {
        if (*it == buffer) {
            ERROR("invalid cancel operation on buffer %p, already owned by surface", buffer);
            FLEAVE_WITH_CODE(-1);
        }
    }

    if (mSurfaceWrapper) {
        SURFACE_FUNC_START()

        //ret = mSurface->dropBuffer((yunos::libgui::BaseNativeSurfaceBuffer*)buffer,
                                   //fenceFd);
        ret = mSurfaceWrapper->cancel_buffer(buffer, fenceFd, flags);

        if (ret)
            WARNING("fail to cancel buffer to gui surface, w/ ret %d", ret);

        mCanceledBufferCount++;
        INFO("WindowSurface show(%d), cancel buffer %p to gui, cancel buffer count %d\n", mIsShow, buffer, mCanceledBufferCount);

        mBuffersInCodec--;
        SURFACE_FUNC_END()
    } else {
        INFO("add to available buffers %p", buffer);
        mAvailableBuffers.push_back(buffer);
    }

    mBuffersInCodec--;
    FLEAVE_WITH_CODE(0);
}

MMNativeBuffer* MediaSurfaceTextureImp::acquireBuffer(int index) {
    FENTER();
    MMAutoLock lock(mLock);
    DEFINE_STATIC_TIDYLOG();

    PRINT_TIDY_LOG();

    if ((uint32_t)index >= mAllocBuffers.size() ||
        mAllocBuffers.size() < (uint32_t)mBufferCount) {
        INFO("invalid acquireBuffer: index %d, allocated %d buffers, mst buffer cnt %d",
             index, mAllocBuffers.size(), mBufferCount);
        return NULL;
    }

    MMNativeBuffer* buffer = mAllocBuffers.at(index);
    if (buffer == NULL) {
        ERROR("invalid buffer idx %d", index);
        return NULL;
    }

    if (index == mLastSurfaceBufferIdx) {
        INFO("acquire last surface buffer, buffer[%d] %p", index, buffer);
        return buffer;
    }

    std::list<MMNativeBuffer*>::iterator it = mAvailableBuffers.begin();
#if 0
    for (; it != mAvailableBuffers.end(); it++) {
        if (*it == buffer) {
            WARNING("buffer idx %d is not filled, possibly it's dropped and recycle in available buffer list, %p\n",
                index, buffer);
            return NULL;
        }
    }
#endif

    MMNativeBuffer* anb;

    if (mPrevFilledBuffer == buffer) {
        DEBUG("acquire prev filled buffer");
        anb = mPrevFilledBuffer;
        mPrevFilledBuffer = NULL;
        goto acquired;
    }

    if (mFilledBuffer == NULL) {
        INFO("filled buffer %d is NULL", index);
        return NULL;
    }

    if (mFilledBuffer != buffer) {
        INFO("filled buffer %d is replaced, possibly it's dropped", index);
        return NULL;
    }

    anb = mFilledBuffer;
    mFilledBuffer = NULL;

    if (mPrevFilledBuffer)
        ERROR("invalid buffer state, may go with out of order");

acquired:
    mBuffersAcquired.push_back(anb);
    INFO("acquired buffer(%d) %p", index, anb);

    return anb;
}

int MediaSurfaceTextureImp::returnBuffer_lock(MMNativeBuffer* buffer) {
    std::list<MMNativeBuffer*>::iterator it = mAvailableBuffers.begin();
    for (; it != mAvailableBuffers.end(); it++) {
        if (*it == buffer) {
            ERROR("buffer idx %p is already return", buffer);
            FLEAVE_WITH_CODE(0);
        }
    }

    bool found = false;
    /*
    std::vector<MMNativeBuffer*>::iterator v = mAllocBuffers.begin();
    for (; v != mAllocBuffers.end(); v++) {
        if ((*v) == buffer) {
            found = true;
            break;
        }
    }
    */

    int index = BufferToIndex(buffer);

    if (index < 0) {
        ERROR("buffer %p is not allocated by me", buffer);
        FLEAVE_WITH_CODE(-1);
    }

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    if (mCheckGpuSync)
        resetGpuBuffer(buffer);
#endif

    if (mLastSurfaceBufferIdx >= 0 && (buffer == mAllocBuffers.at(mLastSurfaceBufferIdx))) {
        INFO("last surface buffer is returned, buffer[%d] %p", mLastSurfaceBufferIdx, buffer);
        mLastSurfaceBufferIdx = -1;
        return 0;
    }

    found = false;

    for (it = mBuffersAcquired.begin(); it != mBuffersAcquired.end(); it++) {
        if (*it == buffer) {
            mBuffersAcquired.erase(it);
            found = true;
            break;
        }
    }

    if (buffer == mFilledBuffer)
        found = true;

    if (found) {
        INFO("add to available buffers %p", buffer);
        mAvailableBuffers.push_back(buffer);
        mCondition.signal();
    } else
        WARNING("buffer has been return to us or not acquired, buffer %p\n", buffer);

    return 0;
}


int MediaSurfaceTextureImp::returnBuffer(MMNativeBuffer* buffer) {
    FENTER();
    MMAutoLock lock(mLock);
    returnBuffer_lock(buffer);
    FLEAVE_WITH_CODE(0);
}

int MediaSurfaceTextureImp::setWindowSurface(void* ws) {
    FENTER();
    MMAutoLock lock(mLock);

    mSurface = static_cast<WindowSurface*>(ws);

#ifdef __MM_BUILD_DRM_SURFACE__
    mSurfaceWrapper = new YunOSMediaCodec::WlDrmSurfaceWrapper((omap_device *)NULL);
#elif __MM_BUILD_VPU_SURFACE__
    mSurfaceWrapper = new YunOSMediaCodec::VpuSurfaceWrapper();
#else
    mSurfaceWrapper = new WindowSurfaceWrapper(mSurface);
#endif
    FLEAVE_WITH_CODE(0);
}

int MediaSurfaceTextureImp::setShowFlag(bool show) {
    FENTER();
    MMAutoLock lock(mLock);

    INFO("set show flag %d", show);
    mIsShowAsync = show;

    if (show) {
        // TODO queue last tex buffer to Surface
    } else if (mListener && mIsFirstTexture && mLastSurfaceBufferIdx >= 0) {
        INFO("update texture by last SurfaceView buffer (idx %d)", mLastSurfaceBufferIdx);
        mListener->onMessage(0, (int)mLastSurfaceBufferIdx, 0);
        mIsFirstTexture = false;
    }

    FLEAVE_WITH_CODE(0);
}

bool MediaSurfaceTextureImp::getShowFlag() {
    FENTER();

    FLEAVE_WITH_CODE(mIsShowAsync);
}

int MediaSurfaceTextureImp::syncBufferQueues_locked(uint32_t &flags) {
    FENTER();

    if (!mSurfaceWrapper)
        FLEAVE_WITH_CODE(0);

    if (mIsShow != mIsShowAsync) {
        INFO("show surface flag (%d) takes effect", mIsShowAsync);
        mIsShow = mIsShowAsync;

        if (mIsShow) {
            INFO("switching to surface view, set 'first texture' flag");
            mIsFirstTexture = true;
        }
    }

    if (!mIsShow)
        FLEAVE_WITH_CODE(0);

    if (mAvailableBuffers.empty())
        FLEAVE_WITH_CODE(0);

    INFO("show surface, cancel %d free buffers to WindowSurface",
         mAvailableBuffers.size());

    int ret = 0;
    while(!mAvailableBuffers.empty()) {
        MMNativeBuffer *buf = mAvailableBuffers.front();

            //ret = mSurface->dropBuffer((yunos::libgui::BaseNativeSurfaceBuffer*)buf,
                                       //-1);
            ret = mSurfaceWrapper->cancel_buffer(buf, -1, flags);
            if (ret) {
                flags |= MSTDisplayDisconnected;
                ERROR("fail to cancel buffer, err %d", ret);
            } else {
                mCanceledBufferCount++;
                mAvailableBuffers.pop_front();
            }
    }

    FLEAVE_WITH_CODE(ret);
}

void MediaSurfaceTextureImp::finishSwap() {
    FENTER();
    MMAutoLock lock(mLock);

    uint32_t flags;
    if (mSurfaceWrapper && mIsShow)
        mSurfaceWrapper->finishSwap(flags);
}

int MediaSurfaceTextureImp::setListener(SurfaceTextureListener* listener) {
    FENTER();
    MMAutoLock lock(mLock);

    mListener = listener;

    FLEAVE_WITH_CODE(0);
}

bool MediaSurfaceTextureImp::listenerExisted() {
    FENTER();
    MMAutoLock lock(mLock);
    bool ret;

    ret = (mListener != NULL);

    FLEAVE_WITH_CODE(ret);
}

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
void MediaSurfaceTextureImp::resetGpuBuffer(MMNativeBuffer *buffer) {
    Rect rect;
    void *ptr = NULL;
    rect.left = 0;
    rect.top = 0;
    rect.right = mWidth;
    rect.bottom = mHeight;
    NativeWindowBuffer *nativeBuffer = static_cast<NativeWindowBuffer*>(buffer);
#ifndef YUNOS_ENABLE_UNIFIED_SURFACE
    //nativeBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, rect, &ptr);
    nativeBuffer->lock(ALLOC_USAGE_PREF(SW_WRITE_OFTEN), rect, &ptr);
#else
    YunAllocator &allocator(YunAllocator::get());
    allocator.lock(nativeBuffer->target,
        ALLOC_USAGE_PREF(SW_READ_OFTEN) | ALLOC_USAGE_PREF(SW_WRITE_OFTEN),
        rect, &ptr);
#endif

    if (ptr) {
        INFO("fill returned buffer with zeros");
        memset(ptr, 0, mWidth * mHeight * 3 / 2);
    }

#ifndef YUNOS_ENABLE_UNIFIED_SURFACE
#else
    allocator.unlock(nativeBuffer->target);
#endif
}
#endif

void MediaSurfaceTextureImp::dump() {
    INFO("MST buf cnt %d, availale %d, filledBufferPtr %p, acquire %d, cancel %d, codec %d",
          mBufferCount, mAvailableBuffers.size(), mFilledBuffer, mBuffersAcquired.size(), mCanceledBufferCount, mBuffersInCodec);

    INFO("dump available buffer:");
    std::list<MMNativeBuffer*>::iterator it;
    for (it = mAvailableBuffers.begin(); it != mAvailableBuffers.end(); it++)
        INFO("anb: %p", *it);

    INFO("dump filled buffer:");
    INFO("anb: %p", mFilledBuffer);

    INFO("dump acquired buffer:");
    for (it = mBuffersAcquired.begin(); it != mBuffersAcquired.end(); it++)
        INFO("anb: %p", *it);
}

void* MediaSurfaceTextureImp::getSurface() {
    INFO("mSurface is %p", mSurface);
    INFO("mSurfaceName is %p", mSurfaceName);

    if (mSurfaceName)
        return mSurfaceName;

    return (void*)mSurface;
}

int MediaSurfaceTextureImp::setSurfaceName(const char* name) {

    if (!name || name[0] == '\0') {
        ERROR("bad surface name %p", name);
        return -1;
    }

    mSurfaceName = new char[strlen(name) + 1];
    strcpy(mSurfaceName, name);

    const char* key = "host.media.player.type";
    std::string envStr = mm_get_env_str(key, NULL);
    const char* value = envStr.c_str();
    INFO("player type: %s", value);
    if (!strncasecmp(value, "local", 5)) {
        INFO("return -1 as media player service is not used");
        return -1;
    }

    return 0;
}

/*static*/
void MediaSurfaceTextureImp::releaseNWB(MMNativeBuffer* nwb) {
#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
    NativeWindowBuffer *buf = static_cast<NativeWindowBuffer*>(nwb);

    INFO("");
    if (!buf)
        return;

#if defined(__MM_YUNOS_YUNHAL_BUILD__) || defined(YUNOS_ENABLE_UNIFIED_SURFACE)
    (buf)->base.decRef(&(buf->base));
#else
    INFO("");
    (buf)->decStrong(buf);
    //VERBOSE("refCount is %d", buf->refcount);
#endif
#endif

}

void MediaSurfaceTextureImp::checkSurface() {
#ifdef __MM_BUILD_DRM_SURFACE__
    // need to wayland drm surface to generate bo
    if (!mSurfaceWrapper) {
        INFO("create internal wayland drm surface");
        mSurfaceWrapper = new YunOSMediaCodec::WlDrmSurfaceWrapper((omap_device *)NULL);
    }
#elif __MM_BUILD_VPU_SURFACE__
    if (!mSurfaceWrapper) {
        INFO("create internal wayland drm surface");
        mSurfaceWrapper = new YunOSMediaCodec::VpuSurfaceWrapper();
    }
#endif
}


} // end of namespace YunOSMediaCodec
