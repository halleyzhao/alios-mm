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
#include <assert.h>

#include <mm_wayland_drm_surface.h>
#include <multimedia/mm_debug.h>

#include <wayland-client.h>
#include <wayland-drm-client-protocol.h>
#include <scaler-client-protocol.h>

#include <libdce.h>

#include <xf86drm.h>
#include <omap_drm.h>
#include <omap_drmif.h>

#include <DrmAllocator.h>

// 32/24 are Padding Amounts from TI <<H264_Decoder_HDVICP2_UserGuide>>
#define PADDING_AMOUNT_X    32
#define PADDING_AMOUNT_Y    24

#define FOURCC(ch0, ch1, ch2, ch3) \
        ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
         ((uint32_t)(uint8_t)(ch2) << 16)  | ((uint32_t)(uint8_t)(ch3) << 24))

#define ALIGN2(x, n)   (((x) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

#define PADX_H264   32
#define PADY_H264   24
#ifndef PAGE_SHIFT
#  define PAGE_SHIFT 12
#endif
#define PADX_MPEG4  32
#define PADY_MPEG4  32


#define MAX_BUFFER_COUNT 64

#define NEED_REALLOC_CHECK4(comp1, comp2, comp3, comp4, val1, val2, val3, val4) \
    if (comp1 != val1)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp2 != val2)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp3 != val3)                                                          \
        mNeedRealloc = true;                                                    \
    if (comp4 != val4)                                                          \
        mNeedRealloc = true;                                                    \
    if (!mBufferInfo.empty() && mNeedRealloc)                                   \
        INFO("configure changes, will re-allocate buffers");

#define NEED_REALLOC_CHECK2(comp1, comp2, val1, val2) \
    NEED_REALLOC_CHECK4(comp1, comp2, 0, 0, val1, val2, 0, 0)

#define NEED_REALLOC_CHECK1(comp1, val1) \
    NEED_REALLOC_CHECK4(comp1, 0, 0, 0, val1, 0, 0, 0)

MM_LOG_DEFINE_MODULE_NAME("WlDrmSurf");

namespace YUNOS_MM
{

#define FUNC_TRACK() FuncTracker tracker(MM_LOG_TAG, __FUNCTION__, __LINE__)
// #define FUNC_TRACK()

// debug use only
static uint32_t s_ReleaseBufCount = 0;
static uint32_t s_FrameCBCount = 0;

class WlDisplayType
{
  public:
    WlDisplayType();
    ~WlDisplayType();
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct wl_shell *shell;
    struct wl_drm *drm;
    struct wl_viewport *viewport;
    struct wl_scaler *scaler;
    struct wl_callback *callback;
    pthread_t threadID;
    uint32_t threadStatus;  // 0: normal running, 1: main thread inform to quit, 2: event thread quit done
};
WlDisplayType::WlDisplayType()
    : display(NULL)
    , compositor(NULL)
    , surface(NULL)
    , shell(NULL)
    , drm(NULL)
    , callback(NULL)
{
    memset(&threadID, 0, sizeof(threadID));
}

/////////////////////////////////wayland helper func /////////////////////////////////////////////////
static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
    FUNC_TRACK();
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
    sync_callback,
    NULL
};

// schedule a dummy event (display sync callback) to unblock wl_display_dispatch()
static bool wl_display_dummy_event(struct wl_display *display)
{
    FUNC_TRACK();
    struct wl_callback *callback = wl_display_sync(display);
    if (callback == NULL)
        return false;

    wl_callback_add_listener(callback, &sync_listener, NULL);
    wl_callback_request_done(callback);
    wl_display_flush(display);
    return true;
}

void* wayland_event_dispatch_task(void *arg)
{
    FUNC_TRACK();
    int ret;
    WlDisplayType* wlDisplay = (WlDisplayType*) arg;

    while(wlDisplay->threadStatus == 0) {
        ret = wl_display_dispatch(wlDisplay->display);
        VERBOSE("wayland_process_events ret: %d", ret);
    }
    wlDisplay->threadStatus = 2;

    return NULL;
}

