/*
 *  egl_util.c - help utility for egl
 *
 *  Copyright (C) 2014 Intel Corporation
 *    Author: Zhao, Halley<halley.zhao@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "egl_util.h"
// #include "common/log.h"
#if __ENABLE_DMABUF__
#include "libdrm/drm_fourcc.h"
#endif

static PFNEGLCREATEIMAGEKHRPROC createImageProc = NULL;
static PFNEGLDESTROYIMAGEKHRPROC destroyImageProc = NULL;
#ifndef PRINT_MARK
#define PRINT_MARK fprintf(stderr, "%s, %s, %d\n", __FILE__, __func__, __LINE__ )
#endif
#ifndef ERROR
#define ERROR(format, ...)  fprintf(stderr, format, ##__VA_ARGS__)
#endif
#ifndef INFO
#define INFO(format, ...)  fprintf(stderr, format, ##__VA_ARGS__)
#endif
#ifndef DEBUG
#define DEBUG(format, ...)  fprintf(stderr, format, ##__VA_ARGS__)
#endif
#ifndef ASSERT
#include <assert.h>
#define ASSERT  assert
#endif

EGLImageKHR createImage(EGLDisplay dpy, EGLContext ctx, EGLenum target,
                        EGLClientBuffer buffer, const EGLint *attrib_list)
{
    if (!createImageProc) {
        createImageProc = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    }
    return createImageProc(dpy, ctx, target, buffer, attrib_list);
}

EGLBoolean destroyImage(EGLDisplay dpy, EGLImageKHR image)
{
    if (!destroyImageProc) {
        destroyImageProc = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    }
    return destroyImageProc(dpy, image);
}

static EGLImageKHR createEglImageFromDrmBuffer(EGLDisplay eglDisplay, EGLContext eglContext, uint32_t drmName, int width, int height, int pitch)
{
    EGLImageKHR eglImage = EGL_NO_IMAGE_KHR;
    EGLint attribs[] = {
      EGL_WIDTH, width,
      EGL_HEIGHT, height,
      EGL_DRM_BUFFER_STRIDE_MESA, pitch/4,
      EGL_DRM_BUFFER_FORMAT_MESA,
      EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
      EGL_DRM_BUFFER_USE_MESA,
      EGL_DRM_BUFFER_USE_SHARE_MESA,
      EGL_NONE
    };

    DEBUG("createEglImageFromDrmBuffer: width: %d, height: %d, pitch: %d, drmName: %d\n", width, height, pitch, drmName);
    eglImage = createImage(eglDisplay, eglContext, EGL_DRM_BUFFER_MESA,
                     (EGLClientBuffer)(intptr_t)drmName, attribs);
    return eglImage;
}

static EGLImageKHR createEglImageFromDmaBuf(EGLDisplay eglDisplay, EGLContext eglContext, uint32_t dmaBuf, int width, int height, int pitch)
{
    EGLImageKHR eglImage = EGL_NO_IMAGE_KHR;
#if __ENABLE_DMABUF__
    EGLint attribs[] = {
      EGL_WIDTH, width,
      EGL_HEIGHT, height,
      EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XRGB8888,
      EGL_DMA_BUF_PLANE0_FD_EXT, dmaBuf,
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
      EGL_DMA_BUF_PLANE0_PITCH_EXT, pitch,
      EGL_NONE
    };

    eglImage = createImage(eglDisplay, EGL_NO_CONTEXT,
                     EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
    ASSERT(eglImage != EGL_NO_IMAGE_KHR);
    return eglImage;
#else
    ERROR("dma_buf is enabled with --enable-dmabuf option");
    return eglImage;
#endif
}

EGLImageKHR createEglImageFromHandle(EGLDisplay eglDisplay, EGLContext eglContext, /*VideoDataMemoryType type,*/ uint32_t handle, int width, int height, int pitch)
{
    EGLImageKHR eglImage = EGL_NO_IMAGE_KHR;
    if (1) // (type == VIDEO_DATA_MEMORY_TYPE_DRM_NAME)
        eglImage = createEglImageFromDrmBuffer(eglDisplay, eglContext, handle, width, height, pitch);
    else // if (type == VIDEO_DATA_MEMORY_TYPE_DMA_BUF)
        eglImage = createEglImageFromDmaBuf(eglDisplay, eglContext, handle, width, height, pitch);

    return eglImage;
}
