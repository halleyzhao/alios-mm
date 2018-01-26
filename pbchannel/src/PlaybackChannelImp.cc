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
#include <DPMSProxy.h>
#include <dbus/DSignalRule.h>

#include <multimedia/PlaybackChannel.h>
#include <multimedia/mm_debug.h>
#include <multimedia/mm_cpp_utils.h>
#include "multimedia/PlaybackEvent.h"
#include "PlaybackChannelManagerImp.h"
#include "PlaybackChannelImp.h"
#include "multimedia/UBusHelper.h"
#include "multimedia/UBusNode.h"
#include "multimedia/UBusMessage.h"

namespace YUNOS_MM {
using namespace yunos;

DEFINE_LOGTAG1(PlaybackChannelImp, [PC])
DEFINE_LOGTAG1(PlaybackChannelProxy, [PC])

enum {
    MSG_SET_USAGE = 0,
    MSG_SET_ENABLED,
    MSG_SET_PLAYBACKSTATE,
    MSG_SET_MEDIAMETA,
    MSG_SET_PLAYLIST,
    MSG_SET_PLAYLIST_TITLE,
    MSG_GET_REMOTE_ID,
    MSG_SET_LISTENER
};
static int mTimeOut = -1;

////////////////////////////////////////////////////////////////////////////
//PlaybackChannelImp
PlaybackChannelImp::PlaybackChannelImp(const char*tag, const char *packageName)
                                        : mListener(NULL)
                                        , mEnabled(false)
                                        , mTag(tag)
                                        , mPackageName(packageName ? packageName : "null")
                                        , mUserId(0)
                                        , mUsages(0)
{
    INFO("+\n");
    mMediaMeta = MediaMeta::create();
    mPlaybackState = PlaybackState::create();
}

PlaybackChannelImp::~PlaybackChannelImp()
{
    INFO("+\n");
    // mm_print_backtrace();
    if (mMessager && mClientNode) {
        setListener(NULL);
    }
    mClientNode.reset();
    INFO("-\n");
}

bool PlaybackChannelImp::connect()
{
    INFO("+\n");
    try {
        mManager = PlaybackChannelManager::getInstance();
        if (!mManager) {
            ERROR("PlaybackChannelManager::getInstance failed");
            return false;
        }

        // For access control, PlaybackChannelImpl should be friend class of PlaybackChannelManagerImp,
        PlaybackChannelManagerImp *manager = static_cast<PlaybackChannelManagerImp*>(mManager.get());
        mChannelId = manager->createChannel(mTag.c_str(), mPackageName.c_str(), mUserId);
        if (mChannelId == INVALID_CHANNEL_ID) {
            ERROR("channel id is invalid");
            return false;
        }

        DEBUG("PlaybackChannelAdaptor id %s", mChannelId.c_str());
        mClientNode = ClientNode<PlaybackChannelProxy>::create("msp", mChannelId.c_str(), this);
        if (!mClientNode->init()) {
            ERROR("failed to init PlaybackChannelProxy\n");
            mClientNode.reset();
            return false;
        }
        INFO("create PlaybackChannelProxy success\n");
        mMessager = new UBusMessage<PlaybackChannelImp>(this);
        return true;
    } catch (...) {
        ERROR("no mem\n");
        return false;
    }
}

void PlaybackChannelImp::destroy()
{
    if (!mManager)
        return;
    DEBUG("+");
    PlaybackChannelManagerImp *manager = static_cast<PlaybackChannelManagerImp*>(mManager.get());
    manager->destroyChannel(mChannelId.c_str());
    DEBUG("-");

}

mm_status_t PlaybackChannelImp::setListener(Listener * listener)
{
    INFO("listener: %p, mListener %p\n", listener, mListener);
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SET_LISTENER);
    param->writeRawPointer((uint8_t*)listener);
    return mMessager->sendMsg(param);
}