static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t id,
    const char *interface, uint32_t version)
{
    FUNC_TRACK();
   WlDisplayType *wlDisplay = (WlDisplayType *) data;

    if (strcmp(interface, "wl_compositor") == 0) {
        wlDisplay->compositor = (struct wl_compositor*)wl_registry_bind(registry, id,
            &wl_compositor_interface, 3);
    } else if (strcmp(interface, "wl_shell") == 0) {
        wlDisplay->shell = (struct wl_shell*)wl_registry_bind(registry, id,
            &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_drm") == 0) {
        wlDisplay->drm = (struct wl_drm*)wl_registry_bind(registry, id,
            &wl_drm_interface, 1);
    } else if (strcmp(interface, "wl_scaler") == 0) {
        wlDisplay->scaler = (struct wl_scaler*)wl_registry_bind(registry, id,
            &wl_scaler_interface, 2);
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
        uint32_t name)
{
    FUNC_TRACK();
}
static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

WlDisplayType *
disp_wayland_open(struct wl_display *display, uint32_t width, uint32_t height)
{
    FUNC_TRACK();
    WlDisplayType *wlDisplay = NULL;
    struct wl_registry *registry = NULL;
    struct wl_shell_surface *shell_surface = NULL;
    int ret;

    wlDisplay = (WlDisplayType*)calloc(1, sizeof(WlDisplayType));
    // xxx, nested string token, or use app set display handle
    //wlDisplay->display = wl_display_connect(displayName);
    wlDisplay->display = display;
    if (wlDisplay->display == NULL) {
        ERROR("failed to connect to Wayland display: %m\n");
        return NULL;
    } else {
        INFO("wayland display opened\n");
    }

    /* Find out what interfaces are implemented, and initialize */
    registry = wl_display_get_registry(wlDisplay->display);
    wl_registry_add_listener(registry, &registry_listener, wlDisplay);
    wl_display_roundtrip(wlDisplay->display);
    INFO("wayland registries obtained\n");

    wlDisplay->surface = wl_compositor_create_surface(wlDisplay->compositor);
    shell_surface = wl_shell_get_shell_surface(wlDisplay->shell,
                        wlDisplay->surface);
    wl_shell_surface_set_toplevel(shell_surface);
#if 0
    struct wl_region *region = NULL;
    region = wl_compositor_create_region(wlDisplay->compositor);
    wl_region_add(region, 0, 0, width, height);
#endif
    wlDisplay->viewport = wl_scaler_get_viewport(wlDisplay->scaler,
                        wlDisplay->surface);
    wl_viewport_set_source(wlDisplay->viewport, wl_fixed_from_int(PADDING_AMOUNT_X), wl_fixed_from_int(PADDING_AMOUNT_Y), wl_fixed_from_int(width), wl_fixed_from_int(height));
    int win_w = width;
    int win_h = height;
    if (win_w > MM_SCREEN_WIDTH) {
        win_w = MM_SCREEN_WIDTH;
        win_h = (int)(MM_SCREEN_WIDTH * 1.0f * height / width);
    }
    if (win_h > MM_SCREEN_HEIGHT) {
        int tmp = win_h;
        win_h = MM_SCREEN_HEIGHT;
        win_w = (int)(MM_SCREEN_HEIGHT * 1.0f * win_w) / tmp;
    }
    wl_viewport_set_destination(wlDisplay->viewport, win_w, win_h);
    DEBUG("set surface viewport: %d x %d", width, height);
    DEBUG("set surface dest: %d x %d", win_w, win_h);

    ret = pthread_create(&wlDisplay->threadID, NULL, wayland_event_dispatch_task, wlDisplay);
    if(ret) {
        INFO("could not create task for wayland event processing");
    }

    return wlDisplay;
}

/* after frame_callback_listener callback, we can submit another wl_buffer
 * queeuBuffer should check the callback
 */
void frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    FUNC_TRACK();
    DEBUG("s_FrameCBCount: %d, callback: %p", s_FrameCBCount, callback);
    s_FrameCBCount++;

    MMWaylandDrmSurface *tmp = (MMWaylandDrmSurface*) data;
    WlDisplayType *wlDisplay = tmp->mDisplay;
    YUNOS_MM::MMAutoLock locker(tmp->mLock);

    if (callback == wlDisplay->callback) {
        wl_callback_destroy(wlDisplay->callback);
        wlDisplay->callback = NULL;
        tmp->mQueueBufCond.signal();
    }
}
const struct wl_callback_listener frame_listener = { .done = frame_callback };

/* after buffer_release_callback, we can release wl_buffer (and its corresponding MediaBuffer)
 * it avoids  flicker: wayland server is still using the buffer but video driver updates the frame
 */
void buffer_release_callback (void *data, struct wl_buffer *wlBuffer) {
    DEBUG("s_ReleaseBufCount: %d, wl_buffer: %p", s_ReleaseBufCount, wlBuffer);
    s_ReleaseBufCount++;

    if (!data || !wlBuffer) {
        ERROR("invalid data or wlBuffer");
        return;
    }

    MMWaylandDrmSurface *tmp = (MMWaylandDrmSurface*) data;
    //MMWaylandDrmSurface::MMWlDrmBuffer map = &tmp->mBufferInfo;
    std::vector<MMWlDrmBuffer*> &map = tmp->mBufferInfo;
    MMAutoLock locker(tmp->mLock);
     int i = 0;
    for (i=0; i < (int)map.size(); i++) {
        if (map[i]->wl_buf == wlBuffer) {
            // v4l2codec and surface don't check dec buffer free, v4l2 device should do it
            map[i]->usedByDisplay = false;
            tmp->mFreeBuffers.push(map[i]);
            tmp->mCondition.signal();
            break;
        }
    }
}

static const struct wl_buffer_listener frame_buffer_listener = {
    buffer_release_callback
};

MMWaylandDrmSurface::MMWaylandDrmSurface(void *device)
    : mBufferCnt(0),
      mFormat(0),
      mUsage(0),
      mWidth(0),
      mHeight(0),
      mPaddedWidth(0),
      mPaddedHeight(0),
      mTransform(0),
      mCropX(0),
      mCropY(0),
      mCropW(0),
      mCropH(0),
      mHwPadX(PADX_H264),
      mHwPadY(PADY_H264),
      mDrmAlloc(NULL),
      mDisplay(NULL),
      mCondition(mLock),
      mQueueBufCond(mLock),
      mNeedRealloc(false),
      mCreateDevice(false) {
    FUNC_TRACK();

    mDevice = static_cast<omap_device*>(device);

    /* get drm auth fd before codec init */
    mWlDisplay = wl_display_connect(NULL);

    if (!mDevice) {
        int fd = dce_get_fd();
        INFO("dce drm fd %d", fd);
        if (fd < 0) {
            mDrmAlloc = new yunos::DrmAllocator(mWlDisplay);

            fd = mDrmAlloc->getDrmDevice();

            INFO("get auth drm fd %d", fd);
            dce_set_fd(fd);
        }

        mDevice = (struct omap_device*)dce_init();
        mCreateDevice = true;

        fd = dce_get_fd();
        INFO("create omap device on our own with drm fd %d", fd);
    }
}

MMWaylandDrmSurface::~MMWaylandDrmSurface() {
    FUNC_TRACK();
    destroyWaylandDrmSurface();

    if (mCreateDevice && mDevice)
        dce_deinit(mDevice);

    if (mDrmAlloc)
        delete mDrmAlloc;

    if (mWlDisplay == NULL)
        wl_display_disconnect(mDisplay->display);

}

int MMWaylandDrmSurface::createWaylandDrmSurface() {
    FUNC_TRACK();

    if (mDisplay)
        return 0;

    mDisplay = disp_wayland_open(mWlDisplay, mWidth, mHeight);

    return 0;
}

void MMWaylandDrmSurface::destroyWaylandDrmSurface() {
    FUNC_TRACK();

    if (!mDisplay)
        return;

    // terminate wayland event dispatch thread
    mDisplay->threadStatus = 1;
    void * aaa = NULL;
    wl_display_dummy_event(mDisplay->display);
    pthread_join(mDisplay->threadID, &aaa);

    // destroy wl_buffer
    {
        MMAutoLock lock(mLock);
        size_t size = 0;
        while (size < mBufferInfo.size()) {
            destroyNativeBuffer((int)size);
            delete mBufferInfo[size];
            size++;
        }
        mBufferInfo.clear();
    }

    // destroy wl_surface and disconnect wl_display
    wl_surface_destroy(mDisplay->surface);
    wl_display_disconnect(mDisplay->display);
    free(mDisplay);
    mWlDisplay = NULL;
    mDisplay = NULL;
}

/////////////////////////////////////// ducati mem helper func
static struct omap_bo *
alloc_bo(struct omap_device* device, uint32_t bpp, uint32_t width, uint32_t height,
		uint32_t *bo_handle, uint32_t *pitch)
{
    FUNC_TRACK();
    struct omap_bo *bo;
    uint32_t bo_flags = OMAP_BO_SCANOUT | OMAP_BO_WC;

    // the width/height has been padded already
    bo = omap_bo_new(device, width * height * bpp / 8, bo_flags);

    if (bo) {
    	*bo_handle = omap_bo_handle(bo);
    	*pitch = width * bpp / 8;
    	if (bo_flags & OMAP_BO_TILED)
    		*pitch = ALIGN2(*pitch, PAGE_SHIFT);
    }

    return bo;
}

bool MMWaylandDrmSurface::createNativeBuffer() {
    FUNC_TRACK();

    uint32_t bo_handles[4] = {0};
    uint32_t wl_fmt = WL_DRM_FORMAT_NV12;

    MMWlDrmBuffer *outBuf = new MMWlDrmBuffer;
    memset(outBuf, 0, sizeof(MMWlDrmBuffer));

    outBuf->width = mPaddedWidth;
    outBuf->height = mPaddedHeight;
    outBuf->multiBo = false;

    setBufferCrop_l(outBuf);

    if (!mFormat)
        mFormat = FOURCC('N','V','1','2');
    else if (mFormat != FOURCC('N','V','1','2')) {
        WARNING("format 0x%x is not supported", mFormat);
        return false;
    }

    outBuf->fourcc = mFormat;

    switch (outBuf->fourcc) {
    case FOURCC('N','V','1','2'):
            outBuf->bo[0] = alloc_bo(mDevice, 8, outBuf->width, outBuf->height * 3 / 2,
                bo_handles, (uint32_t*)(outBuf->pitches));
            outBuf->offsets[0] = 0;
            outBuf->offsets[1] = outBuf->pitches[0] * outBuf->height;
            outBuf->pitches[1] = outBuf->pitches[0];
            omap_bo_get_name(outBuf->bo[0], &outBuf->drmName);
            outBuf->fd[0] = omap_bo_dmabuf(outBuf->bo[0]);
            outBuf->size = (int)omap_bo_size(outBuf->bo[0]);
            wl_fmt = WL_DRM_FORMAT_NV12;
            break;
    default:
            ERROR("invalid format: 0x%08x", outBuf->fourcc);
            return false;
    }

    // create wl_buffer
    int32_t drmName = outBuf->drmName;
    struct wl_buffer* wl_buf = NULL;

    DEBUG("buffer num: %d-%d, drmName: %d, fd[0] %d, width: %d, height: %d, offsets[0]: %d, ,strides[0]: %d, offsets[1]: %d, strides[1]: %d",
        mBufferCnt, (int)mBufferInfo.size(), drmName, outBuf->fd[0], mWidth, mHeight,
        outBuf->offsets[0], outBuf->pitches[0],
        outBuf->offsets[1], outBuf->pitches[1]);

    // wl_buffer is created with padded resolution, then uses mWidth/mHeight to crop it (wl_viewport_set_source/wl_viewport_set_destination)
    int32_t w = outBuf->pitches[0];
    int32_t h = outBuf->offsets[1]/w;
    wl_buf = wl_drm_create_planar_buffer(mDisplay->drm,
        drmName, w, h, wl_fmt,
        outBuf->offsets[0], outBuf->pitches[0],
        outBuf->offsets[1], outBuf->pitches[1],
        0, 0);

    INFO("wl_buf is %p", wl_buf);
    outBuf->wl_buf = wl_buf;

    wl_buffer_add_listener (wl_buf, &frame_buffer_listener, this);

    mBufferInfo.push_back(outBuf);

    return true;
}

void MMWaylandDrmSurface::destroyNativeBuffer(int i) {
    FUNC_TRACK();

    if (i < 0 || i >= (int)mBufferInfo.size() || !mBufferInfo[i]) {
        ERROR("invalid buffer index %d", i);
        return;
    }

    MMWlDrmBuffer *info = mBufferInfo[i];

    if (info->wl_buf) {
        wl_buffer_destroy(info->wl_buf);
        info->wl_buf = NULL;
    }

    close(info->fd[0]);
    omap_bo_del(info->bo[0]);
    info->fd[0] = -1;
    info->bo[0] = NULL;

    if (info->multiBo) {
        close(info->fd[1]);
        omap_bo_del(info->bo[1]);
        info->fd[1] = -1;
        info->bo[1] = NULL;
    }
}

int MMWaylandDrmSurface::set_buffers_dimensions(uint32_t width, uint32_t height) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK2(mWidth, mHeight, width, height)

    mWidth = width;
    mHeight = height;


    mPaddedWidth  = ALIGN2(mWidth + (2 * PADX_H264), 7);
    mPaddedHeight = mHeight + 4 * PADY_H264;
    INFO("get %dx%d after padding %dx%d", mWidth, mHeight, mPaddedWidth, mPaddedHeight);

    mCropX = 0;
    mCropY = 0;
    mCropW = width;
    mCropH = height;

    return 0;
}

