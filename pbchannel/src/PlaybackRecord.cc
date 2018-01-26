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
#include <unistd.h>
#include <algorithm>
#include <dbus/dbus.h>
#include <dbus/DAdaptor.h>
#include <Permission.h>

#include <multimedia/mm_cpp_utils.h>
#include <multimedia/media_attr_str.h>

#include <multimedia/mm_debug.h>
#include "multimedia/PlaybackChannel.h"
#include "multimedia/PlaybackRemote.h"
#include "multimedia/UBusHelper.h"
#include "multimedia/UBusNode.h"
#include "PlaybackRecord.h"
#include "PlaybackChannelServiceImp.h"


using namespace yunos;

namespace YUNOS_MM {
static PlaybackEventSP nilEvent;

DEFINE_LOGTAG1(PlaybackRecord, [PC])
DEFINE_LOGTAG1(PlaybackChannelAdaptor, [PC])
DEFINE_LOGTAG1(PlaybackRemoteAdaptor, [PC])

#define MSG_NAME(MSG) case (MSG): return #MSG;

PlaybackRecord::PlaybackRecord(const char *tag, const char *packageName,
    int32_t userId, pid_t pid, uid_t uid, PlaybackChannelServiceImp *service) :
          mService(service)
        , mTag(tag)
        , mPackageName(packageName)
        , mPid(pid)
        , mUid(uid)
{
    DEBUG("+");
    mMediaMeta = MediaMeta::create();
    DEBUG("-");
}

mm_status_t PlaybackRecord::init() {

    mRemoteId = getUniqueId();
    mRemoteServiceNode = ServiceNode<PlaybackRemoteAdaptor>::create("mca", mRemoteId.c_str(), this);
    if (!mRemoteServiceNode->init()) {
        ERROR("failed to init\n");
        mRemoteServiceNode.reset();
        return MM_ERROR_UNKNOWN;
    }
    INFO("create PlaybackRemoteAdaptor success, mRemoteId: %s\n", mRemoteId.c_str());

    mChannelId = getUniqueId();
    mChannelServiceNode = ServiceNode<PlaybackChannelAdaptor>::create("msa", mChannelId.c_str(), this);
    if (!mChannelServiceNode->init()) {
        ERROR("failed to init\n");
        mChannelServiceNode.reset();
        mRemoteServiceNode.reset();
        return MM_ERROR_UNKNOWN;
    }
    INFO("create PlaybackChannelAdaptor success, mChannelId: %s\n", mChannelId.c_str());
    return MM_ERROR_SUCCESS;
}

void PlaybackRecord::uinit()
{
    MMAutoLock locker(mLock);
    DEBUG();
    mRemoteServiceNode.reset();
    mChannelServiceNode.reset();
}


PlaybackRecord::~PlaybackRecord() {
    // called in PlaybackChannelService loop thread
    uinit();
}

std::string PlaybackRecord::getUniqueId()
{
    int32_t num = mService->mUniqueId++;
    char buf[16] = {0};
    sprintf(buf, "%d", num);
    std::string o = buf;
    DEBUG("getUniqueId %s", o.c_str());
    return o;
}

/*static*/ int64_t PlaybackRecord::getTimeUs() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000LL + t.tv_nsec / 1000LL;
}


std::string PlaybackRecord::getChannelId() {
    return mChannelId;
}

bool PlaybackRecord::isPlaybackActive() {
    PlaybackState::State state = mPlaybackState ? mPlaybackState->getState() : PlaybackState::State::kNone;
    if (PlaybackState::isActiveState(state)) {
        return true;
    }
    return false;
}

std::string PlaybackRecord::getTag() {
    return mTag;
}


int32_t PlaybackRecord::getUsages() {
    return mUsages;
}

int32_t PlaybackRecord::getUserId() {
    return mUserId;
}

bool PlaybackRecord::isEnabled() {
    return mEnabled && !mChannelDestroyed;
}

// called in PlaybackChannelService loop thread
void PlaybackRecord::destroy() {
    MMAutoLock locker(mLock);
    if (mChannelDestroyed) {
        return;
    }
    mChannelDestroyed = true;

    // Notify PlaybackRemotes the channel is destroy
    channelDestroyNotify_l();
}

