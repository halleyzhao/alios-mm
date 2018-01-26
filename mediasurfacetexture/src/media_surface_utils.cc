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

#include "media_surface_utils.h"

#include "media_surface_texture.h"

//#include "media_codec.h"
#include "multimedia/mm_debug.h"
#include "multimedia/mm_cpp_utils.h"
#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
#include <NativeWindowBuffer.h>
#endif
#include <stdlib.h>
#include <unistd.h>

MM_LOG_DEFINE_MODULE_NAME("MC-Surface");

namespace YunOSMediaCodec {


#define SURFACE_FUNC_START() \
    int ret = -1;            \
    try {

#define SURFACE_FUNC_END()                                   \
    }                                                        \
    catch (int err) {                                        \
        ERROR("catch expectation from surface 0x%x", err);   \
        flags |= 2048;                                       \
    }                                                        \
    return ret;

#define SURFACE_CHECK()             \
    if (initCheck() == false) {     \
        ERROR("init check fail");   \
        return -1;                  \
    }

int SurfaceTextureWrapper::set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->setBuffersDimensions((int)width, (int)height, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::set_buffers_transform(uint32_t transform, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->setBufferTransform((int)transform, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::set_buffers_format(uint32_t format, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->setBuffersFormat((int)format, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::set_crop(Rect *crop, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    if (!crop)
        return -1;

    int left, top, right, bottom;
    left = (int)crop->left;
    top = (int)crop->top;
    right = (int)crop->right;
    bottom = (int)crop->bottom;

    ret = mSurface->setBufferCrop(left, top, right, bottom, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::set_usage(uint32_t usage, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->setUsage((int)usage, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::query(uint32_t query, int *value, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    // FIXME
    *value = 1;
    ret = 0;

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::set_buffer_count(uint32_t count, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->setBufferCount((int)count, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags) {

    SURFACE_FUNC_START()

    ret = mSurface->dequeueBuffer((YNativeSurfaceBuffer**)buf, NULL, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->cancelBuffer(buf, fd, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->queueBuffer(buf, fd, flags);

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::finishSwap(uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    mSurface->finishSwap();
    ret = 0;

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    /*
    Rect rect;
    rect.top = y;
    rect.left = x;
    rect.right = x + w;
    rect. bottom = y + h;
    */

    int usage = 0;
#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
    usage = ALLOC_USAGE_PREF(SW_READ_OFTEN) | ALLOC_USAGE_PREF(SW_WRITE_OFTEN);
#endif
    mSurface->mapBuffer(buf,
                        usage,
                        x, y, x + w, y + h, ptr);
    ret = 0;

    SURFACE_FUNC_END()
}

int SurfaceTextureWrapper::unmapBuffer(MMNativeBuffer *buf, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    mSurface->unmapBuffer(buf);
    ret = 0;

    SURFACE_FUNC_END()
}

#if !defined(__MM_YUNOS_LINUX_BSP_BUILD__)
int WindowSurfaceWrapper::set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = native_set_buffers_dimensions(
        GET_ANATIVEWINDOW(mSurface),
        width,
        height);

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::set_buffers_format(uint32_t format, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = native_set_buffers_format(
            GET_ANATIVEWINDOW(mSurface),
            format);

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::set_buffers_transform(uint32_t transform, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = native_set_buffers_transform(
            GET_ANATIVEWINDOW(mSurface),
            transform);

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::set_crop(Rect *crop, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    if (!crop)
        return -1;

    ret = native_set_crop(GET_ANATIVEWINDOW(mSurface), crop);

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::set_usage(uint32_t usage, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = native_set_flag(GET_ANATIVEWINDOW(mSurface), usage);

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::query(uint32_t query, int *value, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    *value = 1;
    ret = 0;

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::set_buffer_count(uint32_t count, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = native_set_buffer_count(
            GET_ANATIVEWINDOW(mSurface), count);
    static_cast<yunos::libgui::Surface *>(mSurface)->setMode(false);
    mAsyncMode = true;
#if !defined(YUNOS_BOARD_qemu)
    static_cast<yunos::libgui::Surface *>(mSurface)->setBufferTransformMode(transform_full_fill);
#endif

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags) {

    SURFACE_FUNC_START()

#if __MM_YUNOS_CNTRHAL_BUILD__
    ret = native_obtain_buffer_and_wait(GET_ANATIVEWINDOW(mSurface), buf);
#else
    // yunhal _obtain_buffer_and_wait is not implemented
    int count = 0;
    do {
        ret = native_obtain_buffer_and_wait(GET_ANATIVEWINDOW(mSurface), buf);
        if (ret == 0 && *buf != NULL)
            break;
        usleep(10*1000);
    } while(count++ < 100);
#endif

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    ret = mSurface->dropBuffer((yunos::libgui::BaseNativeSurfaceBuffer*)buf,
                               fd);

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    if (mAsyncMode) {
        static_cast<yunos::libgui::Surface *>(mSurface)->setMode(true);
        mAsyncMode = false;
    }
    ret = mSurface->submitBuffer((yunos::libgui::BaseNativeSurfaceBuffer*)buf,
                                 fd);

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::finishSwap(uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()
    mSurface->finishSwap();
    ret = 0;

    SURFACE_FUNC_END()
}

int WindowSurfaceWrapper::mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()

    Rect rect;
    rect.top = y;
    rect.left = x;
    rect.right = x + w;
    rect. bottom = y + h;
    // should new from buf not cast?
    NativeWindowBuffer *bnb = static_cast<NativeWindowBuffer*>(buf);
    if (bnb)
        bnb->lock(ALLOC_USAGE_PREF(SW_READ_OFTEN) |
                  ALLOC_USAGE_PREF(SW_WRITE_OFTEN),
                  rect, ptr);
    ret = 0;

    SURFACE_FUNC_END()
}


int WindowSurfaceWrapper::unmapBuffer(MMNativeBuffer *buf, uint32_t &flags) {

    SURFACE_CHECK()
    SURFACE_FUNC_START()
    //mSurface->unmapBuffer(buf);
    NativeWindowBuffer *bnb = static_cast<NativeWindowBuffer*>(buf);
    if (bnb)
        bnb->unlock();

    ret = 0;

    SURFACE_FUNC_END()
}
#endif // __MM_BUILD_DRM_SURFACE__ // or __MM_BUILD_VPU_SURFACE__

#ifdef __MM_BUILD_DRM_SURFACE__
WlDrmSurfaceWrapper::WlDrmSurfaceWrapper(YUNOS_MM::MMWaylandDrmSurface *surface) {
    mSurface = surface;
    mDevice = NULL;
}

WlDrmSurfaceWrapper::WlDrmSurfaceWrapper(omap_device *device) {
    mDevice = device;
    mSurface = new YUNOS_MM::MMWaylandDrmSurface(device);
}

WlDrmSurfaceWrapper::~WlDrmSurfaceWrapper() {
    if (mDevice && mSurface)
        delete mSurface;
}

int WlDrmSurfaceWrapper::set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffers_dimensions(width, height);
    return ret;
}

int WlDrmSurfaceWrapper::set_buffers_format(uint32_t format, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffers_format(format);
    return ret;
}

int WlDrmSurfaceWrapper::set_buffers_transform(uint32_t transform, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffers_transform(transform);
    return ret;
}
int WlDrmSurfaceWrapper::set_crop(Rect *crop, uint32_t &flags) {
    SURFACE_CHECK()
    if (!crop)
        return -1;

    int ret = mSurface->set_crop(crop->left, crop->top, crop->right - crop->left, crop->bottom - crop->top);

    return ret;
}

int WlDrmSurfaceWrapper::set_usage(uint32_t usage, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_usage(usage);
    return ret;
}

int WlDrmSurfaceWrapper::query(uint32_t query, int *value, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->query(query, value);
    return ret;
}

int WlDrmSurfaceWrapper::set_buffer_count(uint32_t count, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffer_count(count);
    return ret;
}

int WlDrmSurfaceWrapper::dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->dequeue_buffer_and_wait(buf);
    return ret;
}

int WlDrmSurfaceWrapper::cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->cancel_buffer(buf, fd);
    return ret;
}

int WlDrmSurfaceWrapper::queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->queueBuffer(buf, fd);
    return ret;
}

int WlDrmSurfaceWrapper::finishSwap(uint32_t &flags) {
    SURFACE_CHECK()
    //int ret = mSurface->
    return 0;
}

int WlDrmSurfaceWrapper::mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->mapBuffer(buf, x, y, w, h, ptr);
    return ret;
}

int WlDrmSurfaceWrapper::unmapBuffer(MMNativeBuffer *buf, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->unmapBuffer(buf);
    return ret;
}
#endif // __MM_BUILD_DRM_SURFACE__

#ifdef __MM_BUILD_VPU_SURFACE__
VpuSurfaceWrapper::VpuSurfaceWrapper(YUNOS_MM::MMVpuSurface *surface) {
    mSurface = surface;
}

VpuSurfaceWrapper::VpuSurfaceWrapper() {
    mSurface = new YUNOS_MM::MMVpuSurface();
}

VpuSurfaceWrapper::~VpuSurfaceWrapper() {
    if (mSurface)
        delete mSurface;
}

int VpuSurfaceWrapper::set_buffers_dimensions(uint32_t width, uint32_t height, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffers_dimensions(width, height);
    return ret;
}

int VpuSurfaceWrapper::set_buffers_format(uint32_t format, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffers_format(format);
    return ret;
}

int VpuSurfaceWrapper::set_buffers_transform(uint32_t transform, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffers_transform(transform);
    return ret;
}
int VpuSurfaceWrapper::set_crop(Rect *crop, uint32_t &flags) {
    SURFACE_CHECK()
    if (!crop)
        return -1;

    int ret = mSurface->set_crop(crop->left, crop->top, crop->right - crop->left, crop->bottom - crop->top);

    return ret;
}

int VpuSurfaceWrapper::set_usage(uint32_t usage, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_usage(usage);
    return ret;
}

int VpuSurfaceWrapper::query(uint32_t query, int *value, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->query(query, value);
    return ret;
}

int VpuSurfaceWrapper::set_buffer_count(uint32_t count, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->set_buffer_count(count);
    return ret;
}

int VpuSurfaceWrapper::dequeue_buffer_and_wait(MMNativeBuffer **buf, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->dequeue_buffer_and_wait(buf);
    return ret;
}

int VpuSurfaceWrapper::cancel_buffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->cancel_buffer(buf, fd);
    return ret;
}

int VpuSurfaceWrapper::queueBuffer(MMNativeBuffer *buf, int fd, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->queueBuffer(buf, fd);
    return ret;
}

int VpuSurfaceWrapper::finishSwap(uint32_t &flags) {
    SURFACE_CHECK()
    //int ret = mSurface->
    return 0;
}

int VpuSurfaceWrapper::mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->mapBuffer(buf, x, y, w, h, ptr);
    return ret;
}

int VpuSurfaceWrapper::unmapBuffer(MMNativeBuffer *buf, uint32_t &flags) {
    SURFACE_CHECK()
    int ret = mSurface->unmapBuffer(buf);
    return ret;
}
#endif // __MM_BUILD_VPU_SURFACE__



}   // namespace YunOSMediaCodec
