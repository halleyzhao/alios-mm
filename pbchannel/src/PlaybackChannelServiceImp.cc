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
#include <algorithm>
#include <Permission.h>

#include <multimedia/PlaybackChannelService.h>
#include "PlaybackChannelServiceImp.h"
#include "multimedia/UBusNode.h"
#include "multimedia/UBusHelper.h"


namespace YUNOS_MM {
DEFINE_LOGTAG1(PlaybackChannelServiceImp, [PC])
DEFINE_LOGTAG1(PlaybackChannelServiceAdaptor, [PC])

// tmp add to debug PlaybackChannelManager:dispatchMediakey timeout
#ifdef DEBUG
#undef DEBUG
#define DEBUG INFO
#endif


using namespace yunos;
#define PLAYBACK_REMOTE_PERMISSION_NAME  "MEDIA_REMOTE.permission.yunos.com"

#define ADJUST_TOGGLE_MUTE 101
/////////////////////////////////////////////////////////////////////////////////
//RecentTaskHelper
#define MAX_RECENT_TASK 10
class RecentTaskHelper : public BasePtr {
public:
    RecentTaskHelper(PlaybackChannelServiceImp *self) : mOwner(self) {
    }
    void getRecentTasksCB(int32_t error,
            const std::list<SharedPtr<DPMSProxy::RecentTaskInfo> >& result) {
            if (!mOwner)
                return;
            mOwner->getRecentTasksCB(error, result);
    }

    PlaybackChannelServiceImp *mOwner;

};


#define PLAYBACK_CHANNEL_PERMISSION_CHECK() do{                                                                               \
    if (!perm.checkPermission(msg, this)) {                                                                                \
        ERROR("operation not allowed, client (pid %d) need permission[%s]", msg->getPid(), PLAYBACK_REMOTE_PERMISSION_NAME); \
        reply->writeInt32(MM_ERROR_PERMISSION_DENIED);                                                                     \
        sendMessage(reply);                                                                                                \
        return false;                                                                                                      \
    }                                                                                                                      \
}while(0)

#define CHECK_LOOP() \
    Looper* looper = mService->mServiceNode->myLooper();\
    if (!looper) {\
        ERROR("invalid loop");\
        return;\
    }


////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelServiceImp
PlaybackChannelServiceImp::PlaybackChannelServiceImp() :  mCurrentUserId(0)
                                                         , mUniqueId(1)
                                                         , mCondition(mTaskLock)
{
    mOrderList.reset(new PlaybackRecordList());

    mRecentTaskHelper  = new RecentTaskHelper(this);
    mLooperThread = new LooperThread();

    if (mLooperThread->start("getRecentTask")) {
        ERROR("failed to start\n");
        mLooperThread = NULL;
        return;
    }
}

PlaybackChannelServiceImp::~PlaybackChannelServiceImp()
{
    mServiceNode.reset();

    if (mLooperThread) {
        DEBUG("exit getRecentTask thread waiting ... ");
        mLooperThread->requestExitAndWait();
        DEBUG("exit getRecentTask thread end");
    }
}

// run on PlaybackChannelServiceAdaptor msg thread
mm_status_t PlaybackChannelServiceImp::addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid)
{
    MMAutoLock locker(mLock);

    ChannelInfo info;
    info.mPackageName = packageName;
    info.mTag = tag;
    info.mCallingPid = callingPid;
    info.mUserId = userId;
    info.mChannelId = "undefined container channel";
    info.mIsHost = false;

    mChannelInfoByName.insert(std::pair<std::string, ChannelInfo>(packageName, info));
    return MM_ERROR_SUCCESS;
}

// run on PlaybackChannelServiceAdaptor msg thread
void PlaybackChannelServiceImp::removeChannelInfo(const char*packageName)
{
    MMAutoLock locker(mLock);
    auto it = mChannelInfoByName.find(std::string(packageName));
    if (it == mChannelInfoByName.end()) {
        ERROR("invalid packageName %s", packageName);
        return;
    }

    DEBUG("packageName %s removed, host %d, channelInfo.size %d",
        packageName, it->second.mIsHost, mChannelInfoByName.size());
    mChannelInfoByName.erase(it);
}


