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
MM_LOG_DEFINE_MODULE_NAME ("MEDIACODECPLAY")

#include <compat/media/media_codec_layer.h>
#include <compat/media/surface_texture_client_compat.h>


int main(void)
{
    SurfaceTextureClientHybris  stc= NULL;
    stc = surface_texture_client_create();
    bool ret;
    int w = 512;
    int h = 512;
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
    PRINTF("surface texture client created!\n");

    PRINTF("surface texture client setBuffersGeometry\n");
    ret = surface_texture_client_setBuffersGeometry(stc, w, h, 0); /* use default colorFormat */
    if (!ret) {
        PRINTF("surface texture client setBuffersGeometry fail!\n");
        return -1;
    }

    PRINTF("surface texture client setUsage\n");
    ret = surface_texture_client_setUsage(stc, 0); /* use default usage */
    if (!ret) {
        PRINTF("surface texture client setUsage fail!\n");
        return -1;
    }

    PRINTF("surface texture client setBufferCount\n");
    ret = surface_texture_client_setBufferCount(stc, 5);
    if (!ret) {
        PRINTF("surface texture client setBufferCount fail!\n");
        return -1;
    }

    PRINTF("surface texture client setCrop\n");
    ret = surface_texture_client_setCrop(stc, 0, 0, 512, 512);
    if (!ret) {
        PRINTF("surface texture client setCrop fail!\n");
        return -1;
    }

    PRINTF("surface texture client setScalingMode\n");
    ret = surface_texture_client_setScalingMode(stc, 0); /*use default scaling mode */
    if (!ret) {
        PRINTF("surface texture client setScalingMode fail!\n");
        return -1;
    }

    for (int i = 0; i < frame_count; i++) {
        size_t idx;
        void *addr = NULL;
        int buffer_size;
        int pixel_bpp = 4;

        buffer_size = w*h*pixel_bpp;

        ret = surface_texutre_client_dequeueBuffer(stc, &idx);
        if (!ret) {
            PRINTF("surface texture client dequeueBuffer fail!\n");
            return -1;
        }

        ret = surface_texture_client_lockBuffer(stc, idx, &addr);
        if (!ret || NULL == addr) {
            PRINTF("surface texture client lockBuffer fail!\n");
            return -1;
        }

        memset(addr, i%255, buffer_size);

        ret = surface_texture_client_unlockBuffer(stc, idx);
        if (!ret) {
            PRINTF("surface texture client unlockBuffer fail!\n");
            return -1;
        }

        ret = surface_texutre_client_queueBuffer(stc, idx);
    }

    DEBUG("Delete SurfaceTextureClient\n");
    surface_texture_client_destroy(stc);
    DEBUG("Finish delete SurfaceTextureClient\n");

    return ret;
}
