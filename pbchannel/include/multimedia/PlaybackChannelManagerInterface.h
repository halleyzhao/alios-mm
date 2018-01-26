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

#ifndef __PLAYBACK_CHANNEL_MANAGER_INTERFACE_H__
#define __PLAYBACK_CHANNEL_MANAGER_INTERFACE_H__

#include <string>
#include <vector>
#include "multimedia/mm_types.h"
#include "multimedia/mm_errors.h"
#include "multimedia/PlaybackEvent.h"

namespace YUNOS_MM {
#define INVALID_CHANNEL_ID "InvalidChannel"

class PlaybackChannelManagerInterface {
public:
    PlaybackChannelManagerInterface(){}
    virtual ~PlaybackChannelManagerInterface(){}

public:
    virtual mm_status_t dispatchPlaybackEvent(PlaybackEventSP event, bool needWakeLock = false) = 0;
    virtual mm_status_t getEnabledChannels(std::vector<std::string> &remoteIds) = 0;
    virtual mm_status_t addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid) = 0;
    virtual void removeChannelInfo(const char*packageName) = 0;

public:
    // dump PlaybackChannel info in media_service progress
    virtual std::string dumpsys() = 0;

protected:
    virtual std::string createChannel(const char* tag, const char*packageName, int32_t userId) = 0;
    virtual void destroyChannel(const char*channelId) = 0;
    virtual mm_status_t addChannelsListener(const char*callbackName) = 0;

public:
    static const char * serviceName() { return "com.yunos.media.channel.service"; }
    static const char * pathName() { return "/com/yunos/media/channel/service"; }
    static const char * iface() { return "com.yunos.media.channel.service.interface"; }
};

}

#endif //__PLAYBACK_CHANNEL_MANAGER_INTERFACE_H__