// run on PlaybackChannelServiceAdaptor msg thread
std::string PlaybackChannelServiceImp::createChannel(const char* tag, const char*packageName, int32_t userId,
                                    pid_t pid, uid_t uid)
{
    MMAutoLock locker(mLock);
    DEBUG("tag: %s, packageName: %s, userId: %d, before add to mAllChannels, size %d, pid %d, uid %d\n",
        tag, packageName, userId, mAllChannels.size(), pid, uid);
    // add for resource limit
    if (mAllChannels.size() > 64) {
        ERROR("too many channels, exceed the max number");
        return INVALID_CHANNEL_ID;
    }
    PlaybackRecordSP record;
    record.reset(new PlaybackRecord(tag, packageName, userId, pid, uid, this));
    if (!record || record->init() != MM_ERROR_SUCCESS) {
        ERROR("create channel failed");
        return INVALID_CHANNEL_ID;
    }

    mAllChannels.insert(std::pair<std::string, PlaybackRecordSP>(record->mChannelId, record));
    mOrderList->addChannel(record.get());

    //todo:remove hard code in futrue once updateUser is implement

    mCurrentUserId = userId;
    notifyEnabledChannels();

    ChannelInfo info;
    info.mPackageName = packageName;
    info.mTag = tag;
    info.mCallingPid = -1;
    info.mUserId = userId;
    info.mChannelId = record->mChannelId;
    info.mIsHost = true;
    mChannelInfoByName.insert(std::pair<std::string, ChannelInfo>(packageName, info));
    DEBUG("tag: %s, packageName: %s, channelId %s, userId: %d, after add to mAllChannels, size %d\n",
        tag, packageName, record->getChannelId().c_str(), userId, mAllChannels.size());

    return record->getChannelId();
}

// run on PlaybackChannelServiceAdaptor msg thread
void PlaybackChannelServiceImp::destroyChannel(const char *channelId, pid_t pid, uid_t uid)
{
    MMAutoLock locker(mLock);
    DEBUG("channelId %s, before erase from mAllChannels, size %d, pid %d, uid %d",
        channelId, mAllChannels.size(), pid, uid);

    auto it = mAllChannels.find(std::string(channelId));
    if (it == mAllChannels.end()) {
        DEBUG("cannot find record %s\n", channelId);
        return;
    }
    PlaybackRecord *record = it->second.get();
    if (!record) {
        ERROR("invalid record, channelId %s", channelId);
        return;
    }
    if (record->getPid() != pid) {
        ERROR("invalid pid %d, channel pid %d", pid, record->mPid);
        return;
    }
    if (record->getUid() != uid) {
        ERROR("invalid uid %d, channel uid %d", uid, record->mUid);
        return;
    }

    // get userId first before PlaybackRecord destroy
    // int32_t userId = record->getUserId();

    mOrderList->removeChannel(record);

    record->destroy();

    auto it1 = mChannelInfoByName.find(std::string(record->mPackageName.c_str()));
    if (it1 == mChannelInfoByName.end()) {
        DEBUG("cannot find record %s, packageName %s\n", channelId, record->mPackageName.c_str());
        return;
    }
    mChannelInfoByName.erase(it1);

    // Remove this channel specify by channelId
    mAllChannels.erase(it);

    // PlaybackRecord is destroyed
    // notify to MediaRemote enable channels changed
    notifyEnabledChannels();
    DEBUG("channelId %s, after erase from mAllChannels, size %d", channelId, mAllChannels.size());
}

// run on PlaybackChannelServiceAdaptor msg thread
mm_status_t PlaybackChannelServiceImp::updateChannel(const char* channelId)
{
    MMAutoLock locker(mLock);
    auto it = mAllChannels.find(std::string(channelId));
    if (it == mAllChannels.end()) {
        DEBUG("cannot find record %s\n", channelId);
        return MM_ERROR_UNKNOWN;
    }
    PlaybackRecord *record = it->second.get();
    if (!record) {
        ERROR("invalid record, channelId %s", channelId);
        return MM_ERROR_UNKNOWN;
    }

    mOrderList->updateGlobalPriorityChannel(record);
    notifyEnabledChannels();
    return MM_ERROR_SUCCESS;
}

