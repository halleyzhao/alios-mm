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

#ifndef __I_MEDIA_SESSION_TEST_HELPER_H__
#define __I_MEDIA_SESSION_TEST_HELPER_H__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>


#include "multimedia/media_meta.h"
#include "multimedia/media_attr_str.h"
#include <multimedia/mmthread.h>
#include "multimedia/mm_debug.h"
#include "multimedia/PlaybackState.h"
#include <multimedia/PlaybackChannel.h>
#include <multimedia/PlaybackRemote.h>
#include <multimedia/PlaybackChannelManager.h>


namespace YUNOS_MM {
class PlaybackChannelListener : public PlaybackChannel::Listener{
public:
    PlaybackChannelListener(PlaybackChannel *channel);
    virtual void onMessage(int msg, int param1, int param2, const MMParam *obj);
    static std::string getMessageString(int msg);
private:
    PlaybackChannel *mChannel;
    PlaybackStateSP mState;
    DECLARE_LOGTAG()
};

class PlaybackRemoteListener : public PlaybackRemote::Listener{
public:
    PlaybackRemoteListener(PlaybackRemote *controller);
    virtual void onMessage(int msg, int param1, int param2, const MMParam *obj);
    static std::string getMessageString(int msg);
private:
    PlaybackRemote *mRemote;
    DECLARE_LOGTAG()
};


class PlaybackChannelManagerListener : public PlaybackChannelManager::Listener{
public:
    virtual void onEnabledChannelsChanged(std::vector<PlaybackRemoteSP> &remotes);
private:
    DECLARE_LOGTAG()
};

typedef MMSharedPtr<PlaybackChannelManager::Listener> ManagerListenerSP;
typedef MMSharedPtr<PlaybackChannel::Listener> ChannelListenerSP;
typedef MMSharedPtr<PlaybackRemote::Listener> ControllerListenerSP;

class PlaybackChannelTestHelper {
public:
    PlaybackChannelTestHelper();
    ~PlaybackChannelTestHelper() {}

    void channelCreate(int32_t num = 0);
    void channelConfigure(bool isNeedMediaButton, int32_t num = 0);
    void channelRelease(int32_t num = 0);
    void controllerCreate(bool useManager = false, const char *remoteId = NULL, int32_t num = 0);
    void controllerRelease();
    void managerRelease();

    void controllerGet();
    void controllerTransportControl();
    void remoteSendPlaybackEvent(bool ipc = false);

    PlaybackChannelSP getChannel(int32_t num = 0) { return mChannels[0]; }
    PlaybackRemoteSP getRemote() { return mRemote; }
    PlaybackChannelManagerSP getManager() { return mManager; }

public:
    std::vector<PlaybackChannelSP> mChannels;
    std::vector<ChannelListenerSP> mChannelListeners;

    PlaybackRemoteSP mRemote;
    std::vector<PlaybackRemoteSP> mRemotes;
    PlaybackChannelManagerSP mManager;

    ControllerListenerSP mControllerListener;
    ManagerListenerSP mManagerListener;

private:
    void remoteSendPlaybackEvent1(bool ipc = false);
    void remoteSendPlaybackEvent2(bool ipc = false);
    void remoteSendPlaybackEvent3(bool ipc = false);
    DECLARE_LOGTAG()
};
typedef MMSharedPtr<PlaybackChannelTestHelper> PlaybackChannelTestHelperSP;

}

#endif //__I_MEDIA_SESSION_TEST_HELPER_H__
