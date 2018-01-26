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
#include <dbus/DSignalRule.h>

#include <multimedia/mm_debug.h>
#include <multimedia/mm_cpp_utils.h>
#include "PlaybackRemoteImp.h"
#include "UBusHelper.h"
#include "multimedia/UBusNode.h"
#include "UBusMessage.h"

namespace YUNOS_MM {
using namespace yunos;

DEFINE_LOGTAG1(PlaybackRemoteImp, [PC])
DEFINE_LOGTAG1(PlaybackRemoteProxy, [PC])

enum {
    MSG_GET_MEDIAMETA = 0,
    MSG_GET_PALYBACKSTATE,
    MSG_GET_PLAYLIST,
    MSG_GET_PLAYLIST_TITLE,
    MSG_GET_USAGES,
    MSG_GET_TAG,
    MSG_DISPATCH_PLAYBACK_EVENT,
    MSG_ADD_LISTENER,
    MSG_REMOVE_LISTENER,
    MSG_PLAY,
    MSG_PAUSE,
    MSG_SEEKTO,
    MSG_STOP,
    MSG_NEXT,
    MSG_PREVIOUS,
    MSG_FASTFORWARD,
    MSG_REWIND,
    MSG_GET_PACKAGE_NAME
};
static int mTimeOut = -1;

///////////////////////////////////////////////////////////////////////
//PlaybackRemoteImp
PlaybackRemoteImp::PlaybackRemoteImp()
{
    INFO("+\n");
}

PlaybackRemoteImp::~PlaybackRemoteImp()
{
    INFO("mRemoteId %s begin\n", mRemoteId.c_str());
    if (mClientNode) {
        removeListener(NULL);
    }
    mClientNode.reset();
    INFO("mRemoteId %s end\n", mRemoteId.c_str());
}

bool PlaybackRemoteImp::connect(const char*remoteId)
{
    INFO("+\n");
    try {
        mRemoteId = remoteId;
        DEBUG("PlaybackRemote UniqueId: %s", remoteId);
        mClientNode = ClientNode<PlaybackRemoteProxy>::create("mcp", remoteId, this);
        if (!mClientNode->init()) {
            ERROR("failed to init PlaybackRemoteProxy\n");
            mClientNode.reset();
            return false;
        }
        INFO("create PlaybackRemoteProxy success\n");
        mMessager = new UBusMessage<PlaybackRemoteImp>(this);
        return true;
    } catch (...) {
        ERROR("no mem\n");
        return false;
    }
}

bool PlaybackRemoteImp::handleSignal(const SharedPtr<DMessage> &msg)
{
    if (mListener.empty()) {
        WARNING("listener is null, just return");
        return true;
    }

    int type = msg->readInt32();
    INFO("callback msg %s", msgName(type).c_str());
    // listener is thread safe
    // MMAutoLock lock(mLock);
    if (type == PlaybackRemote::Listener::MsgType::kMsgChannelEvent) {
        std::string event = msg->readString().c_str();
        MMParamSP param(new MMParam());
        param->writeCString(event.c_str());
        for (auto it = mListener.begin(); it != mListener.end(); it++) {
            (*it)->onMessage(PlaybackRemote::Listener::MsgType::kMsgChannelEvent, 0, 0, param.get());
        }
    } else if (type == PlaybackRemote::Listener::MsgType::kMsgChannelDestroyed) {
        for (auto it = mListener.begin(); it != mListener.end(); it++) {
            (*it)->onMessage(PlaybackRemote::Listener::MsgType::kMsgChannelDestroyed, 0, 0, NULL);
        }
    } else if (type == PlaybackRemote::Listener::MsgType::kMsgPlaybackStateChanged) {
        PlaybackStateSP state = PlaybackState::create();
        state->readFromMsg(msg);
        for (auto it = mListener.begin(); it != mListener.end(); it++) {
            MMParamSP param(new MMParam());
            state->writeToMMParam(param.get());
            (*it)->onMessage(PlaybackRemote::Listener::MsgType::kMsgPlaybackStateChanged, 0, 0, param.get());
        }
    } else if (type == PlaybackRemote::Listener::MsgType::kMsgMetadataChanged) {
        MediaMetaSP meta = MediaMeta::create();
        meta->readFromMsg(msg);
        for (auto it = mListener.begin(); it != mListener.end(); it++) {
            MMParamSP param(new MMParam());
            meta->writeToMMParam(param.get());
            (*it)->onMessage(PlaybackRemote::Listener::MsgType::kMsgMetadataChanged, 0, 0, param.get());
        }
    } else if (type == PlaybackRemote::Listener::MsgType::kMsgPlaylistChanged) {
        int32_t playlist = msg->readInt32();
        for (auto it = mListener.begin(); it != mListener.end(); it++) {
            (*it)->onMessage(PlaybackRemote::Listener::MsgType::kMsgPlaylistChanged, playlist, 0, NULL);
        }
    } else if (type == PlaybackRemote::Listener::MsgType::kMsgPlaylistTitleChanged) {
        std::string title = msg->readString().c_str();
        DEBUG("onQueueTitleChanged\n", title.c_str());
        for (auto it = mListener.begin(); it != mListener.end(); it++) {
            MMParamSP param(new MMParam());
            param->writeCString(title.c_str());
            (*it)->onMessage(PlaybackRemote::Listener::MsgType::kMsgPlaylistTitleChanged, 0, 0, param.get());
        }
    } else {
        return false;
    }

    return true;
}

mm_status_t PlaybackRemoteImp::handleMsg(MMParamSP param)
{
    int msg = param->readInt32();
    DEBUG("name %s", methodName(msg).c_str());

    mm_status_t status = MM_ERROR_SUCCESS;
    switch (msg) {
        case MSG_GET_MEDIAMETA:
        {
            MediaMetaSP meta = mClientNode->node()->getMetadata();
            meta->writeToMMParam(param.get());
            break;
        }
        case MSG_GET_PALYBACKSTATE:
        {
            PlaybackStateSP state = mClientNode->node()->getPlaybackState();
            state->writeToMMParam(param.get());
            break;
        }
        case MSG_GET_PLAYLIST:
        {
            int32_t playlist = mClientNode->node()->getPlaylist();
            param->writeInt32(playlist);
            break;
        }
        case MSG_GET_PACKAGE_NAME:
        {
            std::string packageName = mClientNode->node()->getPackageName();
            param->writeCString(packageName.c_str());
            break;
        }
        case MSG_GET_PLAYLIST_TITLE:
        {
            std::string title = mClientNode->node()->getPlaylistTitle();
            param->writeCString(title.c_str());
            break;
        }
        case MSG_GET_USAGES:
        {
            int32_t usges = mClientNode->node()->getUsages();
            param->writeInt32(usges);
            break;
        }
        case MSG_GET_TAG:
        {
            std::string tag = mClientNode->node()->getTag();
            param->writeCString(tag.c_str());
            break;
        }
        case MSG_DISPATCH_PLAYBACK_EVENT:
        {
            PlaybackEventSP event = PlaybackEvent::create();
            event->readFromMMParam(param.get());
            status = mClientNode->node()->sendPlaybackEvent(event);
            break;
        }
        case MSG_PLAY:
        {
            status = mClientNode->node()->play();
            break;
        }
        case MSG_PAUSE:
        {
            status = mClientNode->node()->pause();
            break;
        }
        case MSG_SEEKTO:
        {
            int64_t pos = param->readInt64();
            status = mClientNode->node()->seekTo(pos);
            break;
        }
        case MSG_STOP:
        {
            status = mClientNode->node()->stop();
            break;
        }

        case MSG_NEXT:
        {
            status = mClientNode->node()->next();
            break;
        }

        case MSG_PREVIOUS:
        {
            status = mClientNode->node()->previous();
            break;
        }
        case MSG_FASTFORWARD:
        {
            status = mClientNode->node()->fastForward();
            break;
        }
        case MSG_REWIND:
        {
            status = mClientNode->node()->rewind();
            break;
        }
        case MSG_ADD_LISTENER:
        {
            Listener *listener = (Listener*)param->readRawPointer();

            INFO("mListener.size %d, listener %p\n", mListener.size(), listener);
            auto it = std::find(mListener.begin(), mListener.end(), listener);
            if (it != mListener.end()) {
                DEBUG("listener %p is already exists", listener);
                return MM_ERROR_SUCCESS;
            }

            mListener.push_back(listener);

            if (mListener.size() > 1) {
                return MM_ERROR_SUCCESS;
            }

            status = mMessager->addRule(mMessager);
            if (status != MM_ERROR_SUCCESS) {
                return status;
            }

            if (mClientNode->node()->registerCallbackListener(mMessager->callbackName().c_str()) !=
                MM_ERROR_SUCCESS) {
                DEBUG("registerCallbackListener failed");
                return MM_ERROR_UNKNOWN;
            }

            return MM_ERROR_SUCCESS;
        }
        case MSG_REMOVE_LISTENER:
        {
            Listener *listener = (Listener*)param->readRawPointer();
            INFO("mListener.size %d, listener %p\n", mListener.size(), listener);
            if (mListener.empty())
                return MM_ERROR_SUCCESS;
            if (listener) {
                auto it = std::find(mListener.begin(), mListener.end(), listener);
                if (it == mListener.end()) {
                    ERROR("listener %p is not exists", listener);
                    return MM_ERROR_INVALID_PARAM;
                }

                mListener.erase(it);
            } else {
                // if listener is null, means to remove all the listeners
                mListener.clear();
            }
            if (mListener.empty()) {
                mMessager->removeRule();
            }
            return MM_ERROR_SUCCESS;
        }
        default:
        {
            ASSERT(0 && "Should not be here\n");
            break;
        }
    }

    return status;
}

mm_status_t PlaybackRemoteImp::addListener(Listener * listener)
{
    if (!listener) {
        ERROR("null listener");
        return MM_ERROR_INVALID_PARAM;
    }

    MMParamSP param(new MMParam);
    param->writeInt32(MSG_ADD_LISTENER);
    param->writeRawPointer((uint8_t*)listener);
    return mMessager->sendMsg(param);
}

void PlaybackRemoteImp::removeListener(Listener * listener)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_REMOVE_LISTENER);
    param->writeRawPointer((uint8_t*)listener);
    mMessager->sendMsg(param);
}