// Notify PlaybackRemotes the channel is destroy
// called in PlaybackChannelService loop thread
void PlaybackRecord::channelDestroyNotify_l()
{
    if (!mRemoteServiceNode)
        return;

    mRemoteServiceNode->node()->notify(PlaybackRemote::Listener::MsgType::kMsgChannelDestroyed,
        0, 0);
}

// called in PlaybackChannelService looper thread
void PlaybackRecord::sendPlaybackEvent(PlaybackEventSP pbEvent) {
    if (!mChannelServiceNode)
        return;
    DEBUG("mChannelServiceNode %p, tag %s", mChannelServiceNode.get(), mTag.c_str());
    mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgMediaButton, 0, 0, pbEvent);
}

#define CHECK_SESSEN_VALID() \
    MMAutoLock locker(mLock); \
    DEBUG("+");\
    if (mChannelDestroyed)\
        return;\
    if (!mRemoteServiceNode)\
        return;\

// called in PlaybackChannel looper thread
void PlaybackRecord::playbackStateNotify()
{
    //protected mChannelDestroyed/mRemoteServiceNode
    CHECK_SESSEN_VALID();

    mRemoteServiceNode->node()->notify(PlaybackRemote::Listener::MsgType::kMsgPlaybackStateChanged,
        0, (int64_t)mPlaybackState.get());
}

// called in PlaybackChannel looper thread
void PlaybackRecord::metadataUpdateNotify()
{
    CHECK_SESSEN_VALID();

    mRemoteServiceNode->node()->notify(PlaybackRemote::Listener::MsgType::kMsgMetadataChanged,
        0, (int64_t)mMediaMeta.get());
}


// called in PlaybackChannel looper thread
void PlaybackRecord::titleChangeNotify()
{
    CHECK_SESSEN_VALID();

    mRemoteServiceNode->node()->notify(PlaybackRemote::Listener::MsgType::kMsgPlaylistTitleChanged,
        0, (int64_t)mPlaylistTitle.c_str());
}

// called in PlaybackChannel looper thread
void PlaybackRecord::queueChangeNotify()
{
    CHECK_SESSEN_VALID();

    mRemoteServiceNode->node()->notify(PlaybackRemote::Listener::MsgType::kMsgPlaylistChanged,
        mPlaylist, 0);
}

std::string PlaybackRecord::dump(const char *prefix) {
    std::string info;
    char tmp[1024] = {0};
    char newPrefix[64] = {0};
    sprintf(newPrefix, "%s  ", prefix);

    int len = sprintf(tmp, "%smTag: %s\n", prefix, mTag.c_str());
    len += sprintf(tmp + len, "%smPackageName: %s\n", prefix, mPackageName.c_str());
    len += sprintf(tmp + len, "%smPlaylistTitle: %s\n", prefix, mPlaylistTitle.c_str());
    len += sprintf(tmp + len, "%smUserId: %d\n", prefix, mUserId);
    len += sprintf(tmp + len, "%smUsages: 0x%0x\n", prefix, mUsages);
    len += sprintf(tmp + len, "%smEnabled: %d\n", prefix, mEnabled);
    len += sprintf(tmp + len, "%smChannelDestroyed: %d\n", prefix, mChannelDestroyed);
    len += sprintf(tmp + len, "%smChannelId: %s\n", prefix, mChannelId.c_str());
    len += sprintf(tmp + len, "%smRemoteId: %s\n", prefix, mRemoteId.c_str());
    len += sprintf(tmp + len, "%sPlaybackState dump info:\n", prefix);
    info.append(tmp);
    DEBUG("%s", tmp);

    if (mPlaybackState) {
        info.append(mPlaybackState->dump(newPrefix));
    }

#if 0
    if (mMediaMeta) {
        info.append(mMediaMeta->dump(newPrefix));
    }
#endif
    return info;
}


///////////////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelAdaptor
PlaybackChannelAdaptor::PlaybackChannelAdaptor(const yunos::SharedPtr <yunos::DService> &service,
            yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg)
            : yunos::DAdaptor(service, pathName, iface)
            , mUniqueId(serviceName.substr(strlen(PlaybackChannelInterface::serviceName())))
            , mRecord(static_cast<PlaybackRecord*>(arg))
{
    INFO("+\n");
    ASSERT(mRecord);
    mService = mRecord->mService;
    DEBUG("mUniqueId %s", mUniqueId.c_str());
    ASSERT(mService);
    INFO("-\n");
}

PlaybackChannelAdaptor::~PlaybackChannelAdaptor() {
    INFO("+\n");
    // MM_RELEASE(mListener);
    INFO("-\n");
}