int MMWaylandDrmSurface::set_buffers_format(uint32_t format) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK1(mFormat, format)

    INFO("set format %x", format);
    mFormat = format;

    return 0;
}

int MMWaylandDrmSurface::set_buffers_transform(uint32_t transform) {
    FUNC_TRACK();

    INFO("set transform rotation %d", transform);
    mTransform = transform;

    return 0;
}

int MMWaylandDrmSurface::set_crop(int x, int y, int w, int h) {
    FUNC_TRACK();

    INFO("set crop %d %d %d %d", x, y, w, h);

    mCropX = x;
    mCropY = y;
    mCropW = w;
    mCropH = h;

    return 0;
}

int MMWaylandDrmSurface::set_usage(uint32_t usage) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK1(mUsage, usage)

    INFO("set usage %x", usage);
    mUsage = usage;

    if (mUsage & DRM_NAME_USAGE_TI_HW_ENCODE) {
        mPaddedWidth  = mWidth;
        mPaddedHeight = mHeight;
    } else if (mUsage & DRM_NAME_USAGE_HW_DECODE_PAD_H264 ||
        mUsage & DRM_NAME_USAGE_HW_DECODE_PAD_VC1) {
    } else if (mUsage & DRM_NAME_USAGE_HW_DECODE_PAD_MPEG4) {
        mPaddedWidth  = ALIGN2(mWidth + PADX_MPEG4, 7);
        mPaddedHeight = mHeight + PADY_MPEG4;
    } else if (mUsage & DRM_NAME_USAGE_HW_DECODE_PAD_MPEG2) {
        mPaddedWidth  = mWidth;
        mPaddedHeight = mHeight;
    }

    INFO("get %dx%d after padding %dx%d", mWidth, mHeight, mPaddedWidth, mPaddedHeight);
    return 0;
}

