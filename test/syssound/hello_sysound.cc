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

#include <multimedia/SysSoundService.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

MM_LOG_DEFINE_MODULE_NAME("Hello_SysSoundService");

using namespace YUNOS_MM;

int main(int argc, char** argv)
{
    MMLOGI("hello, syssound service!\n");
    SysSoundServicePtr s = SysSoundService::publish();
    if (!s) {
        MMLOGE("failed to publish\n");
        return 0;
    }

    usleep(0xfffffff);
    MMLOGI("bye, sys sound service!\n");

    return 0;
}