/* static */
void PlaybackChannelAdaptor::notify_loop_s(PlaybackChannelAdaptor* p, int msg, int32_t param1, int64_t param2, PlaybackEventSP key)
{
    if (!p) {
        ERROR("adaptor is null");
        return;
    }

    p->notify_loop(msg, param1, param2, key);
}

// run on PlaybackChannel loop thread
void PlaybackChannelAdaptor::notify(int msg, int32_t param1, int64_t param2, PlaybackEventSP event)
{
    if (!mRecord) {
        ERROR("record is null");
        return;
    }
    Looper* looper = mRecord->mChannelServiceNode->myLooper();
    if (looper) {
        looper->sendTask(Task(PlaybackChannelAdaptor::notify_loop_s, this, msg, param1, param2, event));
    } else {
        ERROR("NULL looper");
    }
}

std::string PlaybackChannelAdaptor::msgName(int msg)
{
    switch (msg) {
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgPlay);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgPause);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgStop);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgNext);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgPrevious);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgFastForward);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgRewind);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgSeekTo);
        MSG_NAME(PlaybackChannel::Listener::MsgType::kMsgMediaButton);
        default: return "unknown msg";
    }
    return "unknown msg";
}

// run on PlaybackChannel loop thread
void PlaybackChannelAdaptor::notify_loop(int msg, int32_t param1, int64_t param2, PlaybackEventSP event)
{
    INFO("msg %s, param1 %d", msgName(msg).c_str(), param1);
    if (!mChannelCbName.isNull()) {
        SharedPtr<DMessage> signal = obtainSignalMessage(mChannelCbName);
        if (!signal) {
            ERROR("fail to create signal for callback, name %s", mChannelCbName.c_str());
            return;
        }
        // msg type
        signal->writeInt32(msg);
        if (msg == PlaybackChannel::Listener::MsgType::kMsgSeekTo) {
            DEBUG("seekto %" PRId64 "", param2);
            signal->writeInt64(param2);
        } else if (msg == PlaybackChannel::Listener::MsgType::kMsgMediaButton) {
            ASSERT(event);
            event->writeToMsg(signal);
        } else {
            // do nothing
        }
        sendMessage(signal);
    }
}

bool PlaybackChannelAdaptor::handleMethodCall(const yunos::SharedPtr <yunos::DMessage> &msg) {
    mm_status_t status = MM_ERROR_SUCCESS;
    yunos::SharedPtr <yunos::DMessage> reply = yunos::DMessage::makeMethodReturn(msg);

    if (msg->methodName() == "registerCallbackListener") {
        mChannelCbName = msg->readString().c_str();
        DEBUG("registerCallbackListener, mChannelCbName: %s", mChannelCbName.c_str());
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "setUsages") {
        int32_t usage = msg->readInt32();
        DEBUG("setUsages, flag %d\n", usage);
        mRecord->mUsages = usage;

        // update channel run on PlaybackChannelServiceAdaptor loop thread
        mService->mServiceNode->node()->updateChannel_loop(mRecord->mChannelId.c_str());
        reply->writeInt32(status);
    } else if (msg->methodName() == "setEnabled") {
        bool enabled = msg->readBool();
        DEBUG("setEnabled, enabled %d\n", enabled);
        mRecord->mEnabled = enabled;

        // update channel run on PlaybackChannelServiceAdaptor loop thread
        mService->mServiceNode->node()->updateChannel_loop(mRecord->mChannelId.c_str());
        reply->writeInt32(status);
    } else if (msg->methodName() == "setPlaybackState") {
        PlaybackStateSP state = PlaybackState::create();
        state->readFromMsg(msg);
        state->dump();
        int32_t oldState = mRecord->mPlaybackState == NULL ? PlaybackState::State::kNone : (int32_t)mRecord->mPlaybackState->getState();
        int32_t newState = (int32_t)state->getState();
        if (PlaybackState::isActiveState((PlaybackState::State)oldState) &&
            newState == PlaybackState::State::kPaused) {
            mRecord->mLastActiveTime = PlaybackRecord::getTimeUs();
        }

        {
            MMAutoLock locker(mRecord->mLock);
            mRecord->mPlaybackState = state;
        }
        ASSERT(mService != NULL);
        // update state run on PlaybackChannelServiceAdaptor loop thread
        mService->mServiceNode->node()->stateChange_loop(mRecord->mChannelId.c_str(), oldState, newState);

        mRecord->playbackStateNotify();
        reply->writeInt32(status);
    } else if (msg->methodName() == "setPlaylist") {
        // add mLock protected in future
        mRecord->mPlaylist = -1; //fix in future
        DEBUG("setPlaylist\n");
        mRecord->queueChangeNotify();
        reply->writeInt32(status);
    } else if (msg->methodName() == "setPlaylistTitle") {
        // add mLock protected in future
        mRecord->mPlaylistTitle = msg->readString();
        DEBUG("ChannelAdaptor setPlaylistTitle: %s", mRecord->mPlaylistTitle.c_str());
        mRecord->titleChangeNotify();
        reply->writeInt32(status);
    } else if (msg->methodName() == "setMetadata") {
        DEBUG("setMetadata\n");
        MediaMetaSP meta = MediaMeta::create();
        meta->readFromMsg(msg);
        meta->dump();
        //todo: add lock, for PlaybackChannel::setMediaMeta and PlaybackRemote::getMetadata
        {
            MMAutoLock locker(mRecord->mLock);
            mRecord->mMediaMeta->merge(meta);
        }
        mRecord->metadataUpdateNotify();
        reply->writeInt32(status);
    } else if (msg->methodName() == "getPlaybackRemoteId") {
        DEBUG("getPlaybackRemoteId: %s\n", mRecord->mRemoteId.c_str());
        reply->writeString(yunos::String(mRecord->mRemoteId.c_str()));
    } else {
        ERROR("unknown call: %s\n", msg->methodName().c_str());
        status = MM_ERROR_UNSUPPORTED;
        reply->writeInt32((int32_t)status);
    }

    sendMessage(reply);
    return true;
}