// run on PlaybackChannelServiceAdaptor msg thread
mm_status_t PlaybackChannelServiceImp::onChannelPlaystateChange(const char*channelId,
    int32_t oldState, int32_t newState)
{
    MMAutoLock locker(mLock);
    auto it = mAllChannels.find(std::string(channelId));
    if (it == mAllChannels.end()) {
        DEBUG("cannot find record %s\n", channelId);
        return MM_ERROR_UNKNOWN;
    }
    PlaybackRecord *record = it->second.get();
    if (!record) {
        ERROR("invalid record, channelId %s", channelId);
        return MM_ERROR_UNKNOWN;
    }
    #if 0
    bool update = mOrderList->playstateChange(record, oldState, newState);

    if (update) {
        notifyEnabledChannels();
    }
    #endif
    return MM_ERROR_SUCCESS;
}

// run on PlaybackChannelServiceAdaptor msg thread
mm_status_t PlaybackChannelServiceImp::notifyEnabledChannels()
{
    DEBUG("mCallbackName %s", mCallbackName.c_str());
    if (mCallbackName.isNull()) {
        VERBOSE("mCallbackName is empty, return");
        return MM_ERROR_SUCCESS;
    }

    std::vector<std::string> remoteIds;
    mm_status_t status = mServiceNode->node()->getEnabledChannels(remoteIds);
    if (status != MM_ERROR_SUCCESS) {
        DEBUG("getEnabledChannels failed");
        return MM_ERROR_UNKNOWN;
    }

    status = mServiceNode->node()->notify(remoteIds);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("failed");
    }
    DEBUG("remoteIds.size %d", remoteIds.size());
    return MM_ERROR_SUCCESS;
}

bool PlaybackChannelServiceImp::publish()
{
    try {
        mServiceNode = ServiceNode<PlaybackChannelServiceAdaptor>::create("mssa", NULL, this);
        if (!mServiceNode->init()) {
            ERROR("failed to init PlaybackChannelServiceAdaptor\n");
            mServiceNode.reset();
            return false;
        }

        INFO("PlaybackChannelServiceAdaptor pulish success\n");
        return true;
    } catch (...) {
        ERROR("no mem\n");
        return false;
    }
}

void PlaybackChannelServiceImp::getRecentTasksCB(int32_t error,
            const std::list<SharedPtr<DPMSProxy::RecentTaskInfo> >& result)
{
    mTaskInfo.clear();
    DEBUG("tasks size %d", result.size());
    if (result.empty()) {
        goto SIGNAL;
    }

    if (error == 0) {
        for(auto& task : result) {
            mTaskInfo.push_back(TaskInfo());
            TaskInfo &info = mTaskInfo[mTaskInfo.size()-1];

            info.mIsHost = task->isHost();
            if (info.mIsHost) {
                const std::list<String> &pageUriList = task->runningPageUIList();
                if (!pageUriList.empty()) {
                    for (auto it = pageUriList.begin(); it != pageUriList.end(); it++) {
                        info.mPageName.push_back(it->c_str());
                    }
                }
            } else {
                info.mPageName.push_back(task->packageName().c_str());
            }
            info.mFirstActiveTime = task->firstActiveTime();
            info.mLastActiveTime = task->lastActiveTime();
        }
    } else {
        ERROR("error %d", error);
    }

    dumpTaskAndChannels();

SIGNAL:
    {
        MMAutoLock lock(mTaskLock);
        mCondition.signal();
    }
}

/*static*/void PlaybackChannelServiceImp::getRecentTasks_s(void *arg)
{
    SharedPtr<DPMSProxy> dpmsProxy = DPMSProxy::getInstance();
    ASSERT(dpmsProxy);
    PlaybackChannelServiceImp *imp = static_cast<PlaybackChannelServiceImp*>(arg);
    bool ret = dpmsProxy->getRecentTasks(MAX_RECENT_TASK, 0,
        makeCallback(&RecentTaskHelper::getRecentTasksCB, imp->mRecentTaskHelper));
    if (ret) {
        ERROR("failed, ret %d", ret);
        MMAutoLock lock(imp->mTaskLock);
        imp->mCondition.signal();
    }
}

