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

#include "SysSoundImp.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>
#include "multimedia/UBusMessage.h"

namespace YUNOS_MM {

DEFINE_LOGTAG(SysSoundImp);

enum {
    MSG_play = 0
};

SysSoundImp::SysSoundImp()
{
    MMLOGI("+\n");
    MMLOGV("-\n");
}

SysSoundImp::~SysSoundImp()
{
    MMLOGI("+\n");
    mClientNode.reset();
    MMLOGV("-\n");
}

bool SysSoundImp::connect()
{
    MMLOGI("+\n");
    try {
        mClientNode = ClientNode<SysSoundProxy>::create("ssndclt");
        if (!mClientNode->init()) {
            MMLOGE("failed to init\n");
            mClientNode.reset();
            return false;
        }
        mMessager = new UBusMessage<SysSoundImp>(this);

        MMLOGI("success\n");
        return true;
    } catch (...) {
        MMLOGE("no mem\n");
        return false;
    }
}

mm_status_t SysSoundImp::play(SoundType soundType, float leftVolume, float rightVolume)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_play);
    param->writeInt32((int32_t)soundType);
    param->writeFloat(leftVolume);
    param->writeFloat(rightVolume);
    return mMessager->sendMsg(param);
}

mm_status_t SysSoundImp::handleMsg(MMParamSP param)
{
    int msg = param->readInt32();
    DEBUG("name %d", msg);

    mm_status_t status = MM_ERROR_SUCCESS;
    switch (msg) {
        case MSG_play:
        {
            SoundType soundType = (SoundType)param->readInt32();
            float leftVolume = param->readFloat();
            float rightVolume = param->readFloat();
            status = mClientNode->node()->play(soundType, leftVolume, rightVolume);
            break;
        }
        default:
        {
            ASSERT(0 && "Should not be here\n");
            break;
        }
    }

    return status;
}

}