void PlaybackChannelAdaptor::onDeath(const DLifecycleListener::DeathInfo& deathInfo)
{
    INFO("client die: %s, mSeq %d",
        deathInfo.mName.c_str(), deathInfo.mSeq);
    DAdaptor::onDeath(deathInfo);
    /// MMAutoLock locker(mRecord->mService->mLock);
    if (mRecord && mRecord->mService) {
        // destroy channel run on PlaybackChannelServiceAdaptor loop thread
        mRecord->mService->mServiceNode->node()->destroyChannel_loop(mRecord->mChannelId.c_str(),
        mRecord->getPid(), mRecord->getUid());
    }
}

void PlaybackChannelAdaptor::onBirth(const DLifecycleListener::BirthInfo& brithInfo)
{
    DAdaptor::onBirth(brithInfo);
    INFO("client birth, nName %s, mSeq %d",
        brithInfo.mName.c_str(), brithInfo.mSeq);
}


///////////////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelAdaptor
PlaybackRemoteAdaptor::PlaybackRemoteAdaptor(const yunos::SharedPtr <yunos::DService> &service,
            yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg)
            : yunos::DAdaptor(service, pathName, iface)
            , mUniqueId(serviceName.substr(strlen(PlaybackRemoteInterface::serviceName())))
            , mRecord(static_cast<PlaybackRecord*>(arg))
{
    INFO("+\n");
    DEBUG("mUniqueId %s", mUniqueId.c_str());
    ASSERT(mRecord);
    INFO("-\n");
}

PlaybackRemoteAdaptor::~PlaybackRemoteAdaptor() {
    INFO("+\n");
    // MM_RELEASE(mListener);
    INFO("-\n");
}

/* static */
void PlaybackRemoteAdaptor::notify_loop_s(PlaybackRemoteAdaptor* p, int msg, int32_t param1, int64_t param2)
{
    if (!p) {
        ERROR("adaptor is null");
        return;
    }

    p->notify_loop(msg, param1, param2);
}

void PlaybackRemoteAdaptor::notify(int msg, int32_t param1, int64_t param2)
{
    if (!mRecord) {
        ERROR("record is null");
        return;
    }
    Looper* looper = mRecord->mRemoteServiceNode->myLooper();
    if (looper) {
        looper->sendTask(Task(PlaybackRemoteAdaptor::notify_loop_s, this, msg, param1, param2));
    } else {
        ERROR("NULL looper");
    }
}

