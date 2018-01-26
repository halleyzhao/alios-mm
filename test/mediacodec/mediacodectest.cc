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
#include <stdlib.h>
#include <glib.h>
#include <gtest/gtest.h>
#include <stdio.h>

#include "mediacodecPlay.h"
#include "multimedia/mm_debug.h"
#include "mmwakelocker.h"
MM_LOG_DEFINE_MODULE_NAME ("MEDIACODECTEST")

extern float g_surface_pos_x, g_surface_pos_y, g_surface_alpha;
extern uint32_t g_surface_width, g_surface_height;
extern bool g_full_run_video;
extern int g_exit_after_draw_times;
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
extern bool g_use_sub_surface;
extern bool g_use_media_texture;
#endif

static const char *input_url = NULL;
static float xPos = -1;
static float yPos = -1;
static float alpha = -1;
static uint32_t width =0;
static uint32_t height =0;
static gboolean full_run_video =FALSE;
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
static gboolean use_sub_surface =FALSE;
static gboolean use_media_texture =FALSE;
#endif
static int exit_after_draw_times =0;

static GOptionEntry entries[] = {
    {"file", 'f', 0, G_OPTION_ARG_STRING, &input_url, "media file", NULL},
    {"xpos", 'x', 0, G_OPTION_ARG_INT, &xPos, "horizontal offset of the video surface", NULL},
    {"ypos", 'y', 0, G_OPTION_ARG_INT, &yPos, "vertical offset of the video surface", NULL},
    {"width", 'w', 0, G_OPTION_ARG_INT, &width, "video surface width", NULL},
    {"height", 'h', 0, G_OPTION_ARG_INT, &height, "video surface height", NULL},
    {"alpha", 'a', 0, G_OPTION_ARG_INT, &alpha, "video surface alpha", NULL},
    {"full_run_video", 'r', 0, G_OPTION_ARG_NONE, &full_run_video, "play video as fast as possible", NULL},
    {"exit_after_draw_times", 'c', 0, G_OPTION_ARG_INT, &exit_after_draw_times, "quit after play count of frames", NULL},
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    {"use_sub_surface", 'b', 0, G_OPTION_ARG_NONE, &use_sub_surface, "use wayland sub surface", NULL},
    {"use_media_texture", 't', 0, G_OPTION_ARG_NONE, &use_media_texture, "use media texture", NULL},
#endif
    {NULL}
};


class MediacodecTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(MediacodecTest, mediacodecplaytest) {
    int ret;
    ret= playVideo(input_url, NULL, 0);
    EXPECT_TRUE(ret == 0);
}

int main(int argc, char* const argv[]) {
    int ret = -1;

     if (argc < 2) {
        PRINTF("no input media file\n");
        return 0;
    }

    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new(MM_LOG_TAG);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, TRUE);

    if (!g_option_context_parse(context, &argc, (gchar***)&argv, &error)) {
            PRINTF("option parsing failed: %s\n", error->message);
            return -1;
    }
    g_option_context_free(context);

    if(input_url){
        PRINTF("set file %s\n", input_url);
    }

    if(xPos >0){
         PRINTF("set xPos %f\n", xPos);
         g_surface_pos_x = xPos;
    }

    if(yPos >0){
         PRINTF("set yPos %f\n", yPos);
         g_surface_pos_y= yPos;
    }

    if(width >0){
        PRINTF("set width %d\n",width);
        g_surface_width = width;
    }

    if(height>0){
        PRINTF("set height %d\n",height);
    }

    if(alpha >0){
        PRINTF("set alpha %f\n", alpha);
        g_surface_alpha = alpha;
    }

    if(full_run_video){
        PRINTF("set full_run_video %d\n",full_run_video);
        g_full_run_video = full_run_video;
    }

    if(exit_after_draw_times>0){
        PRINTF("set exit_after_draw_times %d\n",exit_after_draw_times);
        g_exit_after_draw_times = exit_after_draw_times;
    }

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    if(use_sub_surface){
        PRINTF("set use_sub_surface %d\n",use_sub_surface);
        g_use_sub_surface = use_sub_surface;
    }

    if(use_media_texture){
        PRINTF("set use_media_texture %d\n",use_media_texture);
        g_use_media_texture = use_media_texture;
    }
#endif
    AutoWakeLock awl;
    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
    } catch (...) {
        PRINTF("InitGoogleTest failed!");
        ret = -1;
    }

    if (input_url)
        g_free((void*)input_url);
    return ret;
}


