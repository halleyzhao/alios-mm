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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <ctype.h>
#include "multimedia/mm_debug.h"
#include "multimedia/mm_cpp_utils.h"

using namespace YUNOS_MM;
MM_LOG_DEFINE_MODULE_NAME ("cameraSource")

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif

#include <compat/media/surface_texture_client_compat.h>
#include <compat/media/camera_source_compat.h>

int main(void)
{
    SurfaceTextureClientHybris stc= NULL;
    CameraSourceHybris source = NULL;


    stc = surface_texture_client_create();
    bool ret;
    int frame_count = 1000;
/*
    bool res = surface_texture_client_set_position(stc, g_surface_pos_x, g_surface_pos_y);
    res &= surface_texture_client_set_size(stc, g_surface_width, g_surface_height);
    res &= surface_texture_client_set_alpha(stc, g_surface_alpha);
    DEBUG("%s to resize video surface to %.1f+%.1f+%dx%d with alpha=%.2f\n",
        res ? "success" : "fail", g_surface_pos_x, g_surface_pos_y, g_surface_width, g_surface_height, g_surface_alpha);
    }
*/

    if(stc == NULL){
        ERROR("surface texture client create fail!\n");
        return -1;
    }
    DEBUG("surface texture client created!\n");

    DEBUG("create camera source\n");
    //sprd don't support 1080P resolution
    source = camera_source_create("camera.source.simple.test", 0,
                                  30,
                                  stc,
                                  1280, 720,
                                  true);
    if (source == NULL) {
        ERROR("create 720P resolution camera source failed");
        return -1;
    }

    DEBUG("create camera source done!\n");

    DEBUG("camera start preview\n");
    ret = camera_source_start_preview(source);
    if (!ret) {
        ERROR("camera start preview fail!\n");
        return -1;
    }

    DEBUG("camera start preview done!\n");

    DEBUG("camera start recording\n");
    ret = camera_source_start_recording(source);
    if (!ret) {
        ERROR("camera start recording fail!\n");
        return -1;
    }

    DEBUG("camera recording started\n");
#if 0
    DEBUG("surface texture client setBuffersGeometry\n");
    ret = surface_texture_client_setBuffersGeometry(stc, w, h, 0); /* use default colorFormat */
    if (!ret)
        DEBUG("surface texture client setBuffersGeometry fail!\n");

    DEBUG("surface texture client setUsage\n");
    ret = surface_texture_client_setUsage(stc, 0); /* use default usage */
    if (!ret)
        DEBUG("surface texture client setUsage fail!\n");

    DEBUG("surface texture client setBufferCount\n");
    ret = surface_texture_client_setBufferCount(stc, 5);
    if (!ret)
        DEBUG("surface texture client setBufferCount fail!\n");

    DEBUG("surface texture client setCrop\n");
    ret = surface_texture_client_setCrop(stc, 0, 0, 512, 512);
    if (!ret)
        DEBUG("surface texture client setCrop fail!\n");

    DEBUG("surface texture client setScalingMode\n");
    ret = surface_texture_client_setScalingMode(stc, 0); /*use default scaling mode */
    if (!ret)
        DEBUG("surface texture client setScalingMode fail!\n");
#endif

    for (int i = 0; i < frame_count; i++) {
        void *addr;
        int32_t size;
        int64_t timeUs;

        ret = camera_source_read(source, &addr, &size, &timeUs);
        if (!ret) {
            ERROR("camera source read fail\n");
            break;
        }

        DEBUG("get buffer: addr(%p) size(%d) timeStampt(%d)\n", addr, size, (int32_t)(timeUs/1000));
        ret = camera_source_return_buffer(source, addr);
        if (!ret) {
            ERROR("camera source return buffer fail\n");
            break;
        }
    }

    DEBUG("stop camera source\n");
    camera_source_stop_recording(source);
    DEBUG("finish stop camera source\n");

    DEBUG("Delete camera source\n");
    camera_source_destroy(source);
    DEBUG("Finish delete camera source\n");

    DEBUG("Delete SurfaceTextureClient\n");
    surface_texture_client_destroy(stc);
    DEBUG("Finish delete SurfaceTextureClient\n");

    return ret;
}
