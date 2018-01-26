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

#ifndef MEDIA_SURFACE_UTILS_H_
#define MEDIA_SURFACE_UTILS_H_

#include "multimedia/mm_surface_compat.h"
#include <stdio.h>

namespace YunOSMediaCodec {

class MediaSurfaceTexture;

class SurfaceWrapper {

public:
    SurfaceWrapper() {}
    virtual ~SurfaceWrapper() {}

    virtual bool initCheck() = 0;
    virtual int set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags) = 0;
    virtual int set_buffers_format(uint32_t format, uint32_t &flags) = 0;
    virtual int set_buffers_transform(uint32_t transform, uint32_t &flags) = 0;
    virtual int set_crop(Rect *crop, uint32_t &flags) = 0;
    virtual int set_usage(uint32_t usage, uint32_t &flags) = 0;
    virtual int query(uint32_t query, int *value, uint32_t &flags) = 0;
    virtual int set_buffer_count(uint32_t count, uint32_t &flags) = 0;
    virtual int dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags) = 0;
    virtual int cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags) = 0;
    //virtual int free_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags) = 0;
    virtual int queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags) = 0;
    virtual int finishSwap(uint32_t &flags) = 0;
    virtual int mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags) = 0;
    virtual int unmapBuffer(MMNativeBuffer *buf, uint32_t &flags) = 0;
};

#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
class WindowSurfaceWrapper : public SurfaceWrapper {

public:
    explicit WindowSurfaceWrapper(WindowSurface *surface) {
        mSurface = surface;
        mAsyncMode = false;
    }

    virtual ~WindowSurfaceWrapper() {}

    virtual bool initCheck() { return (mSurface != NULL); }

    virtual int set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags);
    virtual int set_buffers_format(uint32_t format, uint32_t &flags);
    virtual int set_buffers_transform(uint32_t transform, uint32_t &flags);
    virtual int set_crop(Rect *crop, uint32_t &flags);
    virtual int set_usage(uint32_t usage, uint32_t &flags);
    virtual int query(uint32_t query, int *value, uint32_t &flags);
    virtual int set_buffer_count(uint32_t count, uint32_t &flags);
    virtual int dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags);
    virtual int cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    //virtual int free_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int finishSwap(uint32_t &flags);
    virtual int mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags);
    virtual int unmapBuffer(MMNativeBuffer *buf, uint32_t &flags);

private:
    WindowSurface *mSurface;
    bool mAsyncMode;
};
#endif

class SurfaceTextureWrapper : public SurfaceWrapper {

public:
    explicit SurfaceTextureWrapper(MediaSurfaceTexture *surface) { mSurface = surface; }
    virtual ~SurfaceTextureWrapper() {}

    virtual bool initCheck() { return (mSurface != NULL); }

    virtual int set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags);
    virtual int set_buffers_format(uint32_t format, uint32_t &flags);
    virtual int set_buffers_transform(uint32_t transform, uint32_t &flags);
    virtual int set_crop(Rect *crop, uint32_t &flags);
    virtual int set_usage(uint32_t usage, uint32_t &flags);
    virtual int query(uint32_t query, int *value, uint32_t &flags);
    virtual int set_buffer_count(uint32_t count, uint32_t &flags);
    virtual int dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags);
    virtual int cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    //virtual int free_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int finishSwap(uint32_t &flags);
    virtual int mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags);
    virtual int unmapBuffer(MMNativeBuffer *buf, uint32_t &flags);

private:
    MediaSurfaceTexture *mSurface;
};

}   // namespace YunOSMediaCodec

#ifdef __MM_BUILD_DRM_SURFACE__
#include <mm_wayland_drm_surface.h>
namespace YUNOS_MM {
class MMWaylandDrmSurface;
};
struct omap_device;

namespace YunOSMediaCodec {
class WlDrmSurfaceWrapper : public SurfaceWrapper {
public:
    explicit WlDrmSurfaceWrapper(YUNOS_MM::MMWaylandDrmSurface *surface);
    explicit WlDrmSurfaceWrapper(omap_device *device);
    virtual ~WlDrmSurfaceWrapper();

    virtual bool initCheck() { return (mSurface != NULL); }

    virtual int set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags);
    virtual int set_buffers_format(uint32_t format, uint32_t &flags);
    virtual int set_buffers_transform(uint32_t transform, uint32_t &flags);
    virtual int set_crop(Rect *crop, uint32_t &flags);
    virtual int set_usage(uint32_t usage, uint32_t &flags);
    virtual int query(uint32_t query, int *value, uint32_t &flags);
    virtual int set_buffer_count(uint32_t count, uint32_t &flags);
    virtual int dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags);
    virtual int cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    //virtual int free_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int finishSwap(uint32_t &flags);
    virtual int mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags);
    virtual int unmapBuffer(MMNativeBuffer *buf, uint32_t &flags);

private:
    YUNOS_MM::MMWaylandDrmSurface *mSurface;
    omap_device *mDevice;
};
}   // namespace YunOSMediaCodec
#endif

#ifdef __MM_BUILD_VPU_SURFACE__
#include <mm_vpu_surface.h>
namespace YUNOS_MM {
class MMVpuSurface;
};

namespace YunOSMediaCodec {
class VpuSurfaceWrapper : public SurfaceWrapper {
public:
    explicit VpuSurfaceWrapper(YUNOS_MM::MMVpuSurface *surface);
    explicit VpuSurfaceWrapper();
    virtual ~VpuSurfaceWrapper();

    virtual bool initCheck() { return (mSurface != NULL); }

    virtual int set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags);
    virtual int set_buffers_format(uint32_t format, uint32_t &flags);
    virtual int set_buffers_transform(uint32_t transform, uint32_t &flags);
    virtual int set_crop(Rect *crop, uint32_t &flags);
    virtual int set_usage(uint32_t usage, uint32_t &flags);
    virtual int query(uint32_t query, int *value, uint32_t &flags);
    virtual int set_buffer_count(uint32_t count, uint32_t &flags);
    virtual int dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags);
    virtual int cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    //virtual int free_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags);
    virtual int finishSwap(uint32_t &flags);
    virtual int mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags);
    virtual int unmapBuffer(MMNativeBuffer *buf, uint32_t &flags);

private:
    YUNOS_MM::MMVpuSurface *mSurface;
};
}   // namespace YunOSMediaCodec
#endif

#endif  // MEDIA_SURFACE_UTILS_H_