void PlaybackRemoteImp::dump()
{
    DEBUG("##########PlaybackRemoteImp dump info:\n");
    DEBUG("mListener.size: %d", mListener.size());
    for (auto it = mListener.begin(); it != mListener.end(); it++) {
        DEBUG("listener %p", *it);
    }

    DEBUG("mRemoteId: %s", mRemoteId.c_str());
    DEBUG("mCallbackName: %s", mMessager->callbackName().c_str());
}

MediaMetaSP PlaybackRemoteImp::getMetadata()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_MEDIAMETA);
    mm_status_t status = mMessager->sendMsg(param);

    MediaMetaSP meta;
    if (status == MM_ERROR_SUCCESS) {
        meta = MediaMeta::create();
        meta->readFromMMParam(param.get());
    }
    return meta;
}

PlaybackStateSP PlaybackRemoteImp::getPlaybackState()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_PALYBACKSTATE);
    mm_status_t status = mMessager->sendMsg(param);

    PlaybackStateSP state;
    if (status == MM_ERROR_SUCCESS) {
        state = PlaybackState::create();
        state->readFromMMParam(param.get());
    }
    return state;
}

int PlaybackRemoteImp::getPlaylist()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_PLAYLIST);
    mMessager->sendMsg(param);
    return param->readInt32();
}

