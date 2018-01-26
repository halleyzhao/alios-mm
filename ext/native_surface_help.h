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

#ifndef __native_surface_help_h
#define __native_surface_help_h

#include "multimedia/mm_surface_compat.h"
#if defined(__MM_YUNOS_CNTRHAL_BUILD__) || defined(__MM_YUNOS_YUNHAL_BUILD__) || defined(__MM_YUNOS_LINUX_BSP_BUILD__)
    #include <WaylandConnection.h>
    #include "WindowSurfaceTestWindow.h"
    #if defined(MM_ENABLE_PAGEWINDOW)
        #include "SimpleSubWindow.h"
    #endif
#endif
typedef enum {
    NST_SimpleWindow = 0,
    NST_PagewindowMain = 1,
    NST_PagewindowSub = 2,
    NST_PagewindowRemote = 4,
    NST_PagewindowMainRemote = NST_PagewindowMain | NST_PagewindowRemote,
    NST_PagewindowMainSub = NST_PagewindowSub | NST_PagewindowRemote,
} NativeSurfaceType;
WindowSurface* createSimpleSurface(int32_t width, int32_t height, NativeSurfaceType pagewindow_surface_type = NST_SimpleWindow, int rotation = 0);
void destroySimpleSurface(WindowSurface *surface);
void cleanupSurfaces();

#endif // __native_surface_help_h