bool PlaybackChannelImp::handleSignal(const yunos::SharedPtr<yunos::DMessage>& msg)
{
    if (!mListener) {
        WARNING("listener is null, just return");
        return true;
    }

    int type = msg->readInt32();
    INFO("callback msg %s", msgName(type).c_str());
    // listener is thread safe
    // MMAutoLock lock(mLock);
    if (type == PlaybackChannel::Listener::MsgType::kMsgPlay ||
        type == PlaybackChannel::Listener::MsgType::kMsgPause ||
        type == PlaybackChannel::Listener::MsgType::kMsgStop ||
        type == PlaybackChannel::Listener::MsgType::kMsgNext ||
        type == PlaybackChannel::Listener::MsgType::kMsgPrevious ||
        type == PlaybackChannel::Listener::MsgType::kMsgFastForward ||
        type == PlaybackChannel::Listener::MsgType::kMsgRewind ||
        type == PlaybackChannel::Listener::MsgType::kMsgPlay) {
        mListener->onMessage(type, 0, 0, NULL);
    } else if (type == PlaybackChannel::Listener::MsgType::kMsgSeekTo) {
        int64_t pos = msg->readInt64();
        MMParamSP param(new MMParam());
        param->writeInt64(pos);
        mListener->onMessage(type, 0, 0, param.get());
    } else if (type == PlaybackChannel::Listener::MsgType::kMsgMediaButton) {
        PlaybackEventSP event = PlaybackEvent::create();
        event->readFromMsg(msg);
        if (mUsages & PlaybackChannelInterface::Usages::kNeedMediaButtonKeys) {
            MMParamSP param(new MMParam());
            event->writeToMMParam(param.get());
            mListener->onMessage(type, 0, 0, param.get());
        } else {
            onPlaybackEvent(event);
        }
    }

    return true;
}

mm_status_t PlaybackChannelImp::handleMsg(MMParamSP param)
{
    int msg = param->readInt32();
    DEBUG("name %s", methodName(msg).c_str());

    mm_status_t status = MM_ERROR_SUCCESS;
    switch (msg) {
        case MSG_SET_USAGE:
        {
            int32_t usage = param->readInt32();
            mUsages = usage;
            status = mClientNode->node()->setUsages(usage);
            break;
        }
        case MSG_SET_ENABLED:
        {
            int32_t enable = param->readInt32();
            mEnabled = enable != 0;
            status = mClientNode->node()->setEnabled(enable != 0);
            break;
        }
        case MSG_SET_PLAYBACKSTATE:
        {
            PlaybackStateSP state = PlaybackState::create();
            state->readFromMMParam(param.get());
            mPlaybackState = state;
            status = mClientNode->node()->setPlaybackState(state);
            break;
        }
        case MSG_SET_MEDIAMETA:
        {
            MediaMetaSP meta = MediaMeta::create();
            meta->readFromMMParam(param.get());
            mMediaMeta = meta;
            status = mClientNode->node()->setMetadata(meta);
            break;
        }
        case MSG_SET_PLAYLIST:
        {
            int32_t playlist = param->readInt32();
            status = mClientNode->node()->setPlaylist(playlist);
            break;
        }
        case MSG_SET_PLAYLIST_TITLE:
        {
            const char* title = param->readCString();
            status = mClientNode->node()->setPlaylistTitle(title);
            break;
        }
        case MSG_GET_REMOTE_ID:
        {
            std::string id = mClientNode->node()->getPlaybackRemoteId();
            param->writeCString(id.c_str());
            break;
        }
        case MSG_SET_LISTENER:
        {
            Listener *listener = (Listener*)param->readRawPointer();
            DEBUG("listener %p", listener);
            if (!listener) {
                if (!mListener) {
                    return MM_ERROR_SUCCESS;
                }
                mListener = NULL;
                DEBUG("remove signal rule");
                mMessager->removeRule();
                DEBUG("remove end");
                return MM_ERROR_SUCCESS;
            } else {
                Listener *pre = mListener;
                mListener = listener;
                if (pre) {
                    // callback has been setup, just return
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

std::string PlaybackChannelImp::getPlaybackRemoteId()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_REMOTE_ID);
    mMessager->sendMsg(param);
    return param->readCString();
}

mm_status_t PlaybackChannelImp::setUsages(int32_t usage)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SET_USAGE);
    param->writeInt32(usage);
    return mMessager->sendMsg(param);
}


mm_status_t PlaybackChannelImp::setEnabled(bool enabled)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SET_ENABLED);
    param->writeInt32((int32_t)enabled);
    return mMessager->sendMsg(param);
}