std::string PlaybackRemoteImp::getPlaylistTitle()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_PLAYLIST_TITLE);
    mMessager->sendMsg(param);
    return param->readCString();
}

std::string PlaybackRemoteImp::getPackageName()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_PACKAGE_NAME);
    mMessager->sendMsg(param);
    return param->readCString();
}

int32_t PlaybackRemoteImp::getUsages()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_USAGES);
    mMessager->sendMsg(param);
    return param->readInt32();
}

std::string PlaybackRemoteImp::getTag()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_TAG);
    mMessager->sendMsg(param);
    return param->readCString();
}

mm_status_t PlaybackRemoteImp::sendPlaybackEvent(PlaybackEventSP event)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_DISPATCH_PLAYBACK_EVENT);
    event->writeToMMParam(param.get());
    return mMessager->sendMsg(param);
}

mm_status_t PlaybackRemoteImp::play()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_PLAY);
    return mMessager->sendMsg(param);
}
mm_status_t PlaybackRemoteImp::pause()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_PAUSE);
    return mMessager->sendMsg(param);
}
mm_status_t PlaybackRemoteImp::seekTo(int64_t pos/*ms*/)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SEEKTO);
    param->writeInt64(pos);
    return mMessager->sendMsg(param);
}
mm_status_t PlaybackRemoteImp::stop()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_STOP);
    return mMessager->sendMsg(param);
}
mm_status_t PlaybackRemoteImp::next()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_NEXT);
    return mMessager->sendMsg(param);
}
mm_status_t PlaybackRemoteImp::previous()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_PREVIOUS);
    return mMessager->sendMsg(param);
}
mm_status_t PlaybackRemoteImp::fastForward()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_FASTFORWARD);
    return mMessager->sendMsg(param);
}
mm_status_t PlaybackRemoteImp::rewind()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_REWIND);
    return mMessager->sendMsg(param);
}

