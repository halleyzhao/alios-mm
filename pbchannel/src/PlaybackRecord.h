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

#ifndef __PLAYBACK_RECORD_H__
#define __PLAYBACK_RECORD_H__
#include <string>

#include <semaphore.h>

#include <dbus/DAdaptor.h>
#include <dbus/DService.h>
#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/PlaybackEvent.h>
#include <multimedia/PlaybackState.h>
#include <multimedia/PlaybackChannelInterface.h>
#include <multimedia/PlaybackRemoteInterface.h>

namespace YUNOS_MM {

class PlaybackRecord;
typedef MMSharedPtr<PlaybackRecord> PlaybackRecordSP;
class PlaybackRemoteAdaptor;
typedef MMSharedPtr<PlaybackRemoteAdaptor> PlaybackRemoteAdaptorSP;
class PlaybackChannelAdaptor;
typedef MMSharedPtr<PlaybackChannelAdaptor> PlaybackChannelAdaptorSP;
template <class UBusType> class ServiceNode;

typedef MMSharedPtr<ServiceNode<PlaybackRemoteAdaptor> > PlaybackRemoteSNSP;
typedef MMSharedPtr<ServiceNode<PlaybackChannelAdaptor> > PlaybackChannelSNSP;

class PlaybackChannelServiceImp;
class PlaybackRecord {
public:
    PlaybackRecord(const char *tag, const char *packageName, int32_t userId, pid_t pid, uid_t uid,
        PlaybackChannelServiceImp *service);
    ~PlaybackRecord();

public:
    std::string getChannelId();
    bool isPlaybackActive();
    std::string getTag();
    int32_t getUsages();
    int32_t getUserId();
    bool isEnabled();
    void destroy();
    void sendPlaybackEvent(PlaybackEventSP pbEvent);

public:
    static int64_t getTimeUs();
    std::string getUniqueId();
    std::string dump(const char *prefix = "");
    mm_status_t init();
    void uinit();

    uid_t getUid() { return mUid; }
    pid_t getPid() { return mPid; }

private:
    void channelDestroyNotify_l();
    void playbackStateNotify();
    void metadataUpdateNotify();
    void titleChangeNotify();
    void queueChangeNotify();

private:
    friend class PlaybackRemoteAdaptor;
    friend class PlaybackChannelAdaptor;
    friend class PlaybackChannelServiceAdaptor;
    friend class PlaybackChannelServiceImp;
    PlaybackChannelServiceImp *mService = NULL;

    std::string mTag;
    int32_t mUserId = 0;
    int32_t mUsages = 0;

    bool mEnabled = false;
    bool mChannelDestroyed = false;
    MediaMetaSP mMediaMeta;;
    PlaybackStateSP mPlaybackState;
    int32_t mPlaylist = -1;
    yunos::String mPlaylistTitle;
    yunos::String mPackageName;
    int64_t mLastActiveTime;

    PlaybackChannelSNSP mChannelServiceNode;
    PlaybackRemoteSNSP mRemoteServiceNode;

    // node will be released in UBusNode::~~UBusNode
    std::string mChannelId;
    std::string mRemoteId;
    Lock mLock; //protect mMediaMeta/mPlaybackState/mChannelDestoryed

    pid_t mPid = -1;
    uid_t mUid = -1;

private:
    MM_DISALLOW_COPY(PlaybackRecord)
    DECLARE_LOGTAG()
};

////////////////////////////////////////////////////////////////////////////////////////
class PlaybackRemoteAdaptor : public yunos::DAdaptor, public PlaybackRemoteInterface {
public:
    PlaybackRemoteAdaptor(const yunos::SharedPtr<yunos::DService>& service,
        yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg);
    virtual ~PlaybackRemoteAdaptor();

public:
    virtual bool handleMethodCall(const yunos::SharedPtr<yunos::DMessage>& msg);
    virtual void onDeath(const DLifecycleListener::DeathInfo& deathInfo);
    virtual void onBirth(const DLifecycleListener::BirthInfo& brithInfo);
    bool init() { return true; }

