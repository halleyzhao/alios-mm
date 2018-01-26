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

#include "native_surface_help.h"
#include "multimedia/mm_cpp_utils.h"
#include "multimedia/mm_debug.h"
MM_LOG_DEFINE_MODULE_NAME("NATIVE_SURFACE_HELP");

#if defined(MM_ENABLE_PAGEWINDOW)
    static bool g_pagewindowInited = false;
    // FIXME: use a map to record window type of each surface
    NativeSurfaceType g_windowType = NST_SimpleWindow;
#endif

#if defined(__MM_YUNOS_CNTRHAL_BUILD__) || defined(__MM_YUNOS_YUNHAL_BUILD__) || defined(__MM_YUNOS_LINUX_BSP_BUILD__)
WindowSurface* createSimpleSurface(int32_t width, int32_t height, NativeSurfaceType pagewindow_surface_type, int rotation)
{
    WindowSurface* ws = NULL;
#if defined(MM_ENABLE_PAGEWINDOW)
    static int32_t sub_surface_offset = 0;
    static uint32_t sub_surface_width = 1280;
    static uint32_t sub_surface_height = 720;

    if (pagewindow_surface_type != NST_SimpleWindow) {
        setenv("USE_BUFFER_QUEUE", "1", 1); // required by pagewindow
        g_windowType = pagewindow_surface_type;
        if  (!g_pagewindowInited) {
            bool remote = (pagewindow_surface_type & NST_PagewindowRemote);
            bool mainRemote = (remote && !(pagewindow_surface_type & NST_PagewindowSub));
            bool subRemote = (remote && (pagewindow_surface_type & NST_PagewindowSub));

            DEBUG("main remote: %d, sub remote %d", mainRemote, subRemote);
            initializePageWindow(mainRemote, subRemote, rotation);
            g_pagewindowInited = true;
        }
        if (pagewindow_surface_type & NST_PagewindowSub) {
            createSubWindow(&ws);
        } else if (pagewindow_surface_type &  NST_PagewindowMain) {
            createWindow(&ws);
        }

        sub_surface_offset += 50;
        if (sub_surface_offset > 200)
            sub_surface_offset = 0;
        sub_surface_width -= 50;
        if (sub_surface_width < 200)
            sub_surface_width = 1280;
        sub_surface_height -= 50;
        if (sub_surface_height < 200)
            sub_surface_height = 720;

        DEBUG("ws: %p", ws);
        return ws;
    }
#endif

#if defined(__MM_YUNOS_CNTRHAL_BUILD__) || defined(__MM_YUNOS_YUNHAL_BUILD__)
    ws = createWaylandWindow2(width, height);
#elif defined(__MM_YUNOS_LINUX_BSP_BUILD__)
    ws = createWaylandWindow(width, height);
#else
    ERROR("uncertain surface type");
#endif

    return ws;
}
void destroySimpleSurface(WindowSurface* surface)
{
    if (!surface)
        return;
#if defined(MM_ENABLE_PAGEWINDOW)
    if (g_windowType & NST_PagewindowMain) {
        return;
    }
    if (g_windowType & NST_PagewindowSub) {
        destroySubWindow(surface);
        return;
    }
#endif

#if defined(__MM_YUNOS_CNTRHAL_BUILD__) || defined(__MM_YUNOS_YUNHAL_BUILD__)
        destroyWaylandWindow2(surface);
#else
        ERROR("uncertain surface type");
#endif
}

void cleanupSurfaces()
{
#if defined(MM_ENABLE_PAGEWINDOW)
    if  (g_pagewindowInited) {
        deinitializePageWindow();
        g_pagewindowInited = false;
    }
#endif
}

#else
void* createSimpleSurface(int32_t width, int32_t height, int rotation)
{
    return NULL;
}
void destroySimpleSurface(WindowSurface *surface){}
void cleanupSurfaces() {}
#endif
