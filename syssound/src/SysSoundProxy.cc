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

#include "SysSoundProxy.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>


namespace YUNOS_MM {

DEFINE_LOGTAG(SysSoundProxy);

SysSoundProxy::SysSoundProxy(const yunos::SharedPtr<yunos::DService>& service,
                                    yunos::String /*serviceName*/,
                                    yunos::String /*pathName*/,
                                    yunos::String /*iface*/,
                                    void* /*arg*/)
                    : yunos::DProxy(service, yunos::String(ISysSound::pathName()), yunos::String(ISysSound::_interface()))
{
    MMLOGI("+\n");
    MMLOGV("-\n");
}

SysSoundProxy::~SysSoundProxy()
{
    MMLOGI("+\n");
    MMLOGV("-\n");
}

mm_status_t SysSoundProxy::play(SoundType soundType, float leftVolume, float rightVolume)
{
    if (soundType < ST_KEY_CLICK || soundType >= ST_COUNT) {
        MMLOGE("invalid soundType: %d\n", soundType);
        return MM_ERROR_INVALID_PARAM;
    }

    MMLOGV("soundType: %d, leftVolume: %f, rightVolume: %f\n", soundType, leftVolume, rightVolume);
    yunos::SharedPtr<yunos::DMessage> msg = obtainMethodCallMessage(yunos::String("play"));
    if (msg == NULL) {
        MMLOGE("msg is null\n");
        return MM_ERROR_NO_MEM;
    }

    msg->writeInt32(soundType);
    msg->writeDouble(leftVolume);
    msg->writeDouble(rightVolume);

    bool ret = sendMessage(msg);

    MMLOGV("ret: %d\n", ret);
    return ret ? MM_ERROR_SUCCESS : MM_ERROR_OP_FAILED;
}

}