void PlaybackChannelServiceImp::getRecentTasks()
{
    MMAutoLock lock(mTaskLock);
    bool ret = mLooperThread->sendTask(Task(getRecentTasks_s, this));
    ASSERT(ret);
    DEBUG("waitting ...\n");
    mCondition.timedWait(300*1000);
    DEBUG("end");
}

PlaybackChannelServiceImp::ChannelInfo* PlaybackChannelServiceImp::getDefaultChannelInfo(bool checkEnabled)
{
    // recent tasks may be empty sometimes
    // ASSERT(mTaskInfo.size());
    ASSERT(mChannelInfoByName.size());

    ChannelInfo *found = NULL;
    if (mTaskInfo.empty()) {
        return found;
    }
    for (auto it = mTaskInfo.begin(); it != mTaskInfo.end(); it++) {
        const std::vector<std::string> &pageNameList = it->mPageName;
        for (uint32_t i = 0; i < pageNameList.size(); i++) {
            const char*pageName = pageNameList[i].c_str();
            for (auto iit = mChannelInfoByName.begin(); iit != mChannelInfoByName.end(); iit++) {
                const char*packageName = iit->first.c_str();
                if (strstr(pageName, packageName) != NULL) {
                    DEBUG("tasks contain %s and mChannelInfoByName contain %s", pageName, packageName);
                    if (!found) {
                        found = &(iit->second);
                        INFO("found one record %p", found);
                    }
                    if (!checkEnabled) {
                        return &(iit->second);
                    } else {
                        auto j = mAllChannels.find(iit->second.mChannelId);
                        ASSERT(j != mAllChannels.end());
                        if (j->second->isEnabled()) {
                            return &(iit->second);
                        }
                    }
                }
            }
        }
    }

    // If no record is found, may be 2 case as following:
    //   1. mChannelInfoByName element is not contained by mTaskInfo
    //   2. mChannelInfoByName element is contained by mTaskInfo, but this element is not enabled(checkEnabled is true)
    return found;
}

PlaybackRecord* PlaybackChannelServiceImp::getDefaultChannel()
{
    // 1. first check global priority channel
    // If global channel exist, dispatch playback key to this channel directly
    if (mOrderList->isGlobalPriorityEnabled()) {
        return mOrderList->globalPriorityChannel();
    }

    // 2. check the recenttask list
    if (!mTaskInfo.empty()) {
        ChannelInfo *info = getDefaultChannelInfo(true);
        if (info) {
            std::string id = info->mChannelId;
            auto j = mAllChannels.find(id);
            ASSERT(j != mAllChannels.end());
            return j->second.get();
        }
    }

    // 3. recentTask is empty or pbchannel in recenttask is disabled
    // dispatch playback event to enabled channel
    return mOrderList->getDefaultChannel(true); //get enabled channel
}

bool PlaybackChannelServiceImp::isHandledByHost(PlaybackEventSP key)
{
    // If recentTasks is emtpy,
    // Media key will be handled by host if focus window is host
    // Meida key will be handled by container if focus window is container
    if (mTaskInfo.empty()) {
        INFO("task is empty");
        return isMediaKeyFromHost(key);
    }

    ChannelInfo *info = getDefaultChannelInfo(false);
    if (info != NULL) {
        DEBUG("media key dispatch to %s", info->mIsHost ? "host" : "container");
        return info->mIsHost;
    }

    WARNING("mTaskInfo and mChannelInfoByName have no common packageName");
    // No channels is enable, host/container will handle this media key if focus window is host/container
    return isMediaKeyFromHost(key);
}

bool PlaybackChannelServiceImp::isMediaKeyFromHost(PlaybackEventSP key)
{
    return key->isFromHost();
}

