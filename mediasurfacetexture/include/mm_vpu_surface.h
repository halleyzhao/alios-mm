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

#ifndef __MM_VPU_SURFACE_H__
#define __MM_VPU_SURFACE_H__

#include "multimedia/mm_cpp_utils.h"
#include "mm_surface_compat.h"

#include <vector>
#include <queue>
#include <map>


namespace yunos {
}

namespace YUNOS_MM {

class MMVpuSurface {
public:
    MMVpuSurface();
    // TODO all construct to accept outside wl_surface
    //MMVpuSurface(void *device);
    virtual ~MMVpuSurface();

    //bool initCheck();
    int set_buffers_dimensions(uint32_t width, uint32_t height);
    int set_buffers_format(uint32_t format);
    int set_buffers_transform(uint32_t transform);
    //int set_crop(Rect *crop, uint32_t &flags);
    int set_crop(int x, int y, int w, int h);
    int set_usage(uint32_t usage);
    int query(uint32_t query, int *value);
    int set_buffer_count(uint32_t count);
    int dequeue_buffer_and_wait(MMNativeBuffer **buf);
    int cancel_buffer(MMNativeBuffer *buf, int fd);
    int queueBuffer(MMNativeBuffer *buf, int fd);
    //int finishSwap();
    int mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr);
    virtual int unmapBuffer(MMNativeBuffer *buf);


private:

    int createVpuSurface();
    void destroyVpuSurface();
    bool createNativeBuffer();
    void destroyNativeBuffer(int index);
    int BufferToIndex(MMNativeBuffer* buffer);
    void dumpBufferInfo();
    void dumpBufferInfo_l();

    uint32_t mBufferCnt;
    uint32_t mFormat;
    uint32_t mUsage;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mPaddedWidth;
    uint32_t mPaddedHeight;
    uint32_t mTransform;

    int32_t mCropX;
    int32_t mCropY;
    int32_t mCropW;
    int32_t mCropH;


    Lock mLock;
    Condition mCondition;
    Condition mQueueBufCond;

    bool mNeedRealloc;
    bool mCreateDevice;

    std::vector<MMVpuBuffer*> mBufferInfo;
    std::queue<MMVpuBuffer*> mFreeBuffers; // ready buffers for dequeueBuffer
    std::map<MMNativeBuffer*, int> mBufferToIdxMap;

private:
    MM_DISALLOW_COPY(MMVpuSurface);
};

}; // end of namespace YUNOS_MM
#endif // end of __MM_VPU_SURFACE_H__
