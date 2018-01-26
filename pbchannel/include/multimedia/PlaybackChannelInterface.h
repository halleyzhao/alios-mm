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

#ifndef __PLAYBACK_CHANNEL_INTERFACE_H__
#define __PLAYBACK_CHANNEL_INTERFACE_H__
#include <multimedia/mm_cpp_utils.h>
#include "multimedia/PlaybackState.h"
#include "multimedia/media_meta.h"

namespace YUNOS_MM {

class PlaybackChannelInterface {
public:
    enum Usages
    {
        /**
         * Indicate PlaybackChannel will report MediaButton key to client,
         * otherwise report onplay/onpuase .etc command
         */
        kNeedMediaButtonKeys = 1 << 0,
        /**
         * Playback setting this usage meaning playback event always is sent to this Playback Channel.
         * always the usage is used for TelCom app.
         */
         kGlobalPriority = 1 << 1
    };

public:
    PlaybackChannelInterface(){}
    virtual ~PlaybackChannelInterface(){}

public:
    /**
     * Note: get the id of PlaybackRemote adaptor,
     * and should pass it to the progress which PlaybackRemote is in,
     * so PlaybackRemote proxy can communicate with it's adaptor
     */
    virtual std::string getPlaybackRemoteId() = 0;
    virtual mm_status_t setUsages(int32_t usage) = 0;
    virtual mm_status_t setEnabled(bool enabled) = 0;
    virtual mm_status_t setPlaybackState(PlaybackStateSP state) = 0;
    virtual mm_status_t setPlaylist(int32_t playlist) = 0;
    virtual mm_status_t setPlaylistTitle(const char*title) = 0;
    virtual mm_status_t setMetadata(MediaMetaSP meta) = 0;

public:

    static const char * serviceName() { return "com.yunos.playback.channel"; }
    static const char * pathName() { return "/com/yunos/playback/channel"; }
    static const char * iface() { return "com.yunos.playback.channel.interface"; }
};

}

#endif //__PLAYBACK_CHANNEL_INTERFACE_H__