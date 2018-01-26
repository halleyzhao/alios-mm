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
#include <stdlib.h>
#include "SimpleSubWindow.h"

uint32_t g_surface_width = 360;
uint32_t g_surface_height = 202;

#define SUB_SURFACE_COUNT   5
int main()
{
    srand(time(NULL));
    uint32_t i=0;
    WindowSurface *sub[SUB_SURFACE_COUNT];

    initializePageWindow();

    for (i=0; i<SUB_SURFACE_COUNT; i++) {
        sub[i] = NULL;
        createSubWindow(&sub[i]);
        setSubWindowGeometry(sub[i], (i*50)%720, (i*150)%1280, g_surface_width, g_surface_height);
        renderSubWindow(sub[i]);
        sleep(rand()%2);
    }

    sleep(1);

    deinitializePageWindow();

    return 0;
}