bool PlaybackChannelImp::isEnabled()
{
    return mEnabled;
}

mm_status_t PlaybackChannelImp::setPlaybackState(PlaybackStateSP state)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SET_PLAYBACKSTATE);
    state->writeToMMParam(param.get());
    return mMessager->sendMsg(param);
}

mm_status_t PlaybackChannelImp::setPlaylist(int32_t playlist)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SET_PLAYLIST);
    param->writeInt32(playlist);
    return mMessager->sendMsg(param);
}

mm_status_t PlaybackChannelImp::setPlaylistTitle(const char*title)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SET_PLAYLIST_TITLE);
    param->writeCString(title);
    return mMessager->sendMsg(param);
}

mm_status_t PlaybackChannelImp::setMetadata(MediaMetaSP meta)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_SET_MEDIAMETA);
    meta->writeToMMParam(param.get());
    return mMessager->sendMsg(param);
}

void PlaybackChannelImp::dump()
{
    DEBUG("##########PlaybackChannelImp dump info:\n");
    DEBUG("mTag: %s", mTag.c_str());
    DEBUG("mPackageName: %s", mPackageName.c_str());
    DEBUG("mUserId: %d", mUserId);
    DEBUG("mUsages: %d", mUsages);
    DEBUG("mChannelId: %s", mChannelId.c_str());
    DEBUG("mCallbackName: %s", mMessager->callbackName().c_str());
    DEBUG("mListener: %p", mListener);
    mMediaMeta->dump();
    mPlaybackState->dump();
}

#define MSG_NAME(MSG) case (MSG): return #MSG;

std::string PlaybackChannelImp::methodName(int msg)
{
    switch (msg) {
        MSG_NAME(MSG_SET_USAGE);
        MSG_NAME(MSG_SET_ENABLED);
        MSG_NAME(MSG_SET_PLAYBACKSTATE);
        MSG_NAME(MSG_SET_MEDIAMETA);
        MSG_NAME(MSG_SET_PLAYLIST);
        MSG_NAME(MSG_SET_PLAYLIST_TITLE);
        MSG_NAME(MSG_GET_REMOTE_ID);
        MSG_NAME(MSG_SET_LISTENER);
        default: return "unknown msg";
    }
    return "unknown msg";
}

std::string PlaybackChannelImp::msgName(int msg)
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


mm_status_t PlaybackChannelImp::onPlaybackEvent(PlaybackEventSP pbEvent)
{
    int32_t capabilities = mPlaybackState == NULL ? 0 : mPlaybackState->getCapabilities();
    DEBUG("capabilities 0x%0x, type %d, key %d, isFromHost %d",
        capabilities, pbEvent->getType(), pbEvent->getKey(), pbEvent->isFromHost());
    if (pbEvent->getType() == PlaybackEvent::Types::kKeyDown) {
        mPlaybackState->dump();
        //todo: whether need to check the playback state??
        if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaPlay) {
            if ((capabilities & PlaybackState::Capabilities::kCapPlay) != 0) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgPlay, 0, 0, NULL);
            }
        } else if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaPause) {
            if ((capabilities & PlaybackState::Capabilities::kCapPause) != 0) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgPause, 0, 0, NULL);
            }
        } else if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaNext) {
            if ((capabilities & PlaybackState::Capabilities::kCapNext) != 0) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgNext, 0, 0, NULL);
            }
        } else if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaPrevious) {
            if ((capabilities & PlaybackState::Capabilities::kCapPrevious) != 0) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgPrevious, 0, 0, NULL);
            }
        } else if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaStop) {
            if ((capabilities & PlaybackState::Capabilities::kCapStop) != 0) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgStop, 0, 0, NULL);
            }
        } else if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaFastForward) {
            if ((capabilities & PlaybackState::Capabilities::kCapFastForward) != 0) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgFastForward, 0, 0, NULL);
            }
        } else if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaRewind) {
            if ((capabilities & PlaybackState::Capabilities::kCapRewind) != 0) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgRewind, 0, 0, NULL);
            }

        } else if (pbEvent->getKey() == PlaybackEvent::Keys::kMediaPlayPause ||
                pbEvent->getKey() == PlaybackEvent::Keys::kHeadsetHook) {
            // "MediaPlayPause" is deprecated
            bool isPlaying = mPlaybackState == NULL ? false : mPlaybackState->getState() == PlaybackState::State::kPlaying;
            if (isPlaying) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgPause, 0, 0, NULL);
            } else if (!isPlaying) {
                mListener->onMessage(PlaybackChannel::Listener::MsgType::kMsgPlay, 0, 0, NULL);
            }
        }
    }

    return MM_ERROR_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////