// run on PlaybackRemoteAdaptor loop thread
void PlaybackRemoteAdaptor::notify_loop(int msg, int32_t param1, int64_t param2)
{
    INFO("msg %s, param1 %d, param2 %p", msgName(msg).c_str(), param1, (void*)param2);
    if (!mRemoteCbName.isNull()) {
        SharedPtr<DMessage> signal = obtainSignalMessage(mRemoteCbName);
        if (!signal) {
            ERROR("fail to create signal for callback, name %s", mRemoteCbName.c_str());
            return;
        }
        // msg type
        signal->writeInt32(msg);
        if (msg == PlaybackRemote::Listener::MsgType::kMsgPlaybackStateChanged) {
            PlaybackState *state = (PlaybackState*)param2;
            ASSERT(state);
            state->writeToMsg(signal);
        } else if (msg == PlaybackRemote::Listener::MsgType::kMsgMetadataChanged) {
            MediaMeta *meta = (MediaMeta*)param2;
            ASSERT(meta);
            meta->writeToMsg(signal);
        } else if (msg == PlaybackRemote::Listener::MsgType::kMsgPlaylistTitleChanged) {
            char *title = (char*)param2;
            ASSERT(title);
            signal->writeString(String(title));
        } else if (msg == PlaybackRemote::Listener::MsgType::kMsgPlaylistChanged) {
            signal->writeInt32(param1);
        } else if (msg == PlaybackRemote::Listener::MsgType::kMsgChannelDestroyed) {
            mRemoteCbName.reset();
            DEBUG("channel destroy, reset callback name");
        } else {
            // do nothing
        }
        sendMessage(signal);
    }
}

std::string PlaybackRemoteAdaptor::msgName(int msg)
{
    switch (msg) {
        MSG_NAME(PlaybackRemote::Listener::MsgType::kMsgChannelDestroyed);
        MSG_NAME(PlaybackRemote::Listener::MsgType::kMsgChannelEvent);
        MSG_NAME(PlaybackRemote::Listener::MsgType::kMsgPlaybackStateChanged);
        MSG_NAME(PlaybackRemote::Listener::MsgType::kMsgMetadataChanged);
        MSG_NAME(PlaybackRemote::Listener::MsgType::kMsgPlaylistChanged);
        MSG_NAME(PlaybackRemote::Listener::MsgType::kMsgPlaylistTitleChanged);
       default: return "unknown msg";
    }
    return "unknown msg";
}


