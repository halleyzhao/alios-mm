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

#ifndef __PLAYBACK_CHANNEL_SERVICE_IMP_H__
#define __PLAYBACK_CHANNEL_SERVICE_IMP_H__
#include <dbus/DAdaptor.h>
#include <dbus/DService.h>
#include <thread/LooperThread.h>
#include <DPMSProxy.h>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include "multimedia/PlaybackChannelManagerInterface.h"
#include "multimedia/PlaybackChannelService.h"
#include "PlaybackRecordList.h"

namespace YUNOS_MM {
class PlaybackChannelServiceImp;
typedef MMSharedPtr<PlaybackChannelServiceImp> PlaybackChannelServiceImpSP;
class PlaybackChannelServiceAdaptor;
typedef MMSharedPtr<PlaybackChannelServiceAdaptor> PlaybackChannelServiceAdaptorSP;
template <class UBusType> class ServiceNode;
typedef MMSharedPtr<ServiceNode<PlaybackChannelServiceAdaptor> > PlaybackChannelServiceSNSP;


//////////////////////////////////////////////////////////////////////
//PlaybackChannelServiceAdaptor
class PlaybackChannelServiceAdaptor : public yunos::DAdaptor, public PlaybackChannelManagerInterface {
public:
    PlaybackChannelServiceAdaptor(const yunos::SharedPtr<yunos::DService>& service,
        yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg);
    virtual ~PlaybackChannelServiceAdaptor();

public:
    virtual bool handleMethodCall(const yunos::SharedPtr<yunos::DMessage>& msg);
    bool init();
    mm_status_t notify(std::vector<std::string> &remoteIds);
    virtual mm_status_t dispatchPlaybackEvent(PlaybackEventSP event, bool needWakeLock = false);
    virtual mm_status_t getEnabledChannels(std::vector<std::string> &remoteIds);
    virtual mm_status_t addChannelsListener(const char*callbackName) { return MM_ERROR_SUCCESS; }

public:
    std::string dumpsys();

protected:
    virtual std::string createChannel(const char* tag, const char*packageName, int32_t userId) { std::string str; return str; }
    virtual void destroyChannel(const char*ChannelId) {}
    virtual mm_status_t addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid) {return MM_ERROR_UNSUPPORTED;}
    virtual void removeChannelInfo(const char*packageName) {}

    void destroyChannel_loop(const char* channelId, pid_t pid, uid_t uid);
    void updateChannel_loop(const char* channelId);
    void stateChange_loop(const char* channelId, int32_t oldState, int32_t newState);
    static void destroyChannel_loop_s(PlaybackChannelServiceAdaptor*p, const char*channelId, pid_t pid, uid_t uid);
    static void updateChannel_loop_s(PlaybackChannelServiceAdaptor*p, const char*channelId);
    static void stateChange_loop_s(PlaybackChannelServiceAdaptor*p, const char*channelId, int32_t oldState, int32_t newState);

private:
    mm_status_t dispatchPlaybackEvent(
        PlaybackEventSP pbEvent, bool needWakeLock, PlaybackRecord* record);

protected:


private:
    friend class PlaybackChannelServiceImp;
    friend class PlaybackChannelAdaptor;
    yunos::String mUniqueId;
    PlaybackChannelServiceImp *mService;
    Lock mMsgLock;
    Condition mMsgCondition;

    DECLARE_LOGTAG();
    MM_DISALLOW_COPY(PlaybackChannelServiceAdaptor);
};

////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelServiceImp
class PlaybackRecord;
class RecentTaskHelper;
class PlaybackChannelServiceImp : public PlaybackChannelService {
public:
    PlaybackChannelServiceImp();

    virtual ~PlaybackChannelServiceImp();

public:
    bool publish();

private:
    friend class PlaybackRecord;
    virtual mm_status_t addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid);
    virtual void removeChannelInfo(const char*packageName);
    virtual std::string createChannel(const char* tag, const char *packageName, int32_t userId, pid_t pid, uid_t uid);
    virtual void destroyChannel(const char* channelId, pid_t pid, uid_t uid);
    virtual mm_status_t updateChannel(const char* channelId);
    virtual mm_status_t notifyEnabledChannels();
    virtual mm_status_t onChannelPlaystateChange(const char*channelId, int32_t oldState, int32_t newState);
    static void getRecentTasks_s(void *);
    void getRecentTasks();
    void getRecentTasksCB(int32_t error, const std::list<yunos::SharedPtr<yunos::DPMSProxy::RecentTaskInfo> >& result);
    void dumpTaskAndChannels();
    bool isHandledByHost(PlaybackEventSP key);
    bool isMediaKeyFromHost(PlaybackEventSP key);
    struct ChannelInfo{
        std::string mPackageName;
        std::string mTag; //for debug
        int32_t mCallingPid;
        int32_t mUserId;
        bool mIsHost; //whether the channel is host or not.
        std::string mChannelId;
    };
    ChannelInfo* getDefaultChannelInfo(bool checkEnabled = false);
    PlaybackRecord* getDefaultChannel();
private:
    friend class PlaybackChannelServiceAdaptor;
    friend class PlaybackChannelAdaptor;
    friend class RecentTaskHelper;

    PlaybackChannelServiceSNSP mServiceNode;
    std::map<std::string/*channel id*/, PlaybackRecordSP> mAllChannels;
    PlaybackRecordListSP mOrderList;
    int32_t mCurrentUserId;
    int32_t mUniqueId;
    Lock mLock;
    yunos::String mCallbackName;
    struct TaskInfo {
        bool mIsHost; //task is host or plugin
        std::vector<std::string> mPageName;
        uint64_t mFirstActiveTime;
        uint64_t mLastActiveTime;
    };

    std::map<std::string/*packageName*/, ChannelInfo> mChannelInfoByName; //channels from host and plugin
    std::vector<TaskInfo> mTaskInfo; //store recent tasks

    // for getRecentTask
    yunos::SharedPtr<RecentTaskHelper> mRecentTaskHelper;
    yunos::SharedPtr<yunos::LooperThread> mLooperThread;
    Lock mTaskLock;
    Condition mCondition;
    DECLARE_LOGTAG()
    MM_DISALLOW_COPY(PlaybackChannelServiceImp)
};

}

#endif //__PLAYBACK_CHANNEL_SERVICE_IMP_H__
