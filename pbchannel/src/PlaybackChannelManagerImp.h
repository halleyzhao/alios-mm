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

#ifndef __PLAYBACK_CHANNEL_MANAGER_IMP_H__
#define __PLAYBACK_CHANNEL_MANAGER_IMP_H__
#include <dbus/dbus.h>
#include <dbus/DProxy.h>
#include <dbus/DService.h>
#include <pointer/SharedPtr.h>
#include <dbus/DAdaptor.h>
#include <dbus/DService.h>
#include <dbus/DSignalCallback.h>

#include <multimedia/PlaybackChannelManager.h>
#include <thread/LooperThread.h>

#ifdef __USING_PLAYBACK_CHANNEL_FUSION__
#include <ContainerSessionManagerWrapper.h>
#endif

namespace YUNOS_MM {
class PlaybackChannelManagerImp;
class PlaybackRemoteProxy;
class PlaybackChannelManagerProxy;
template <class ClientType> class UBusMessage;
template <class UBusType> class ClientNode;
typedef MMSharedPtr<ClientNode<PlaybackChannelManagerProxy> > PlaybackChannelManagerCNSP;


////////////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelManagerProxy
class PlaybackChannelManagerProxy : public yunos::DProxy, public PlaybackChannelManagerInterface {
public:
    PlaybackChannelManagerProxy(const yunos::SharedPtr<yunos::DService>& service,
        yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg);
    virtual ~PlaybackChannelManagerProxy();

public:
    virtual mm_status_t dispatchPlaybackEvent(PlaybackEventSP event, bool needWakeLock = false);
    virtual mm_status_t getEnabledChannels(std::vector<std::string> &remoteIds);

public:
    virtual std::string dumpsys();

protected:
    friend class PlaybackChannelManagerImp;
    virtual std::string createChannel(const char* tag, const char*packageName,  int32_t userId);
    virtual void destroyChannel(const char* channelId);
    virtual mm_status_t addChannelsListener(const char*channelListenerId);
    virtual mm_status_t addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid);
    virtual void removeChannelInfo(const char*packageName);

private:
    MM_DISALLOW_COPY(PlaybackChannelManagerProxy)
    DECLARE_LOGTAG()
};

////////////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelManagerImp
class PlaybackChannelManagerImp : public PlaybackChannelManager {
public:
    PlaybackChannelManagerImp();
    virtual ~PlaybackChannelManagerImp();

public:
    bool connect();

    virtual mm_status_t dispatchPlaybackEvent(PlaybackEventSP event, bool needWakeLock = false);
    virtual mm_status_t getEnabledChannels(std::vector<PlaybackRemoteSP> &remotes);
    virtual mm_status_t getEnabledChannels(std::vector<std::string> &remoteIds);
    virtual mm_status_t addListener(Listener * listener);
    virtual void removeListener(Listener * listener);
    mm_status_t handleMsg(MMParamSP param);
    bool handleSignal(const yunos::SharedPtr<yunos::DMessage>& msg);

public:
    virtual std::string dumpsys();

protected:
    friend class PlaybackChannelImp;
    virtual std::string createChannel(const char* tag, const char*packageName,  int32_t userId);
    virtual void destroyChannel(const char* channelId);
    virtual mm_status_t addChannelsListener(const char*callbackName) { return MM_ERROR_SUCCESS; }
    virtual mm_status_t addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid);
    virtual void removeChannelInfo(const char*packageName);

private:
    static std::string msgName(int msg);
    friend class Listener;
    friend class UBusMessage<PlaybackChannelManagerImp>;
    PlaybackChannelManagerCNSP mClientNode;

    std::map<Listener*, int32_t> mListener; // second is userId
    Lock mLock;
    yunos::SharedPtr<UBusMessage<PlaybackChannelManagerImp> > mMessager;
    std::vector<std::string> mRemoteIds;
#ifdef __USING_PLAYBACK_CHANNEL_FUSION__
    ContainerSessionManagerWrapperSP mContainerWrapper;
#endif

private:
    MM_DISALLOW_COPY(PlaybackChannelManagerImp)
    DECLARE_LOGTAG()
};



}

#endif //__PLAYBACK_CHANNEL_MANAGER_IMP_H__