bool PlaybackRemoteAdaptor::handleMethodCall(const yunos::SharedPtr <yunos::DMessage> &msg) {
    mm_status_t status = MM_ERROR_SUCCESS;
    yunos::SharedPtr <yunos::DMessage> reply = yunos::DMessage::makeMethodReturn(msg);
    if (msg->methodName() == "registerCallbackListener") {
        mRemoteCbName = msg->readString();

        reply->writeInt32((int32_t)status);
        MMAutoLock locker(mRecord->mLock);
        if (mRecord->mChannelDestroyed) {
            DEBUG("channel is destroy, notify to PlaybackRemote");
            mRecord->mRemoteServiceNode->node()->notify(PlaybackRemote::Listener::MsgType::kMsgChannelDestroyed,
                0, 0);
        }
    } else if (msg->methodName() == "getMetadata") {
        // MediaRemote::getPlaybackState and MediaSesson:SetPlaybackState should be protected by lock
        DEBUG("getMetadata\n");
        MMAutoLock locker(mRecord->mLock);
        if (mRecord->mMediaMeta) {
            status = MM_ERROR_SUCCESS;
            reply->writeInt32((int32_t)status);
            mRecord->mMediaMeta->writeToMsg(reply);
        } else {
            status = MM_ERROR_AGAIN;
            reply->writeInt32((int32_t)status);
        }
    } else if (msg->methodName() == "getPlaybackState") {
        DEBUG("getPlaybackState\n");

        // MediaRemote::getPlaybackState and MediaSesson:SetPlaybackState should be protected by lock
        MMAutoLock locker(mRecord->mLock);
        PlaybackStateSP state = mRecord->mPlaybackState;
        state->dump();
        int64_t duration = -1;

        MediaMetaSP meta = mRecord->mMediaMeta;
        if (meta != NULL && meta->getInt64(MEDIA_ATTR_DURATION, duration)) {
            DEBUG("ChannelRemoteAdaptor: getPlaybackState, duration: %" PRId64 "", duration);
        }

        bool isUpdate = false;
        if (state != NULL) {
            if (state->getState() == PlaybackState::State::kPlaying ||
                state->getState() == PlaybackState::State::kFastForwarding ||
                state->getState() == PlaybackState::State::kRewinding) {
                int64_t updateTime = state->getUpdateTime();
                int64_t currentTime = PlaybackRecord::getTimeUs();
                if (updateTime > 0) {
                    int64_t position = (int64_t(state->getSpeed() * (currentTime - updateTime)) +
                        state->getPosition())/1000LL; //in ms
                    DEBUG("pos: %" PRId64 "", position);
                    if (duration >= 0 && position > duration) {
                        position = duration;
                    } else if (position < 0) {
                        position = 0;
                    }
                    PlaybackStateSP newState = PlaybackState::create();
                    newState->setState(state->getState(), position, state->getSpeed(),
                        currentTime);

                    reply->writeInt32((int32_t)status);
                    newState->writeToMsg(reply);
                    isUpdate = true;
                }
            }

            if (!isUpdate) {
                reply->writeInt32((int32_t)status);
                state->writeToMsg(reply);
            }
        } else {
            reply->writeInt32((int32_t)MM_ERROR_UNKNOWN);
        }
    } else if (msg->methodName() == "getPlaylist") {
        DEBUG("getPlaylist\n");
        status = MM_ERROR_SUCCESS;
        // mPlaylist should be protected by mLock in future
        reply->writeInt32(-1);
    } else if (msg->methodName() == "getPlaylistTitle") {
        DEBUG("getPlaylistTitle\n");
        // add mLock protected in future
        status = MM_ERROR_SUCCESS;
        reply->writeString(mRecord->mPlaylistTitle);
    } else if (msg->methodName() == "getPackageName") {
        DEBUG("getPackageName\n");

        status = MM_ERROR_SUCCESS;
        reply->writeString(mRecord->mPackageName);
    } else if (msg->methodName() == "getUsages") {
        DEBUG("getUsages\n");

        status = MM_ERROR_SUCCESS;
        reply->writeInt32(mRecord->mUsages);
    } else if (msg->methodName() == "getTag") {
        DEBUG("getTag\n");

        status = MM_ERROR_SUCCESS;
        reply->writeString(yunos::String(mRecord->mTag.c_str()));
    } else if (msg->methodName() == "sendPlaybackEvent") {
        PlaybackEventSP event = PlaybackEvent::create();
        event->readFromMsg(msg);

        DEBUG("sendPlaybackEvent\n");
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgMediaButton, 0, 0, event);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "play") {
        DEBUG("play\n");
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgPlay, 0 , 0, nilEvent);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "pause") {
        DEBUG("pause\n");

        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgPause, 0 , 0, nilEvent);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "seekTo") {
        int64_t pos = msg->readInt64();
        DEBUG("seekTo: %" PRId64 "\n", pos);
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgSeekTo, 0 , pos, nilEvent);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "stop") {
        DEBUG("stop\n");
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgStop, 0 , 0, nilEvent);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "next") {
        DEBUG("next\n");
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgNext, 0 , 0, nilEvent);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "previous") {
        DEBUG("previous\n");
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgPrevious, 0 , 0, nilEvent);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "fastForward") {
        DEBUG("fastForward\n");
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgFastForward, 0 , 0, nilEvent);
        reply->writeInt32((int32_t)status);
    } else if (msg->methodName() == "rewind") {
        DEBUG("rewind\n");
        mRecord->mChannelServiceNode->node()->notify(PlaybackChannel::Listener::MsgType::kMsgRewind, 0 , 0, nilEvent);
        reply->writeInt32((int32_t)status);
    } else {
        ERROR("unknown call: %s\n", msg->methodName().c_str());
        status = MM_ERROR_UNSUPPORTED;
        reply->writeInt32((int32_t)status);
    }

    sendMessage(reply);
    return true;
}

void PlaybackRemoteAdaptor::onDeath(const DLifecycleListener::DeathInfo& deathInfo)
{
    DAdaptor::onDeath(deathInfo);
    INFO("client die, nName %s, mSeq %d",
        deathInfo.mName.c_str(), deathInfo.mSeq);
}

void PlaybackRemoteAdaptor::onBirth(const DLifecycleListener::BirthInfo& brithInfo)
{
    DAdaptor::onBirth(brithInfo);
    INFO("client birth, nName %s, mSeq %d",
        brithInfo.mName.c_str(), brithInfo.mSeq);
}

}