int MMWaylandDrmSurface::query(uint32_t query, int *value) {
    FUNC_TRACK();
    ASSERT(value);
    switch (query) {
        case QUERY_GET_X_STRIDE:
            *value = mPaddedWidth;
            break;
        case QUERY_GET_Y_STRIDE:
            *value = mPaddedHeight;
            break;
    }

    return 0;
}

int MMWaylandDrmSurface::set_buffer_count(uint32_t count) {
    FUNC_TRACK();
    NEED_REALLOC_CHECK1(mBufferCnt, count)

    mBufferCnt = count;

    return 0;
}

int MMWaylandDrmSurface::dequeue_buffer_and_wait(MMNativeBuffer **buffer) {
    FUNC_TRACK();
    MMAutoLock lock(mLock);

    MMWlDrmBuffer *info;
    *buffer = NULL;

    if (mBufferCnt < 0 || mBufferCnt > MAX_BUFFER_COUNT) {
        ERROR("invalid buffer count %d", mBufferCnt);
        return -1;
    }

    if (mNeedRealloc) {
        INFO("need realloc buffers");
        mNeedRealloc = false;

        createWaylandDrmSurface();

        size_t i = 0;
        for (i = 0; i < mBufferInfo.size(); i++) {
            destroyNativeBuffer(i);
            delete mBufferInfo[i];
        }
        mBufferInfo.clear();
        mBufferToIdxMap.clear();

        while(!mFreeBuffers.empty())
            mFreeBuffers.pop();
        //mAllocBuffersSP.clear();
        //mAvailableBuffers.clear();
    }

    if (mBufferInfo.size() < (size_t)mBufferCnt) {
        if (!createNativeBuffer()) {
            ERROR("fail to create native buffer");
            return -1;
        }

        int index = (int)mBufferInfo.size() - 1;
        //mBufferInfo.push_back(bufInfo);
        mBufferToIdxMap[mBufferInfo[index]] = index;
        info = mBufferInfo[index];
        info->usedByCodec = true;
        info->usedByDisplay = false;
        *buffer = info;
        INFO("create native buffer, %p", info);
        return 0;
    }

    if (mFreeBuffers.empty()) {
        INFO("no buffer to dequeue now");
        dumpBufferInfo_l();
        mCondition.wait();
    }

    if (!mFreeBuffers.empty()) {
        info = mFreeBuffers.front();
        mFreeBuffers.pop();
        info->usedByCodec = true;
        *buffer = info;
    } else
        ERROR("no buffer available, return null");

    setBufferCrop_l(*buffer);

    return 0;
}