#define MSG_NAME(MSG) case (MSG): return #MSG;
std::string PlaybackRemoteImp::methodName(int method)
{
    switch (method) {
        MSG_NAME(MSG_GET_MEDIAMETA);
        MSG_NAME(MSG_GET_PALYBACKSTATE);
        MSG_NAME(MSG_GET_PLAYLIST);
        MSG_NAME(MSG_GET_PLAYLIST_TITLE);
        MSG_NAME(MSG_GET_USAGES);
        MSG_NAME(MSG_GET_TAG);
        MSG_NAME(MSG_DISPATCH_PLAYBACK_EVENT);
        MSG_NAME(MSG_ADD_LISTENER);
        MSG_NAME(MSG_REMOVE_LISTENER);
        MSG_NAME(MSG_PLAY);
        MSG_NAME(MSG_PAUSE);
        MSG_NAME(MSG_SEEKTO);
        MSG_NAME(MSG_STOP);
        MSG_NAME(MSG_NEXT);
        MSG_NAME(MSG_PREVIOUS);
        MSG_NAME(MSG_FASTFORWARD);
        MSG_NAME(MSG_REWIND);
        default: return "unknown msg";
    }
    return "unknown msg";
}

std::string PlaybackRemoteImp::msgName(int msg)
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


///////////////////////////////////////////////////////////////////////
//PlaybackRemoteProxy
PlaybackRemoteProxy::PlaybackRemoteProxy(const yunos::SharedPtr<yunos::DService>& service,
                        yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg)
                    : yunos::DProxy(service, pathName, iface)
                    , mImp(static_cast<PlaybackRemoteImp*>(arg))
{
    INFO("[%s]", iface.c_str());
    ASSERT(mImp);
}

PlaybackRemoteProxy::~PlaybackRemoteProxy()
{
    INFO("[%s]", interface().c_str());
}

mm_status_t PlaybackRemoteProxy::registerCallbackListener(const char*controlCbId)
{
    CHECK_OBTAIN_MSG_RETURN1("registerCallbackListener", MM_ERROR_NO_MEM);
    msg->writeString(String(controlCbId));
    CHECK_REPLY_MSG_RETURN_INT();
}

mm_status_t PlaybackRemoteProxy::unregisterCallbackListener(const char*controlCbId)
{
    CHECK_OBTAIN_MSG_RETURN1("unregisterCallbackListener", MM_ERROR_NO_MEM);
    msg->writeString(String(controlCbId));
    CHECK_REPLY_MSG_RETURN_INT();

}

MediaMetaSP PlaybackRemoteProxy::getMetadata()
{
    SEND_UBUS_MESSAGE_PARAM0_RETURNOBJ("getMetadata", MediaMeta);
}

PlaybackStateSP PlaybackRemoteProxy::getPlaybackState()
{
    SEND_UBUS_MESSAGE_PARAM0_RETURNOBJ("getPlaybackState", PlaybackState);
}

int PlaybackRemoteProxy::getPlaylist()
{
    CHECK_OBTAIN_MSG_RETURN1("getPlaylist", -1);
    HANDLE_INVALID_REPLY_RETURN1(-1);
    return reply->readInt32();
}