void PlaybackChannelServiceImp::dumpTaskAndChannels()
{
    if (mChannelInfoByName.empty())
        return;
    DEBUG("dump mChannelInfoByName, size %d", mChannelInfoByName.size());
    for (auto it = mChannelInfoByName.begin(); it != mChannelInfoByName.end(); it++) {
        DEBUG("  [%s, host: %d]", it->first.c_str(), it->second.mIsHost);
    }

    if (mTaskInfo.empty())
        return;
    DEBUG("dump mTaskInfo, size %d", mTaskInfo.size());
    for (auto it = mTaskInfo.begin(); it != mTaskInfo.end(); it++) {
        for (auto i = it->mPageName.begin(); i != it->mPageName.end(); i++) {
            DEBUG("  [%s, host: %d]", i->c_str(), it->mIsHost);
        }

    }
}
////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelServiceAdaptor
PlaybackChannelServiceAdaptor::PlaybackChannelServiceAdaptor(const yunos::SharedPtr<yunos::DService>& service,
    yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg)
        : yunos::DAdaptor(service, yunos::String(PlaybackChannelManagerInterface::pathName())
        , yunos::String(PlaybackChannelManagerInterface::iface()))
        , mService(static_cast<PlaybackChannelServiceImp*>(arg))
        , mMsgCondition(mMsgLock)

{
    INFO("+\n");
    INFO("-\n");
}

PlaybackChannelServiceAdaptor::~PlaybackChannelServiceAdaptor()
{
    INFO("+\n");
    // MM_RELEASE(mListener);
    INFO("-\n");
}

void PlaybackChannelServiceAdaptor::destroyChannel_loop(const char* channelId, pid_t pid, uid_t uid)
{
    CHECK_LOOP();
    // MMAutoLock lock(mMsgLock);
    looper->sendTask(Task(PlaybackChannelServiceAdaptor::destroyChannel_loop_s, this, channelId, pid, uid));
    // mMsgCondition.wait();
}

void PlaybackChannelServiceAdaptor::updateChannel_loop(const char* channelId)
{
    CHECK_LOOP();
    MMAutoLock lock(mMsgLock);
    looper->sendTask(Task(PlaybackChannelServiceAdaptor::updateChannel_loop_s, this, channelId));
    mMsgCondition.wait();
}

void PlaybackChannelServiceAdaptor::stateChange_loop(const char* channelId, int32_t oldState, int32_t newState)
{
    CHECK_LOOP();
    MMAutoLock lock(mMsgLock);
    looper->sendTask(Task(PlaybackChannelServiceAdaptor::stateChange_loop_s, this, channelId, oldState, newState));
    mMsgCondition.wait();
}


/*static*/
void PlaybackChannelServiceAdaptor::destroyChannel_loop_s(PlaybackChannelServiceAdaptor*p, const char*channelId,
                                                            pid_t pid, uid_t uid)
{
    // MMAutoLock lock(p->mMsgLock);
    if (!p || !p->mService) {
        ERROR("adaptor is null");
        return;
    }

    p->mService->destroyChannel(channelId, pid, uid);
    // p->mMsgCondition.signal();
}

/*static*/
void PlaybackChannelServiceAdaptor::updateChannel_loop_s(PlaybackChannelServiceAdaptor*p, const char*channelId)
{
    MMAutoLock lock(p->mMsgLock);
    if (!p || !p->mService) {
        ERROR("adaptor is null");
        return;
    }

    p->mService->updateChannel(channelId);
    p->mMsgCondition.signal();
}

/*static*/
void PlaybackChannelServiceAdaptor::stateChange_loop_s(PlaybackChannelServiceAdaptor*p, const char*channelId,
                                                    int32_t oldState, int32_t newState)
{
    MMAutoLock lock(p->mMsgLock);
    if (!p || !p->mService) {
        ERROR("adaptor is null");
        return;
    }

    p->mService->onChannelPlaystateChange(channelId, oldState, newState);
    p->mMsgCondition.signal();
}

