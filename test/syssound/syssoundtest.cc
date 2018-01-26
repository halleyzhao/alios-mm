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
#include <multimedia/SysSound.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

MM_LOG_DEFINE_MODULE_NAME("syssound-test");

using namespace YUNOS_MM;

int main(int argc, char** argv)
{
    MMLOGI("hello, sound service!\n");

    SysSoundPtr s = SysSound::create();
    if (!s) {
        MMLOGE("failed to create\n");
        return 0;
    }

    for (int j = 0; j < 100; ++j) {
        for (int i = 0; i < ISysSound::ST_COUNT; ++i) {
            printf("playing %d / %d\n", i, j);
            MMLOGI("playing %d / %d\n", i, j);
            s->play((ISysSound::SoundType)i, 1.0, 1.0);
            MMLOGV("playing %d / %d, call over\n", i, j);
            usleep(50000);
        }
    }

    MMLOGI("test over, bye\n");
    return 0;
}