int MMWaylandDrmSurface::cancel_buffer(MMNativeBuffer *buf, int fd) {
    FUNC_TRACK();
    MMAutoLock lock(mLock);

    int index = BufferToIndex(buf);
    if (index < 0) {
        ERROR("unkown buffer %p", buf);
        return -1;
    }

    MMWlDrmBuffer* info = mBufferInfo[index];

    //INFO("buf %p, index %d, info %p", buf, index, info);
    if (info->usedByCodec == false || info->usedByDisplay == true) {
        ERROR("fail to cancel buffer, used by codec %d, used by display %d",
            info->usedByCodec, info->usedByDisplay);
        return -1;
    }

    info->usedByCodec = false;
    //info->usedByDisplay = false;
    mFreeBuffers.push(info);

    INFO("cancel buffer index %d, has %d free", index, (int)mFreeBuffers.size());

    return 0;
}

int MMWaylandDrmSurface::queueBuffer(MMNativeBuffer *buf, int fd) {
    FUNC_TRACK();
    MMAutoLock lock(mLock);

    int index = BufferToIndex(buf);
    if (index < 0) {
        ERROR("unkown buffer %p", buf);
        return -1;
    }

    MMWlDrmBuffer* info = mBufferInfo[index];

    if (info->usedByCodec == false || info->usedByDisplay == true) {
        ERROR("fail to cancel infofer, used by codec %d, used by display %d",
            info->usedByCodec, info->usedByDisplay);
        return -1;
    }

    if (mDisplay->callback) {
        INFO("wait display callback before queueBuffer");
        dumpBufferInfo_l();
        mQueueBufCond.timedWait(80000);
        INFO("");
    }

    // attach the wl_infofer to wl_surface
    wl_surface_attach(mDisplay->surface, info->wl_buf, 0, 0);

    if (mDisplay->callback) {
        WARNING("previous callback isn't released in time, callback: %p", mDisplay->callback);
        wl_callback_destroy(mDisplay->callback);
    }

    mDisplay->callback = wl_surface_frame(mDisplay->surface);
    wl_callback_add_listener(mDisplay->callback, &frame_listener, this);

    wl_surface_commit(mDisplay->surface);
    wl_display_flush(mDisplay->display);

    info->usedByCodec = false;
    info->usedByDisplay = true;
    return 0;
}