bool PlaybackChannelServiceAdaptor::handleMethodCall(const yunos::SharedPtr<yunos::DMessage>& msg)
{
    static PermissionCheck perm(String(PLAYBACK_REMOTE_PERMISSION_NAME));

    mm_status_t status = MM_ERROR_SUCCESS;
    yunos::SharedPtr<yunos::DMessage> reply = yunos::DMessage::makeMethodReturn(msg);

    pid_t pid = msg->getPid();
    uid_t uid = msg->getUid();
    INFO("(pid: %d, uid: %d, interface: %s) call %s",
        pid, uid, interface().c_str(), msg->methodName().c_str());

    if (msg->methodName() == "addChannelInfo") {
        std::string tag = msg->readString().c_str();
        std::string packageName = msg->readString().c_str();
        int32_t userId = msg->readInt32();
        int32_t callingPid = msg->readInt32();

        mm_status_t status = mService->addChannelInfo(tag.c_str(), packageName.c_str(), userId, callingPid);
        reply->writeInt32(status);
    } else if (msg->methodName() == "removeChannelInfo") {
        std::string name = msg->readString().c_str();
        mService->removeChannelInfo(name.c_str());

    } else if (msg->methodName() == "createChannel") {
        std::string tag = msg->readString().c_str();
        std::string packageName = msg->readString().c_str();
        int32_t usrId = msg->readInt32();

        DEBUG("createChannel %s %s %d\n", tag.c_str(), packageName.c_str(), usrId);
        std::string channelId;

        {
            // MMAutoLock locker(mService->mLock);
            channelId = mService->createChannel(tag.c_str(), packageName.c_str(), usrId, pid, uid);
        }
        DEBUG("channel proxy id %s", channelId.c_str());
        reply->writeString(yunos::String(channelId.c_str()));

    } else if (msg->methodName() == "destroyChannel") {
        std::string channelId = msg->readString().c_str();

        DEBUG("destroyChannel channelId %s\n", channelId.c_str());

        {
            // MMAutoLock locker(mService->mLock);
            mService->destroyChannel(channelId.c_str(), pid, uid);
        }
    } else if (msg->methodName() == "dispatchPlaybackEvent") {
        DEBUG("dispatchPlaybackEvent\n");
        PlaybackEventSP event  = PlaybackEvent::create();
        event->readFromMsg(msg);
        bool needWakeLock = msg->readBool();
        status = dispatchPlaybackEvent(event, needWakeLock);
        reply->writeInt32(status);
    } else if (msg->methodName() == "getEnabledChannels") {
        PLAYBACK_CHANNEL_PERMISSION_CHECK();

        std::vector<std::string> remoteIds;
        mm_status_t ret = getEnabledChannels(remoteIds);

        reply->writeInt32((int32_t)ret);
        if (ret == MM_ERROR_SUCCESS) {
            reply->writeInt32(remoteIds.size());
            for (auto it = remoteIds.begin(); it != remoteIds.end(); it++) {
                reply->writeString(yunos::String((*it).c_str()));
            }
        }
    } else if (msg->methodName() == "addChannelsListener") {
        PLAYBACK_CHANNEL_PERMISSION_CHECK();

        mService->mCallbackName = msg->readString();
        DEBUG("addChannelsListener, mCallbackName: %s, userId %d", mService->mCallbackName.c_str());
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "dumpsys") {
        DEBUG("dumpsys");
        std::string info = dumpsys();
        reply->writeString(yunos::String(info.c_str()));
    } else {
        ERROR("unknown call: %s\n", msg->methodName().c_str());
        status = MM_ERROR_UNSUPPORTED;
    }

    sendMessage(reply);
    return true;
}

bool PlaybackChannelServiceAdaptor::init()
{
    INFO("+\n");
    return true;
}

mm_status_t PlaybackChannelServiceAdaptor::notify(std::vector<std::string> &remoteIds)
{
    INFO();
    if (!mService->mCallbackName.isNull()) {
        SharedPtr<DMessage> signal = obtainSignalMessage(mService->mCallbackName);
        if (!signal) {
            ERROR("fail to create signal for callback, name %s", mService->mCallbackName.c_str());
            return MM_ERROR_UNKNOWN;
        }
        // msg type
        signal->writeInt32((int32_t)MM_ERROR_SUCCESS);
        signal->writeInt32(remoteIds.size());
        for (auto it = remoteIds.begin(); it != remoteIds.end(); it++) {
            signal->writeString(yunos::String((*it).c_str()));
        }

        sendMessage(signal);
    }

    return MM_ERROR_SUCCESS;
}

