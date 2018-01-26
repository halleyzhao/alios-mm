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
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/PlaybackChannel.h>
#include "PlaybackChannelImp.h"

namespace YUNOS_MM {

DEFINE_LOGTAG1(PlaybackChannel, [PC])
/*static*/ PlaybackChannelSP PlaybackChannel::create(const char*appTag, const char* packageName)
{
    INFO("+\n");
    try {
        std::string tag = appTag ? appTag : "PlaybackChannelDebug";
        if (!packageName || strlen(packageName) == 0) {
            // ASSERT(0);
            ERROR("invalid package name");
            packageName = "null";
            // return PlaybackChannelSP((PlaybackChannel*)NULL);
        }

        PlaybackChannelImp * imp = new PlaybackChannelImp(tag.c_str(), packageName);
        DEBUG("PlaybackChannel %p", imp);
        if (!imp || !imp->connect()) {
            ERROR("failed to connect\n");
            delete imp;
            return PlaybackChannelSP((PlaybackChannel*)NULL);
        }
        PlaybackChannelSP s(imp, &PlaybackChannel::releaseChannelFunc);
        return s;
    } catch (...) {
        ERROR("no mem\n");
        return PlaybackChannelSP((PlaybackChannel*)NULL);
    }
}

/*static*/ bool PlaybackChannel::releaseChannelFunc(PlaybackChannel *channel)
{
    DEBUG("PlaybackChannel %p", channel);
    ASSERT(channel);
    PlaybackChannelImp *channelImp = static_cast<PlaybackChannelImp *>(channel);
    channelImp->destroy();
    delete channelImp;
    return true;
}


}
