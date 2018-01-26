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

#ifndef __PLAYBACK_REMOTE_INTERFACE_H__
#define __PLAYBACK_REMOTE_INTERFACE_H__
#include "multimedia/media_meta.h"
#include "multimedia/mm_cpp_utils.h"
#include "multimedia/PlaybackState.h"
#include "multimedia/PlaybackEvent.h"

namespace YUNOS_MM {

class PlaybackRemoteInterface {

public:
    PlaybackRemoteInterface(){}
    virtual ~PlaybackRemoteInterface(){}

public:
    virtual MediaMetaSP getMetadata() = 0;
    virtual PlaybackStateSP getPlaybackState() = 0;
    virtual int getPlaylist() = 0; //FIXME: add playlist support
    virtual std::string getPlaylistTitle() = 0;
    virtual std::string getPackageName() = 0;
    virtual int32_t getUsages() = 0;
    virtual std::string getTag() = 0;
    virtual mm_status_t sendPlaybackEvent(PlaybackEventSP event) = 0;

    // for remote
    virtual mm_status_t play() = 0;
    virtual mm_status_t pause() = 0;
    virtual mm_status_t seekTo(int64_t /*ms*/) = 0;
    virtual mm_status_t stop() = 0;
    virtual mm_status_t next() = 0;
    virtual mm_status_t previous() = 0;
    virtual mm_status_t fastForward() = 0;
    virtual mm_status_t rewind() = 0;

public:
    static const char * serviceName() { return "com.yunos.media.remote";}
    static const char * pathName() { return "/com/yunos/media/remote"; }
    static const char * iface() { return "com.yunos.media.remote.interface"; }
};
}

#endif //__PLAYBACK_REMOTE_INTERFACE_H__
