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

#include <multimedia/mm_debug.h>

#include <multimedia/PCMPlayer.h>

#include "PCMPlayerIMP.h"

namespace YUNOS_MM {

MM_LOG_DEFINE_MODULE_NAME("PCMP");

/*static */PCMPlayer * PCMPlayer::create()
{
    return new PCMPlayerIMP();
}

/*static */void PCMPlayer::destroy(PCMPlayer * player)
{
    MM_RELEASE(player);
}

PCMPlayer::PCMPlayer() : mListener(NULL)
{
    MMLOGV("");
}

PCMPlayer::~PCMPlayer()
{
}

void PCMPlayer::setListener(Listener * listener)
{
    MMLOGI("\n");
    mListener = listener;
}


}
