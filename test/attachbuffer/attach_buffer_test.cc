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
#include <queue>
#include <Surface.h>
#include <cutils/graphics.h>
#include <BufferPipe.h>
#include <PlatformHelper.h>

#include <WaylandConnection.h>
#include "WindowSurfaceTestWindow.h"
// #include <SurfaceController.h>
#include "multimedia/mm_debug.h"
#include "multimedia/mm_cpp_utils.h"


MM_LOG_DEFINE_MODULE_NAME("AttachBufferTest");
#define FUNC_TRACK() FuncTracker tracker(MM_LOG_TAG, __FUNCTION__, __LINE__)
typedef yunos::libgui::BaseNativeSurfaceBuffer MM_NativeBuffer;

static WindowSurface *testSurface[2]; // attach buffers from testSurface[0] to testSurface[1]
static uint32_t surfaceWidth = 640, surfaceHeight = 480;
static const uint32_t surfaceBufferCount = 4;
static const uint32_t colorPattern[3][surfaceBufferCount] = { // BGRA
        { 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff },     // surface 0
        { 0xffff0000, 0xff00ff00, 0x00ffff00, 0x00ff00ff},      // surface 1
        { 0x7f000000, 0x007f0000, 0x00007f00, 0x0000007f },     // directly aclocated NativeWindowBuffer
};
static MM_NativeBuffer* nwbArray[3][surfaceBufferCount];
static std::queue<MM_NativeBuffer*> availNwbs; // buffers owned by app
/*
 * 0: drop/submit buffers of surface 1 to surface 1;
 *      shows surface one correctly
 * 1 drop/submit buffers of surface 0 to surface 1
 *      the first 2 buffer show but with content of surface 1; then hangs
 * 2 drop/submit buffers of surface 0 to surface 1, w/o init surface 1 buffers
 *       weston error when submit the first buffer: "weston: unknown object (7), message submit(?oiiiiii)"
 * 5: use BQ procuder to attachAndPostBuffer() from surface 0 to surface 1; it runs ok!!
 * 6: create buffer by NativeWindowBuffer, then attachAndPostBuffer() to BQ of surface 1
 * ** 1 & 2 with addExternalBufferIfNecessary() for libgui BufferPipeProducer
 */
static uint32_t submitBufferMode = 0;
#define SURFACE_SRC     0
#define SURFACE_DST     1
#define RAW_NWB_SRC     2

class MyProducerListener : public yunos::libgui::IProducerObserver
{
public:
     // Callback when a buffer is released by the server.
    virtual void onBufferReleased(YNativeSurfaceBuffer *buffer) {
        DEBUG("release buffer: %p", buffer);
    }
};

bool configureSurfaces()
{
    FUNC_TRACK();
    uint32_t i=0, j=0;
    int ret = -1;
    static YunAllocator &allocator(YunAllocator::get());
    uint32_t bufferUsage = ALLOC_USAGE_PREF(HW_RENDER) | ALLOC_USAGE_PREF(HW_TEXTURE);
    uint32_t bufferFormat = YUN_HAL_FORMAT_RGBX_8888;

    for (i=0; i<3; i++) {
        if (i != RAW_NWB_SRC) {
            ret = native_set_buffers_dimensions(testSurface[i], surfaceWidth, surfaceHeight);
            ret |= native_set_buffers_format(testSurface[i], bufferFormat);
            ret |= native_set_flag(testSurface[i], bufferUsage);
            ret |= native_set_buffer_count(testSurface[i], surfaceBufferCount);
            ASSERT(ret == 0);
        }

        if (submitBufferMode==2 && i == SURFACE_DST)
            continue; // skip init buffer of dst surface

        // populate all buffers and fill color
        for (j=0; j<surfaceBufferCount; j++) {
            int fencefd;
            // allocate/deque buffer
            if (i == RAW_NWB_SRC) {
                NativeWindowBuffer *wnb = new NativeWindowBuffer(surfaceWidth, surfaceHeight, bufferFormat, bufferUsage);
                int err = wnb->bufInit();
                ASSERT(err == 0);
                nativeBufferIncRef(wnb);
                nwbArray[i][j] = wnb;
            } else {
                ret = testSurface[i]->obtainBuffer((&nwbArray[i][j]), &fencefd);
                ASSERT(ret == 0);
            }
            DEBUG("(%d, %d, %p)", i, j, nwbArray[i][j]);

            // fill buffer with colorPattern
            void* vaddr = NULL;
            pf_buffer_handle_t handle = (pf_buffer_handle_t)mm_getBufferHandle(nwbArray[i][j]);
            ret = allocator.lock(handle, ALLOC_USAGE_PREF(SW_READ_OFTEN) | ALLOC_USAGE_PREF(SW_WRITE_OFTEN),
                0, 0, surfaceWidth, surfaceHeight, &vaddr);

            uint32_t *pixel = (uint32_t)vaddr;
            uint32_t k = 0;
            // fill one line of pixel
            for (k=0; k<surfaceWidth; k++)
                *pixel++ = colorPattern[i][j] * (i == RAW_NWB_SRC ? (k/8%2) : 1);  // make RAW_NWB_SRC vertical color strip
            // copy to other lines
            pixel = (uint32_t)vaddr;
            for (k=1; k<surfaceHeight; k++) {
                pixel += surfaceWidth;
                if (i == SURFACE_SRC && k/8%2) { // make SURFACE_SRC horizontal color strip
                    continue;
                }
                memcpy(pixel, vaddr, surfaceWidth*4);
           }

           ret = allocator.unlock(handle);
        }

    }

    return true;
}