mm_status_t PlaybackChannelServiceAdaptor::dispatchPlaybackEvent(PlaybackEventSP pbEvent, bool needWakeLock)
{
    if (!PlaybackEvent::isMediaKey(pbEvent->getKey())) {
        ERROR("non-media key event.");
        return MM_ERROR_INVALID_PARAM;
    }


    uint64_t nowMs;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    nowMs = t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
    mService->getRecentTasks(); // this method will cost some times
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t diff = t.tv_sec * 1000LL + t.tv_nsec / 1000000LL - nowMs;
    DEBUG("getRecentTasks cost %lld ms", diff);

#ifdef __USING_PLAYBACK_CHANNEL_FUSION__
    if (/*!PlaybackEvent::isVoiceKey(pbEvent->getKey()) && */!mService->isHandledByHost(pbEvent)) {
        INFO("dispatch media key to container");
        return MM_ERROR_NOT_HANDLED;
    }
#endif


    PlaybackRecord *record = mService->getDefaultChannel();

    return dispatchPlaybackEvent(pbEvent, needWakeLock, record);
}

mm_status_t PlaybackChannelServiceAdaptor::dispatchPlaybackEvent(
    PlaybackEventSP pbEvent, bool needWakeLock, PlaybackRecord* record)
{

    if (record != NULL) {
        // record->dump();
        if (needWakeLock) {
            // todo: add power lock here
            DEBUG("pow lock acquire");
        }
        pbEvent->dump();

        record->sendPlaybackEvent(pbEvent);

        if (needWakeLock) {
            // todo: release power lock here?? but send mediabutton not finished yet
            DEBUG("pow lock release");
        }
    } else {
        DEBUG("Mediabutton receiver not implement");
#if 1
        yunos::SharedPtr<DPMSProxy> proxy  = DPMSProxy::getInstance();
        yunos::SharedPtr<yunos::PageLink> pl = new yunos::PageLink();
        pl->setEventName(yunos::String("event.media.button.yunos.com"));
        pl->setUri(Uri("page://broadcast.yunos.com"));
        proxy->sendLink(pl);
        DEBUG("sendlink media button success");
#endif
    }

    return MM_ERROR_SUCCESS;
}

mm_status_t PlaybackChannelServiceAdaptor::getEnabledChannels(std::vector<std::string> &remoteIds)
{
    std::list<PlaybackRecord*> channels = mService->mOrderList->getEnabledChannels();
    DEBUG("channels size %d, enable channels size %d",
        mService->mAllChannels.size(), channels.size());

    for (auto it = channels.begin(); it != channels.end(); it++) {
        PlaybackRecord *record = *it;
        if (!record) {
            ERROR("invalid record, channelId %s", record->mChannelId.c_str());
            return MM_ERROR_UNKNOWN;
        }
        remoteIds.push_back(std::string(record->mRemoteId.c_str()));
    }
    return MM_ERROR_SUCCESS;
}

std::string PlaybackChannelServiceAdaptor::dumpsys()
{
    std::string info;
    #if defined(__MM_PROD_MODE_ON__)
        INFO("in product mode, return");
        return info;
    #endif

    char tmp[2048] = {0};
    info.append("################PlaybackChannelService dump info############\n");
    int len = sprintf(tmp, "mCallbackName: %s\n", mService->mCallbackName.isNull() ? "null" : mService->mCallbackName.c_str());
    if (!mService->mChannelInfoByName.empty()) {
        for (auto it = mService->mChannelInfoByName.begin(); it != mService->mChannelInfoByName.end(); it++) {
            len += sprintf(tmp+len, "packageName %s, host %d\n", it->first.c_str(), it->second.mIsHost);
        }
    }
    DEBUG("dump mTaskInfo");

    info.append(tmp);
    info.append(mService->mOrderList->dump("  "));
    info.append("################PlaybackChannelService dump end############\n");
    return info;
}


}