int MMWaylandDrmSurface::BufferToIndex(MMNativeBuffer* buffer) {
    std::map<MMNativeBuffer*, int>::iterator it = mBufferToIdxMap.find(buffer);
    if (it != mBufferToIdxMap.end()) {
        return it->second;
    }

    return -1;
}

int MMWaylandDrmSurface::mapBuffer(MMNativeBuffer *buf, int x, int y, int w, int h, void **ptr) {
    FUNC_TRACK();
    if (!buf || !buf->bo[0]) {
        return -1;
    }

    *ptr = (void*)omap_bo_map(buf->bo[0]);;
    return 0;
}

int MMWaylandDrmSurface::unmapBuffer(MMNativeBuffer *buf) {
    FUNC_TRACK();
    if (!buf || !buf->bo[0]) {
        return -1;
    }

    return 0;
}

void MMWaylandDrmSurface::dumpBufferInfo() {
    MMAutoLock lock(mLock);
    dumpBufferInfo_l();
}

void MMWaylandDrmSurface::dumpBufferInfo_l() {
    FUNC_TRACK();

    int c = 0, d = 0;
    for (int i = 0; i < (int)mBufferInfo.size(); i++) {
        DEBUG("buf_%d	: used by codec %d, used by display %d",
             i, mBufferInfo[i]->usedByCodec, mBufferInfo[i]->usedByDisplay);
        if (mBufferInfo[i]->usedByCodec)
            c++;
        if (mBufferInfo[i]->usedByDisplay)
            d++;
    }
    INFO("buf num %d, used by codec %d, used by display %d", (int)mBufferInfo.size(), c, d);
}

void MMWaylandDrmSurface::setBufferCrop_l(MMWlDrmBuffer* buf) {
    if (!buf) {
        ERROR("null buffer");
        return;
    }

    if (buf->cropX != mCropX + mHwPadX ||
        buf->cropY != mCropY + mHwPadY ||
        buf->cropW != mCropW || buf->cropH != mCropH) {

        INFO("crop update for buffer %p:", buf);
        INFO("    old %x %x %x %x, new %d %d %d %d",
             buf->cropX, buf->cropY, buf->cropW, buf->cropH,
             mCropX + mHwPadX, mCropY + mHwPadY, mCropW, mCropH);
        buf->cropX = mCropX + mHwPadX;
        buf->cropY = mCropY + mHwPadY;
        buf->cropW = mCropW;
        buf->cropH = mCropH;

        if (buf->cropX + mCropW > (int)mPaddedWidth ||
            buf->cropY + mCropH > (int)mPaddedHeight)
            ERROR("buffer crop beyond buffer size");
    }
}

} // end of YUNOS_MM
