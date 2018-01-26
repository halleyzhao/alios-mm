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
#ifndef SimpleSubWindow_H
#define SimpleSubWindow_H
#include <stdbool.h>
#include "multimedia/mm_surface_compat.h"

bool initializePageWindow(bool isRemote = false, bool isSubRemote = false, int rotation = 0);
void deinitializePageWindow();

bool createSubWindow(WindowSurface** sub);
bool createWindow(WindowSurface** sub);
std::string getWindowName(WindowSurface* window);
void pagewindowShow(bool visibility);

bool destroySubWindow(WindowSurface*sub);
bool setSubWindowGeometry(WindowSurface*sub, int32_t x, int32_t y, uint32_t width, uint32_t height);
void setSubWindowFit(WindowSurface*sub);

bool subWindowReleaseBuffer(WindowSurface*sub);
bool subWindowRestoreBuffer(WindowSurface*sub);

// test use, not necessary for video usage
void renderSubWindow(WindowSurface*sub);
#endif // SimpleSubWindow_H
