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

#ifndef __media_surface_texture_h
#define __media_surface_texture_h

#include <stdint.h>

#if defined(__MM_YUNOS_LINUX_BSP_BUILD__)
typedef void YNativeSurfaceBuffer;
#else
#include <ywindow.h>
#endif

namespace YUNOS_MM {
class MediaPlayerClient;
}

namespace YunOSMediaCodec {

class MediaSurfaceTextureImp;

class SurfaceTextureListener {
public:
    SurfaceTextureListener(){}
    virtual ~SurfaceTextureListener(){}

public:
    virtual void onMessage(int msg, int param1, int param2) = 0;

private:
    SurfaceTextureListener(const SurfaceTextureListener&);
    SurfaceTextureListener & operator=(const SurfaceTextureListener &);
};

class MediaSurfaceTexture {

public:
    MediaSurfaceTexture();
    virtual ~MediaSurfaceTexture();

    //Warnig: Client should return the acquired buffers, otherwise it will case to hungup when exit
    //        Client can call "returnAcquiredBuffers" to return all the buffers acquired which owned by client.
    void returnAcquiredBuffers();

    int getBufferCount() const;
    int getBuffersFormat() const;

    int setBufferCount(int cnt, uint32_t &flags);
    int setBufferTransform(int transform, uint32_t &flags);
    int setBufferCrop(int left, int top, int right, int bottom, uint32_t &flags);
    //int setOffset(int x, int y);
    int setBuffersFormat(int format, uint32_t &flags);
    int setBuffersDimensions(int width, int height, uint32_t &flags);
    int setUsage(int usage, uint32_t &flags);

    // TODO make sure the buffer has been consumed?
    void finishSwap();

    int mapBuffer(YNativeSurfaceBuffer* buffer, int usage, int left, int top, int right, int bottom, void **ptr);
    int unmapBuffer(YNativeSurfaceBuffer* buffer);
    int dump_buffer_to_file(YNativeSurfaceBuffer *buf);

    int dequeueBuffer(YNativeSurfaceBuffer **buffer, int *fenceFd, uint32_t &flags);
    //int lockBuffer(YNativeSurfaceBuffer* buffer);
    int queueBuffer(YNativeSurfaceBuffer* buffer, int fenceFd, uint32_t &flags);
    int cancelBuffer(YNativeSurfaceBuffer* buffer, int fenceFd, uint32_t &flags);
    int freeBuffer(YNativeSurfaceBuffer *buf, int fd, uint32_t &flags);

    unsigned int width() const;
    unsigned int height() const;
    unsigned int format() const;
    unsigned int transformHint() const;

    //int setBuffersGeometry(int width, int height, int format);

    YNativeSurfaceBuffer* acquireBuffer(int index);
    int returnBuffer(YNativeSurfaceBuffer* anb);

    int setWindowSurface(void *ws);
    int setSurfaceName(const char *name);
    int setShowFlag(bool show);
    bool getShowFlag();

    int setListener(SurfaceTextureListener* listener);
    bool listenerExisted();

friend class YUNOS_MM::MediaPlayerClient;

    class ConsumerProxy {
    public:
        ConsumerProxy() {}
        virtual ~ConsumerProxy() {}

        virtual void returnAcquiredBuffers() = 0;
        virtual YNativeSurfaceBuffer* acquireBuffer(int index) = 0;
        virtual int returnBuffer(YNativeSurfaceBuffer* anb) = 0;

        virtual int setWindowSurface(void *ws) = 0;
        virtual int setShowFlag(bool show) = 0;
        virtual bool getShowFlag() = 0;
        virtual int setListener(SurfaceTextureListener* listener);

    private:
        ConsumerProxy(const ConsumerProxy &);
        ConsumerProxy & operator=(const ConsumerProxy &);
    };

private:
    void setConsumerProxy(ConsumerProxy* proxy) { mProxy = proxy; }
    void* getSurfaceName();

private:
    MediaSurfaceTextureImp *mPriv;
    ConsumerProxy *mProxy;
};

} // end of namespace YunOSMediaCodec
#endif //__media_surface_texture_h