    virtual MediaMetaSP getMetadata(){ return MediaMetaSP((MediaMeta*)NULL); }
    virtual PlaybackStateSP getPlaybackState(){ return PlaybackStateSP((PlaybackState*)NULL); }
    virtual int getPlaylist(){ return 0; } //FIXME: add playlist support
    virtual std::string getPlaylistTitle(){ std::string str; return str; }
    virtual std::string getPackageName(){ std::string str; return str; }
    virtual int32_t getUsages(){ return 0; }
    virtual std::string getTag(){ std::string str; return str; }
    virtual mm_status_t sendPlaybackEvent(PlaybackEventSP event){ return MM_ERROR_SUCCESS; }

    // for transport remote
    virtual mm_status_t play(){ return MM_ERROR_SUCCESS; }
    virtual mm_status_t pause(){ return MM_ERROR_SUCCESS; }
    virtual mm_status_t seekTo(int64_t /*ms*/){ return MM_ERROR_SUCCESS; }
    virtual mm_status_t stop(){ return MM_ERROR_SUCCESS; }
    virtual mm_status_t next(){ return MM_ERROR_SUCCESS; }
    virtual mm_status_t previous(){ return MM_ERROR_SUCCESS; }
    virtual mm_status_t fastForward(){ return MM_ERROR_SUCCESS; }
    virtual mm_status_t rewind(){ return MM_ERROR_SUCCESS; }

    static void notify_loop_s(PlaybackRemoteAdaptor* p, int msg, int32_t param1, int64_t param2);
    void notify(int msg, int32_t param1, int64_t param2);
    void notify_loop(int msg, int32_t param1, int64_t param2);
    std::string msgName(int msg);

protected:


private:
    yunos::String mUniqueId;
    PlaybackRecord *mRecord;
    yunos::String mRemoteCbName;

    DECLARE_LOGTAG();
    MM_DISALLOW_COPY(PlaybackRemoteAdaptor);
};

//////////////////////////////////////////////////////////////////////////
class PlaybackChannelAdaptor : public yunos::DAdaptor, public PlaybackChannelInterface {
public:
    PlaybackChannelAdaptor(const yunos::SharedPtr<yunos::DService>& service,
        yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg);
    virtual ~PlaybackChannelAdaptor();

public:
    virtual bool handleMethodCall(const yunos::SharedPtr<yunos::DMessage>& msg);
    virtual void onDeath(const DLifecycleListener::DeathInfo& deathInfo);
    virtual void onBirth(const DLifecycleListener::BirthInfo& brithInfo);
    bool init() { return true; }
    static void notify_loop_s(PlaybackChannelAdaptor* p, int msg, int32_t param1, int64_t param2, PlaybackEventSP key);
    void notify(int msg, int32_t param1, int64_t param2, PlaybackEventSP event);
    void notify_loop(int msg, int32_t param1, int64_t param2, PlaybackEventSP event);
    std::string msgName(int msg);

public:

    virtual std::string getPlaybackRemoteId() { std::string str; return str; }
    virtual mm_status_t setUsages(int32_t usage) { return MM_ERROR_SUCCESS; }
    virtual mm_status_t setEnabled(bool enabled) { return MM_ERROR_SUCCESS; }
    virtual bool isEnabled() { return false; }
    virtual mm_status_t setPlaybackState(PlaybackStateSP state) { return MM_ERROR_SUCCESS; }
    virtual mm_status_t setPlaylist(int32_t playlist) { return MM_ERROR_SUCCESS; }
    virtual mm_status_t setPlaylistTitle(const char*title) { return MM_ERROR_SUCCESS; }
    virtual mm_status_t setMetadata(MediaMetaSP meta) { return MM_ERROR_SUCCESS; }


private:
    yunos::String mUniqueId;
    PlaybackRecord *mRecord;
    PlaybackChannelServiceImp *mService;
    yunos::String mChannelCbName;

private:

    DECLARE_LOGTAG();
    MM_DISALLOW_COPY(PlaybackChannelAdaptor);
};

}

#endif //__PLAYBACK_RECORD_H__
