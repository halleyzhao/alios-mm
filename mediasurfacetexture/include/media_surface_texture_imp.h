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

#ifndef __media_surface_texture_imp_h
#define __media_surface_texture_imp_h

#include <mm_surface_compat.h>

#include "multimedia/mm_cpp_utils.h"
#include "media_surface_texture.h"
#include <vector>
#include <list>
#include <map>

namespace YunOSMediaCodec {

using namespace YUNOS_MM;

class SurfaceWrapper;

class MediaSurfaceTextureImp {

public:
    MediaSurfaceTextureImp();
    virtual ~MediaSurfaceTextureImp();

    void returnAcquiredBuffers();

    int getBufferCount() const { return mBufferCount; }
    int getBuffersFormat() const { return mFormat; }

    int setBufferCount(int cnt, uint32_t &flags);
    int setBufferTransform(int transform, uint32_t &flags);
    int setBufferCrop(int left, int top, int right, int bottom, uint32_t &flags);
    //int setOffset(int x, int y);
    int setBuffersFormat(int format, uint32_t &flags);
    int setBuffersDimensions(int width, int height, uint32_t &flags);
    int setUsage(int usage, uint32_t &flags);

    // TODO make sure the buffer has been consumed?
    void finishSwap();

    int mapBuffer(MMNativeBuffer* buffer, int usage, Rect rect, void **ptr);
    int unmapBuffer(MMNativeBuffer* buffer);
    int dump_buffer_to_file(MMNativeBuffer *buf) { return 0; }

    int dequeueBuffer(MMNativeBuffer **buffer, int *fenceFd, uint32_t &flags);
    //int lockBuffer(MMNativeBuffer* buffer);
    int queueBuffer(MMNativeBuffer* buffer, int fenceFd, uint32_t &flags);
    int cancelBuffer(MMNativeBuffer* buffer, int fenceFd, uint32_t &flags);

    unsigned int width() const { return mWidth; }
    unsigned int height() const { return mHeight; }
    unsigned int format() const { return mFormat; }
    unsigned int transformHint() const { return mTransform; }

    //int setBuffersGeometry(int width, int height, int format);

    MMNativeBuffer* acquireBuffer(int index);

    int returnBuffer_lock(MMNativeBuffer* buffer);
    int returnBuffer(MMNativeBuffer* anb);

    int setWindowSurface(void *ws);
    int setShowFlag(bool show);
    bool getShowFlag();

    int setListener(SurfaceTextureListener* listener);
    bool listenerExisted();

    friend class MediaSurfaceTexture;

private:
    void* getSurface();
    int setSurfaceName(const char *name);
    static void releaseNWB(MMNativeBuffer* nwb);

    int mBufferCount;
    int mFormat;
    int mUsage;
    int mScalingMode;
    int mWidth;
    int mHeight;
    int mTransform;
    Rect mCrop;
    bool mNeedRealloc;
    bool mAsyncMode;

    /*
    State {
        DEQUEUED,
        FREE,
    };

    BufferInfo {
        MMNativeBuffer* buf;
        bool isDequeued;
        int64_t timeStamp;
    };
    */
    typedef MMSharedPtr<MMNativeBuffer> NativeWindowBufferSP;

    std::vector<MMNativeBuffer*> mAllocBuffers;
    std::map<MMNativeBuffer*, int> mBufferToIdxMap;

    // share poiners to track buffers life cycle
    std::vector<NativeWindowBufferSP> mAllocBuffersSP;

    std::list<MMNativeBuffer*> mAvailableBuffers;

    //std::list<MMNativeBuffer*> mFilledBuffers;
    MMNativeBuffer* mFilledBuffer;
    MMNativeBuffer* mPrevFilledBuffer;

    WindowSurface *mSurface;
    SurfaceWrapper *mSurfaceWrapper;

    bool mIsShow;
    bool mIsShowAsync;

    /* send last surfaceview buffer to texture client */
    bool mIsFirstTexture;
    int mLastSurfaceBufferIdx;

    Lock mLock;
    YUNOS_MM::Condition mCondition;

    SurfaceTextureListener *mListener;
    std::list<MMNativeBuffer*> mBuffersAcquired;
    int mCanceledBufferCount;
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    bool mCheckGpuSync;
#endif
    int mBuffersInCodec;

    char* mSurfaceName;

    int syncBufferQueues_locked(uint32_t &flags);
    int BufferToIndex(MMNativeBuffer* buffer);
    void dump();
    void checkSurface();

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    void resetGpuBuffer(MMNativeBuffer *buffer);
#endif

};

} // end of namespace YunOSMediaCodec
#endif //__media_surface_texture_imp_h