//PlaybackChannelProxy
PlaybackChannelProxy::PlaybackChannelProxy(const yunos::SharedPtr<yunos::DService>& service,
                    yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg)
                    : yunos::DProxy(service, pathName, iface)
                    , mImp(static_cast<PlaybackChannelImp*>(arg))
{
    INFO("+\n");
    ASSERT(mImp);
}

PlaybackChannelProxy::~PlaybackChannelProxy()
{
    INFO("+\n");
}

mm_status_t PlaybackChannelProxy::registerCallbackListener(const char*channelCbId)
{
    CHECK_OBTAIN_MSG_RETURN1("registerCallbackListener", MM_ERROR_NO_MEM);
    msg->writeString(String(channelCbId));
    CHECK_REPLY_MSG_RETURN_INT();
}

mm_status_t PlaybackChannelProxy::unregisterCallbackListener(const char*channelCbId)
{
    CHECK_OBTAIN_MSG_RETURN1("unregisterCallbackListener", MM_ERROR_NO_MEM);
    msg->writeString(String(channelCbId));
    CHECK_REPLY_MSG_RETURN_INT();
}

std::string PlaybackChannelProxy::getPlaybackRemoteId()
{
    SEND_UBUS_MESSAGE_PARAM0_RETURNSTR("getPlaybackRemoteId");
}

mm_status_t PlaybackChannelProxy::setUsages(int32_t usage)
{
    CHECK_OBTAIN_MSG_RETURN1("setUsages", MM_ERROR_NO_MEM);
    msg->writeInt32(usage);
    CHECK_REPLY_MSG_RETURN_INT();
}

mm_status_t PlaybackChannelProxy::setEnabled(bool enabled)
{
    CHECK_OBTAIN_MSG_RETURN1("setEnabled", MM_ERROR_NO_MEM);
    msg->writeBool(enabled);
    CHECK_REPLY_MSG_RETURN_INT();

}

mm_status_t PlaybackChannelProxy::setPlaybackState(PlaybackStateSP state)
{
    CHECK_OBTAIN_MSG_RETURN1("setPlaybackState", MM_ERROR_NO_MEM);
    state->writeToMsg(msg);
    CHECK_REPLY_MSG_RETURN_INT();
}

mm_status_t PlaybackChannelProxy::setPlaylist(int32_t playlist)
{
    CHECK_OBTAIN_MSG_RETURN1("setPlaylist", MM_ERROR_NO_MEM);
    msg->writeInt32(playlist);
    CHECK_REPLY_MSG_RETURN_INT();
}

mm_status_t PlaybackChannelProxy::setPlaylistTitle(const char*title)
{
    CHECK_OBTAIN_MSG_RETURN1("setPlaylistTitle", MM_ERROR_NO_MEM);
    msg->writeString(String(title));
    CHECK_REPLY_MSG_RETURN_INT();
}

mm_status_t PlaybackChannelProxy::setMetadata(MediaMetaSP meta)
{
    CHECK_OBTAIN_MSG_RETURN1("setMetadata", MM_ERROR_NO_MEM);
    meta->writeToMsg(msg);
    CHECK_REPLY_MSG_RETURN_INT();
}

// Called in msg thread
void PlaybackChannelProxy::onDeath(const DLifecycleListener::DeathInfo& deathInfo)
{
    INFO("media_service die: %s, mSeq %d",
        deathInfo.mName.c_str(), deathInfo.mSeq);

    if (!mImp || !mImp->mListener) {
        WARNING("media channel is already destroyed");
        return;
    }

    DProxy::onDeath(deathInfo);
    mImp->mListener->onMessage(PlaybackChannel::Listener::kMsgError, MM_ERROR_SERVER_DIED, 0, NULL);

}


}
