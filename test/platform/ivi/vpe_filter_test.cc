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
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <gtest/gtest.h>

#include "multimedia/mm_debug.h"

#include <xf86drm.h>
#include <omap_drm.h>
#include <omap_drmif.h>
#include <libdce.h>

#include "vpe_filter.h"

#define ALIGN2(x, n)   (((x) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))
#ifndef PAGE_SHIFT
#  define PAGE_SHIFT 12
#endif

#define FOURCC(ch0, ch1, ch2, ch3) \
        ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
         ((uint32_t)(uint8_t)(ch2) << 16)  | ((uint32_t)(uint8_t)(ch3) << 24))


MM_LOG_DEFINE_MODULE_NAME("MPTEST")

using namespace YUNOS_MM;

static VpeFilter *g_vpe_filter;
static int32_t g_start_ms;
static int32_t g_end_ms;

static int32_t g_output_frame_filled;
static int32_t g_input_frame;
static int32_t g_input_frame_emptied;

//static bool g_multi_bo = false;
static int64_t g_pts;

static omap_device* g_device;

// run g_frame_count frames and quite
static int g_frame_count = 1000;

static int g_width = 1280;
static int g_height = 720;


#define MAX_BUFFER_NUM 15
#define BUFFER_NUM 6

#define PRINTF printf

static int g_input_buffer_num = BUFFER_NUM;

static int g_rgb_fd[MAX_BUFFER_NUM];
static struct omap_bo* g_rgb_bo[MAX_BUFFER_NUM];

class MyListener : public VpeFilter::Listener {
    virtual void onBufferEmptied(int fd[], bool multiBo) {
        if (!g_vpe_filter) {
            ERROR("not init");
            return;
        }
        g_input_frame_emptied++;
        bool ret = g_vpe_filter->emptyBuffer(fd, multiBo, g_pts++);
        if (!ret)
            ERROR("empty buffer fail\n");
        else
            g_input_frame++;
    }

    virtual void onBufferFilled(int fd[], bool multiBo, int64_t pts) {
        if (!g_vpe_filter) {
            ERROR("not init");
            return;
        }

        if (!g_output_frame_filled)
            g_start_ms = getTimeUs() / 1000;
        else if (g_output_frame_filled == (g_frame_count - 1)) {
            g_end_ms = getTimeUs() / 1000;
            float cost_sec = (g_end_ms - g_start_ms) / 1000.0f;
            PRINTF("Finish process output %d frames, cost %.2fs, fps is %.2f\n",
                   g_frame_count,
                   cost_sec,
                   (g_frame_count - 1)/cost_sec );
        }

        g_output_frame_filled++;

        bool ret = g_vpe_filter->fillBuffer(fd, multiBo);
        if (!ret)
            ERROR("fill buffer fail\n");
    }
};

class VpeFilterTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

static struct omap_bo *
allocBo(struct omap_device* device, uint32_t bpp, uint32_t width, uint32_t height,
                uint32_t *bo_handle, uint32_t *pitch)
{
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

bool allocBuffer(int index, uint32_t fourcc, uint32_t w, uint32_t h, bool multiBo) {

    uint32_t bo_handles[4] = {0}/*, offsets[4] = {0}*/;
    uint32_t pitches[4] = {0};;


    switch(fourcc) {
    case FOURCC('N','V','1','2'):
        if (multiBo) {
        } else {
        }
        break;
    case FOURCC('R','G','B','4'): // V4L2_PIX_FMT_RGB32 argb8888 argb4
            g_rgb_bo[index] = allocBo(g_device, 8*4, w, h,
                            &bo_handles[0], &pitches[0]);
            g_rgb_fd[index] = omap_bo_dmabuf(g_rgb_bo[index]);
            bo_handles[1] = bo_handles[0];
            pitches[1] = pitches[0];
        break;
    default:
        ERROR("invalid format: 0x%08x", fourcc);
        goto fail;
    }

    return true;

fail:
    return false;
}


TEST_F(VpeFilterTest, VpeFilterCSC) {
    bool ret;

    g_vpe_filter = new VpeFilter();

    MyListener *listener = new MyListener;
    g_vpe_filter->setListener(listener);

    ret = g_vpe_filter->configureInput(g_width, g_height, V4L2_PIX_FMT_RGB32, g_input_buffer_num, 0);
    if (!ret) {
        ERROR("fail to config input buffer");
        exit(-1);
    }

    ret = g_vpe_filter->configureOutput(g_width, g_height, V4L2_PIX_FMT_NV12);
    if (!ret) {
        ERROR("fail to config output buffer");
        exit(-1);
    }

    for (int i = 0; i < g_input_buffer_num; i++) {
        ret = allocBuffer(i, V4L2_PIX_FMT_RGB32, g_width, g_height, false);
        if (!ret) {
            ERROR("fail to alloc input buffer");
            exit(-1);
        }
    }

    for (int i = 0; i < g_input_buffer_num; i++) {
        int fd[2];
        fd[0] = g_rgb_fd[i];
        ret = g_vpe_filter->emptyBuffer(fd, false, g_pts++);
        if (!ret) {
            ERROR("fail to alloc input buffer");
            exit(-1);
        }
        g_input_frame++;
    }

    ret = g_vpe_filter->start();
    if (!ret) {
        ERROR("fail to start vpe");
        exit(-1);
    }

    while(1) {
        usleep(1000*1000);

        if (g_output_frame_filled >= g_frame_count)
            break;

        int64_t now_ms = getTimeUs() / 1000;
        float cost_sec = (now_ms - g_start_ms) / 1000.0f;
        PRINTF("input %d, process input %d, process output %d frames, cost %.2fs, fps is %.2f\n",
               g_input_frame,
               g_input_frame_emptied,
               g_output_frame_filled,
               cost_sec,
               (g_output_frame_filled - 1)/cost_sec );

    };

    g_vpe_filter->stop();

    delete g_vpe_filter;

    MMLOGI("exit\n");
}

int main(int argc, char* const argv[]) {
    int ret;
    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
    } catch (...) {
        ERROR("InitGoogleTest failed!");
        return -1;
    }
    return ret;
}
