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

#include <multimedia/instantaudio.h>
#include "instantaudioimp.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(InstantAudio)

/*static*/ InstantAudio * InstantAudio::create(size_t maxChannels)
{
    MMLOGI("+maxChannels: %u\n", maxChannels);
    InstantAudioImp * sp = new InstantAudioImp(maxChannels);
    if ( !sp ) {
        MMLOGE("new failed\n");
        return NULL;
    }

    if ( !sp->init() ) {
        MMLOGE("init failed\n");
        MM_RELEASE(sp);
        return NULL;
    }

    return sp;
}

/*static*/ void InstantAudio::destroy(InstantAudio * instantaudio)
{
    MMLOGI("+\n");
    InstantAudioImp * ia = dynamic_cast<InstantAudioImp*>(instantaudio);
    MMASSERT(ia != NULL);
    if (ia) {
        ia->release();
    }
    MM_RELEASE(instantaudio);
}


InstantAudio::InstantAudio() : mListener(NULL)
{
}

InstantAudio::~InstantAudio()
{
}

mm_status_t InstantAudio::setListener(Listener * listener)
{
    MMLOGI("+\n");
    mListener = listener;
    return MM_ERROR_SUCCESS;
}


}

