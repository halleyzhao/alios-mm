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

#ifndef MM_WAYLAND_DRM_SURFACE
#define MM_WAYLAND_DRM_SURFACE

#include "multimedia/mm_cpp_utils.h"
#include "mm_surface_compat.h"

#include <vector>
#include <queue>
#include <map>

struct omap_device;
struct wl_buffer;
struct omap_bo;
struct wl_callback;
struct wl_display;

namespace yunos {
class DrmAllocator;
}

namespace YUNOS_MM {
#define QUERY_GET_X_STRIDE 0x01
#define QUERY_GET_Y_STRIDE 0x02

class WlDisplayType;

class MMWaylandDrmSurface {
public:
    MMWaylandDrmSurface(void *device);
    // TODO all construct to accept outside wl_surface
    //MMWaylandDrmSurface(void *device);
    virtual ~MMWaylandDrmSurface();

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

    friend void frame_callback(void *data, struct wl_callback *callback, uint32_t time);
    friend void buffer_release_callback (void *data, struct wl_buffer *wlBuffer);

private:

    int createWaylandDrmSurface();
    void destroyWaylandDrmSurface();
    bool createNativeBuffer();
    void destroyNativeBuffer(int index);
    int BufferToIndex(MMNativeBuffer* buffer);
    void dumpBufferInfo();
    void dumpBufferInfo_l();
    void setBufferCrop_l(MMWlDrmBuffer*);

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

    int32_t mHwPadX;
    int32_t mHwPadY;

    struct omap_device *mDevice;
    yunos::DrmAllocator *mDrmAlloc;
    struct wl_display *mWlDisplay;

    WlDisplayType* mDisplay;
    Lock mLock;
    Condition mCondition;
    Condition mQueueBufCond;

    bool mNeedRealloc;
    bool mCreateDevice;

    std::vector<MMWlDrmBuffer*> mBufferInfo;
    std::queue<MMWlDrmBuffer*> mFreeBuffers; // ready buffers for dequeueBuffer
    std::map<MMNativeBuffer*, int> mBufferToIdxMap;

private:
    MM_DISALLOW_COPY(MMWaylandDrmSurface);
};

}; // end of namespace YUNOS_MM
#endif // end of MM_WAYLAND_DRM_SURFACE