std::string PlaybackRemoteProxy::getPlaylistTitle()
{
    SEND_UBUS_MESSAGE_PARAM0_RETURNSTR("getPlaylistTitle");
}

std::string PlaybackRemoteProxy::getPackageName()
{
    SEND_UBUS_MESSAGE_PARAM0_RETURNSTR("getPackageName");
}

int32_t PlaybackRemoteProxy::getUsages()
{
    CHECK_OBTAIN_MSG_RETURN1("getUsages", -1);
    HANDLE_INVALID_REPLY_RETURN1(-1);
    return reply->readInt32();
}

std::string PlaybackRemoteProxy::getTag()
{
    SEND_UBUS_MESSAGE_PARAM0_RETURNSTR("getTag");
}

mm_status_t PlaybackRemoteProxy::sendPlaybackEvent(PlaybackEventSP event)
{
    yunos::SharedPtr <yunos::DMessage> msg = obtainMethodCallMessage(yunos::String("sendPlaybackEvent"));
    if (msg == NULL) {
        ERROR("msg is null\n");
        return MM_ERROR_NO_MEM;
    }

    event->writeToMsg(msg);

    yunos::SharedPtr <yunos::DMessage> reply = sendMessageWithReplyBlocking(msg);
    if (!reply) {
        ERROR("invalid reply\n");
        return MM_ERROR_OP_FAILED;
    }

    int result = reply->readInt32();
    DEBUG("ret: %d\n", result);
    return (mm_status_t) result;
}

//
mm_status_t PlaybackRemoteProxy::play()
{
    CHECK_OBTAIN_MSG_RETURN1("play", MM_ERROR_NO_MEM);
    CHECK_REPLY_MSG_RETURN_INT();
}
mm_status_t PlaybackRemoteProxy::pause()
{
    CHECK_OBTAIN_MSG_RETURN1("pause", MM_ERROR_NO_MEM);
    CHECK_REPLY_MSG_RETURN_INT();
}
mm_status_t PlaybackRemoteProxy::stop()
{
    CHECK_OBTAIN_MSG_RETURN1("stop", MM_ERROR_NO_MEM);
    CHECK_REPLY_MSG_RETURN_INT();
}
mm_status_t PlaybackRemoteProxy::next()
{
    CHECK_OBTAIN_MSG_RETURN1("next", MM_ERROR_NO_MEM);
    CHECK_REPLY_MSG_RETURN_INT();
}
mm_status_t PlaybackRemoteProxy::previous()
{
    CHECK_OBTAIN_MSG_RETURN1("previous", MM_ERROR_NO_MEM);
    CHECK_REPLY_MSG_RETURN_INT();
}
mm_status_t PlaybackRemoteProxy::fastForward()
{
    CHECK_OBTAIN_MSG_RETURN1("fastForward", MM_ERROR_NO_MEM);
    CHECK_REPLY_MSG_RETURN_INT();
}
mm_status_t PlaybackRemoteProxy::rewind()
{
    CHECK_OBTAIN_MSG_RETURN1("rewind", MM_ERROR_NO_MEM);
    CHECK_REPLY_MSG_RETURN_INT();
}

mm_status_t PlaybackRemoteProxy::seekTo(int64_t pos/*ms*/)
{
    CHECK_OBTAIN_MSG_RETURN1("seekTo", MM_ERROR_NO_MEM);
    msg->writeInt64(pos);
    CHECK_REPLY_MSG_RETURN_INT();
}

// Called in msg thread
#if 0
void PlaybackRemoteProxy::onDeath(const DLifecycleListener::DeathInfo& deathInfo)
{
    INFO("media_service die: %s, mSeq %d",
        deathInfo.mName.c_str(), deathInfo.mSeq);
    if (!mImp || mImp->mListener.empty()) {
        WARNING("media contorller is already destroyed");
        return;
    }

    DProxy::onDeath(deathInfo);
    for (auto it = mImp->mListener.begin(); it != mImp->mListener.end(); it++) {
        (*it)->onMessage(PlaybackRemote::Listener::MsgType::kMsgError, MM_ERROR_SERVER_DIED, 0, NULL);
    }

}
#endif
}
