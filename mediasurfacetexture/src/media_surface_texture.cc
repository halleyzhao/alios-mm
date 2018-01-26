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

#include "media_surface_texture.h"
#include "media_surface_texture_imp.h"

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
//#include <WindowSurface.h>
#endif
#include "multimedia/mm_debug.h"
MM_LOG_DEFINE_MODULE_NAME("MST");

namespace YunOSMediaCodec {

#define PRE_CHECK()    \
    if (!mPriv)        \
        return -1;

MediaSurfaceTexture::MediaSurfaceTexture()
    : mProxy(NULL) {
    mPriv = new MediaSurfaceTextureImp();
}

MediaSurfaceTexture::~MediaSurfaceTexture() {
    DEBUG("~MediaSurfaceTexture");
    delete mPriv;
    DEBUG("~MediaSurfaceTexture done");
}

void MediaSurfaceTexture::returnAcquiredBuffers() {

    if (mProxy)
        mProxy->returnAcquiredBuffers();

    if (mPriv)
        mPriv->returnAcquiredBuffers();
}


int MediaSurfaceTexture::getBufferCount() const {
    PRE_CHECK();

    return mPriv->getBufferCount();
}

int MediaSurfaceTexture::getBuffersFormat() const {
    PRE_CHECK();

    return mPriv->getBuffersFormat();
}

int MediaSurfaceTexture::setBufferCount(int cnt, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->setBufferCount(cnt, flags);
}

int MediaSurfaceTexture::setBufferTransform(int transform, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->setBufferTransform(transform, flags);
}

int MediaSurfaceTexture::setBufferCrop(int left, int top, int right, int bottom, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->setBufferCrop(left, top, right, bottom, flags);
}

int MediaSurfaceTexture::setBuffersFormat(int format, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->setBuffersFormat(format, flags);
}

int MediaSurfaceTexture::setBuffersDimensions(int width, int height, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->setBuffersDimensions(width, height, flags);
}

int MediaSurfaceTexture::mapBuffer(YNativeSurfaceBuffer* buffer,
                                   int usage,
                                   int left, int top, int right, int bottom,
                                   void **ptr) {

    PRE_CHECK();

    Rect rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;

    return mPriv->mapBuffer((MMNativeBuffer*)buffer, usage, rect, ptr);
}

int MediaSurfaceTexture::unmapBuffer(YNativeSurfaceBuffer* buffer) {

    PRE_CHECK();

    return mPriv->unmapBuffer((MMNativeBuffer*)buffer);
}

int MediaSurfaceTexture::dump_buffer_to_file(YNativeSurfaceBuffer *buf) {

    PRE_CHECK();

    return mPriv->dump_buffer_to_file((MMNativeBuffer*)buf);
}

int MediaSurfaceTexture::setUsage(int usage, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->setUsage(usage, flags);
}

void MediaSurfaceTexture::finishSwap() {
    if (!mPriv)
        return;

    return mPriv->finishSwap();
}

int MediaSurfaceTexture::dequeueBuffer(YNativeSurfaceBuffer **buffer, int *fenceFd, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->dequeueBuffer((MMNativeBuffer**)buffer, fenceFd, flags);
}

int MediaSurfaceTexture::queueBuffer(YNativeSurfaceBuffer* buffer, int fenceFd, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->queueBuffer((MMNativeBuffer*)buffer, fenceFd, flags);
}

int MediaSurfaceTexture::cancelBuffer(YNativeSurfaceBuffer* buffer, int fenceFd, uint32_t &flags) {
    PRE_CHECK();

    return mPriv->cancelBuffer((MMNativeBuffer*)buffer, fenceFd, flags);
}

YNativeSurfaceBuffer* MediaSurfaceTexture::acquireBuffer(int index) {
    if (mProxy)
        return mProxy->acquireBuffer(index);

    if (!mPriv)
        return NULL;

    return mPriv->acquireBuffer(index);
}

int MediaSurfaceTexture::setWindowSurface(void* ws) {
    PRE_CHECK();

    // temp workaround
#if 0
    const char* key = "host.media.player.type";
    std::string envStr = mm_get_env_str(key, NULL);
    const char* value = envStr.c_str();
    if (!strncasecmp(value, "proxy", 5)) {
        INFO("setWindowSurface just return");
        return 0;
    }
#endif

    if (mProxy)
        return mProxy->setWindowSurface(ws);

    return mPriv->setWindowSurface(ws);
}

int MediaSurfaceTexture::setSurfaceName(const char* name) {
    PRE_CHECK();

    if (mProxy)
        mProxy->setWindowSurface((void*)name);

    return mPriv->setSurfaceName(name);
}

int MediaSurfaceTexture::setShowFlag(bool show) {
    PRE_CHECK();

    if (mProxy)
        return mProxy->setShowFlag(show);

    return mPriv->setShowFlag(show);
}

bool MediaSurfaceTexture::getShowFlag() {
    if (mProxy)
        return mProxy->getShowFlag();

    if (!mPriv)
        return false;

    return mPriv->getShowFlag();
}

int MediaSurfaceTexture::returnBuffer(YNativeSurfaceBuffer* buffer) {
    PRE_CHECK();

    if (mProxy)
        return mProxy->returnBuffer(buffer);

    return mPriv->returnBuffer((MMNativeBuffer*)buffer);
}

unsigned int MediaSurfaceTexture::width() const {
    PRE_CHECK();

    return mPriv->width();
}

unsigned int MediaSurfaceTexture::height() const {
    PRE_CHECK();

    return mPriv->height();
}

unsigned int MediaSurfaceTexture::format() const {
    PRE_CHECK();

    return mPriv->format();
}

unsigned int MediaSurfaceTexture::transformHint() const {
    PRE_CHECK();

    return mPriv->transformHint();
}

int MediaSurfaceTexture::setListener(SurfaceTextureListener* listener) {
    PRE_CHECK();

    if (mProxy)
        return mProxy->setListener(listener);

    return mPriv->setListener(listener);
}

bool MediaSurfaceTexture::listenerExisted() {
    if (!mPriv)
        return false;
    return mPriv->listenerExisted();
}

void* MediaSurfaceTexture::getSurfaceName() {
    if (!mPriv)
        return NULL;

    return mPriv->getSurface();
}

} // end of namespace YunOSMediaCodec
