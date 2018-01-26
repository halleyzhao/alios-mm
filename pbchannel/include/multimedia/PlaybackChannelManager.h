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

#ifndef __PLAYBACK_CHANNEL_MANAGER_H__
#define __PLAYBACK_CHANNEL_MANAGER_H__

#include "multimedia/mm_types.h"
#include "multimedia/mm_errors.h"
#include "multimedia/mm_cpp_utils.h"
#include <multimedia/PlaybackChannelManagerInterface.h>
#include <multimedia/PlaybackRemote.h>

namespace YUNOS_MM {

class PlaybackChannelManager;
typedef MMSharedPtr<PlaybackChannelManager> PlaybackChannelManagerSP;

class PlaybackChannelManager : public PlaybackChannelManagerInterface {
public:
    class Listener : public MMListener {
        public:
            enum MsgType {
                /**
                 * onError callback
                 * params:
                 *      param1: error code.
                 */
                kMsgError = 0
            };
            Listener(){}
            virtual ~Listener(){}
            // In order to be compatible with NodePlaybackChannelManager
            virtual void onMessage(int msg, int param1, int param2, const MMParam *obj) {}
            virtual void onEnabledChannelsChanged(std::vector<PlaybackRemoteSP> &remotes) {};
    };

public:
    static PlaybackChannelManagerSP getInstance(); //single instance

public:
    virtual mm_status_t addListener(Listener * listener) = 0;
    // Listener should be added to list before
    // NULL pointer will remove all the listeners
    virtual void removeListener(Listener * listener)  = 0;
    // add for Node PlaybackChannelManager compile
    virtual mm_status_t getEnabledChannels(std::vector<std::string> &remoteIds) = 0;
    virtual mm_status_t getEnabledChannels(std::vector<PlaybackRemoteSP> &remotes) = 0;

public:
    virtual ~PlaybackChannelManager(){}

protected:
    PlaybackChannelManager(){}
    static Lock mLock;

    MM_DISALLOW_COPY(PlaybackChannelManager)
    DECLARE_LOGTAG()
};


}

#endif /* __PLAYBACK_CHANNEL_MANAGER_H__ */

