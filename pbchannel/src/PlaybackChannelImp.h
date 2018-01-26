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

#ifndef __PLAYBACK_CHANNEL_IMP_H__
#define __PLAYBACK_CHANNEL_IMP_H__
#include <dbus/dbus.h>
#include <dbus/DProxy.h>
#include <dbus/DService.h>
#include <pointer/SharedPtr.h>
#include <dbus/DAdaptor.h>
#include <dbus/DService.h>
#include <dbus/DSignalCallback.h>
#include <thread/LooperThread.h>

#include <multimedia/PlaybackChannel.h>
#include <multimedia/PlaybackChannelManager.h>

namespace YUNOS_MM {
class PlaybackChannelProxy;
class PlaybackChannelImp;
template <class ClientType> class UBusMessage;
template <class UBusType> class ClientNode;
typedef MMSharedPtr<ClientNode<PlaybackChannelProxy> > PlaybackChannelCNSP;


//////////////////////////////////////////////////////////////////////
//PlaybackChannelProxy
class PlaybackChannelProxy : public yunos::DProxy, public PlaybackChannelInterface {
public:
    PlaybackChannelProxy(const yunos::SharedPtr<yunos::DService>& service,
        yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg);
    virtual ~PlaybackChannelProxy();

public:
    virtual mm_status_t registerCallbackListener(const char*channelCbId);
    virtual mm_status_t unregisterCallbackListener(const char*channelCbId);
    virtual std::string getPlaybackRemoteId();
    virtual mm_status_t setUsages(int32_t usage);
    virtual mm_status_t setEnabled(bool enabled);
    virtual mm_status_t setPlaybackState(PlaybackStateSP state);
    virtual mm_status_t setPlaylist(int32_t playlist);
    virtual mm_status_t setPlaylistTitle(const char*title);
    virtual mm_status_t setMetadata(MediaMetaSP meta);
    virtual void onDeath(const DLifecycleListener::DeathInfo& deathInfo);

private:
    PlaybackChannelImp *mImp;
    MM_DISALLOW_COPY(PlaybackChannelProxy)
    DECLARE_LOGTAG()
};


//////////////////////////////////////////////////////////////////////
//PlaybackChannelImp
class PlaybackChannelImp : public PlaybackChannel {
public:
    PlaybackChannelImp(const char*tag, const char* packageName);
    virtual ~PlaybackChannelImp();

public:
    bool connect();
    void destroy();
    virtual std::string getPlaybackRemoteId();
    virtual mm_status_t setUsages(int32_t usage);
    virtual mm_status_t setEnabled(bool enabled);
    virtual bool isEnabled();
    virtual mm_status_t setPlaybackState(PlaybackStateSP state);
    virtual mm_status_t setPlaylist(int32_t playlist);
    virtual mm_status_t setPlaylistTitle(const char*title);
    virtual mm_status_t setMetadata(MediaMetaSP meta);
    virtual void dump();
    virtual mm_status_t setListener(Listener * listener);
    mm_status_t handleMsg(MMParamSP param);
    bool handleSignal(const yunos::SharedPtr<yunos::DMessage>& msg);

private:
    mm_status_t onPlaybackEvent(PlaybackEventSP event);
    static std::string msgName(int msg);
    static std::string methodName(int method);

private:
    friend class PlaybackChannel::Listener;
    friend class UBusMessage<PlaybackChannelImp>;
    friend class PlaybackChannelProxy;

    Listener *mListener;
    PlaybackStateSP mPlaybackState;
    MediaMetaSP mMediaMeta;
    PlaybackChannelCNSP mClientNode;
    bool mEnabled;
    PlaybackChannelManagerSP mManager;

    std::string mChannelId;
    std::string mTag;
    std::string mPackageName;
    int32_t mUserId;
    int32_t mUsages;

    Lock mLock;
    yunos::SharedPtr<UBusMessage<PlaybackChannelImp> > mMessager;

    MM_DISALLOW_COPY(PlaybackChannelImp)
    DECLARE_LOGTAG()
};

}

#endif //__PLAYBACK_CHANNEL_IMP_H__
