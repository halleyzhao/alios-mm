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

#ifndef __PLAYBACK_REMOTE_IMP_H__
#define __PLAYBACK_REMOTE_IMP_H__
#include <dbus/dbus.h>
#include <dbus/DProxy.h>
#include <dbus/DService.h>
#include <pointer/SharedPtr.h>
#include <dbus/DAdaptor.h>
#include <dbus/DService.h>
#include <dbus/DSignalCallback.h>
#include <thread/LooperThread.h>
#include <multimedia/PlaybackRemote.h>

namespace YUNOS_MM {
class PlaybackRemoteImp;
class PlaybackRemoteProxy;
template <class ClientType> class UBusMessage;
template <class UBusType> class ClientNode;
typedef MMSharedPtr<ClientNode<PlaybackRemoteProxy> > PlaybackRemoteCNSP;


//////////////////////////////////////////////////////////////////
//PlaybackRemoteProxy
class PlaybackRemoteProxy : public yunos::DProxy, public PlaybackRemoteInterface {
public:
    PlaybackRemoteProxy(const yunos::SharedPtr<yunos::DService>& service,
        yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg);
    virtual ~PlaybackRemoteProxy();

public:

    virtual mm_status_t registerCallbackListener(const char*controlCbId);
    virtual mm_status_t unregisterCallbackListener(const char*controlCbId);
    virtual MediaMetaSP getMetadata();
    virtual PlaybackStateSP getPlaybackState();
    virtual int getPlaylist(); //FIXME: add playlist support
    virtual std::string getPlaylistTitle();
    virtual std::string getPackageName();
    virtual int32_t getUsages();
    virtual std::string getTag();
    virtual mm_status_t sendPlaybackEvent(PlaybackEventSP event);

    // for transport remote
    virtual mm_status_t play();
    virtual mm_status_t pause();
    virtual mm_status_t seekTo(int64_t /*ms*/);
    virtual mm_status_t stop();
    virtual mm_status_t next();
    virtual mm_status_t previous();
    virtual mm_status_t fastForward();
    virtual mm_status_t rewind();
    #if 0
    virtual void onDeath(const DLifecycleListener::DeathInfo& deathInfo);
    #endif
private:
    PlaybackRemoteImp *mImp;
    MM_DISALLOW_COPY(PlaybackRemoteProxy)
    DECLARE_LOGTAG()
};

//////////////////////////////////////////////////////////////////////
//PlaybackRemoteImp
class PlaybackRemoteImp : public PlaybackRemote {
public:
    PlaybackRemoteImp();
    virtual ~PlaybackRemoteImp();

public:
    bool connect(const char*remoteId);

    virtual MediaMetaSP getMetadata();
    virtual PlaybackStateSP getPlaybackState();
    virtual int getPlaylist(); //FIXME: add playlist support
    virtual std::string getPlaylistTitle();
    virtual std::string getPackageName();
    virtual int32_t getUsages();
    virtual std::string getTag();
    virtual mm_status_t sendPlaybackEvent(PlaybackEventSP event);

    // for transport remote
    virtual mm_status_t play();
    virtual mm_status_t pause();
    virtual mm_status_t seekTo(int64_t /*ms*/);
    virtual mm_status_t stop();
    virtual mm_status_t next();
    virtual mm_status_t previous();
    virtual mm_status_t fastForward();
    virtual mm_status_t rewind();

public:
    virtual mm_status_t addListener(Listener * listener);
    virtual void removeListener(Listener *listener);
    mm_status_t handleMsg(MMParamSP param);
    bool handleSignal(const yunos::SharedPtr<yunos::DMessage>& msg);
    virtual void dump();

protected:
    friend class Listener;
    std::list<Listener*> mListener;

private:
    static std::string msgName(int msg);
    static std::string methodName(int method);
    friend class UBusMessage<PlaybackRemoteImp>;
    friend class PlaybackRemoteProxy;

    PlaybackRemoteCNSP mClientNode;
    std::string mRemoteId;

    Lock mLock;
    yunos::SharedPtr<UBusMessage<PlaybackRemoteImp> > mMessager;

    MM_DISALLOW_COPY(PlaybackRemoteImp)
    DECLARE_LOGTAG()
};

}

#endif //__PLAYBACK_REMOTE_IMP_H__
