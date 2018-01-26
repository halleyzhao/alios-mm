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
#ifdef _PLATFORM_TV
#include <GLES2/gl2.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _PLATFORM_TV
typedef unsigned int GLuint;
#endif

 // play video to the given surface. when surface is not provided, a new one will be created automatically
int playVideo(const char *input_file, void *surface, int id);

typedef void (*drawFrameFunc)(void* data);
// bind video frame to the given texture_id, the callback func (draw_frame) is triggered for each output video frame
int playVideoTexture(const char* input_file, GLuint texture_id, drawFrameFunc draw_frame, void* data, int *buffer_num, void *buffers[]);

#ifdef __cplusplus
};
#endif