bool attachBufferTest()
{
    FUNC_TRACK();
    const uint32_t frameCount = 10;
    const uint32_t minUnDeque = 2;
    uint32_t frameNum = 0;
    uint32_t i, j;
    int fenceFd = -1, ret = -1;
    uint32_t surfaceBufferIndex = SURFACE_DST;

    if (submitBufferMode == 1 || submitBufferMode == 2 || submitBufferMode == 5)
        surfaceBufferIndex = SURFACE_SRC;
    else if (submitBufferMode == 6)
        surfaceBufferIndex = RAW_NWB_SRC;

    // cancel necessary buffers back to surface
    for (j=0; j<minUnDeque; j++) {
        DEBUG();
        ret = testSurface[1]->dropBuffer(nwbArray[surfaceBufferIndex][j], -1);   // drop buffer to another surface
        ASSERT(ret == 0);
    }

    // add remaining buffer to buffer list
    availNwbs.push(nwbArray[surfaceBufferIndex][2]);
    availNwbs.push(nwbArray[surfaceBufferIndex][3]);

    DEBUG();
    // init variables for attachAndPostBuffer
    uint32_t bufferIndex = minUnDeque;
    std::shared_ptr<yunos::libgui::BufferPipeProducer>  producer = getBQProducer(testSurface[SURFACE_DST]);
    std::shared_ptr<yunos::libgui::IProducerObserver> producerListener(new MyProducerListener());
    producer->connect(producerListener);

    // update buffers to surface
    while(frameNum < frameCount) {
        if (submitBufferMode >= 5) {
            producer->attachAndPostBuffer(nwbArray[surfaceBufferIndex][bufferIndex]);
            bufferIndex++;
            bufferIndex %= surfaceBufferCount;
            // temp ignore releaseBufferCallback, it seems ok since we wait 500ms for each frame
        } else {
            MM_NativeBuffer * nwb = availNwbs.front();
            DEBUG("frameNum: %d, submitBuffer: %p", frameNum, nwb);
            ret = testSurface[SURFACE_DST]->submitBuffer(nwb, -1);
            ASSERT(ret == 0);
            availNwbs.pop();
            DEBUG();
            ret = testSurface[SURFACE_DST]->obtainBuffer((&nwb), &fenceFd);
            ASSERT(ret == 0);
            DEBUG("frameNum: %d, obtainBuffer: %p", frameNum, nwb);
            availNwbs.push(nwb);
        }
        frameNum++;
        usleep(500000);
    }
    return true;
}

int main(int argc, char* const argv[])
{
    FUNC_TRACK();
    uint32_t i = 0;

    if (argc >1)
        submitBufferMode = atoi(argv[1]);
    INFO("submit buffers from one surface to another: %d", submitBufferMode);

    setenv("MM_USE_BQ_SURFACE", "1", 1);
    testSurface[SURFACE_SRC] = createWaylandWindow2(surfaceWidth, surfaceHeight);
    testSurface[SURFACE_DST] = createWaylandWindow2(surfaceWidth, surfaceHeight);
    // SurfaceController* controlB = getSurfaceController(testSurface[1] );
    ((yunos::libgui::Surface*)testSurface[SURFACE_DST]) ->setOffset(300, 300);

    configureSurfaces();
    attachBufferTest();

    destroyWaylandWindow2(testSurface[SURFACE_SRC]);
    destroyWaylandWindow2(testSurface[SURFACE_DST]);

    // seems there is some crash for it
    for (i=0; i< surfaceBufferCount; i++) {
        nativeBufferDecRef(nwbArray[RAW_NWB_SRC][i] );
    }

    return 0;
}

