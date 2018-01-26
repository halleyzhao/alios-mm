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
#include <vector>
#include <string>
#include <unistd.h>
#include <multimedia/PlaybackEvent.h>
#include <multimedia/PlaybackChannel.h>
#include <multimedia/PlaybackRemote.h>
#include <multimedia/PlaybackChannelManager.h>
#include "PlaybackChannelTestHelper.h"
#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {
DEFINE_LOGTAG1(PlaybackChannelListener, [PC])
DEFINE_LOGTAG1(PlaybackRemoteListener, [PC])
DEFINE_LOGTAG1(PlaybackChannelManagerListener, [PC])
DEFINE_LOGTAG1(PlaybackChannelTestHelper, [PC])

static std::vector<PlaybackRemoteSP> mRemotes;

/////////////////////////////////////////////////////////////////////
//PlaybackChannelListener
PlaybackChannelListener::PlaybackChannelListener(PlaybackChannel *channel) : mChannel(channel)
{
    mState = PlaybackState::create();
    mState->setCapabilities(YUNOS_MM::PlaybackState::Capabilities(PlaybackState::Capabilities::kCapPause |
        PlaybackState::Capabilities::kCapPlay |
        PlaybackState::Capabilities::kCapNext |
        PlaybackState::Capabilities::kCapPrevious |
        PlaybackState::Capabilities::kCapFastForward |
        PlaybackState::Capabilities::kCapRewind |
        PlaybackState::Capabilities::kCapStop |
        PlaybackState::Capabilities::kCapSeekTo));
}
std::string PlaybackChannelListener::getMessageString(int msg) {
    switch(msg) {
        case PlaybackChannel::Listener::MsgType::kMsgPlay: return "kMsgPlay";
        case PlaybackChannel::Listener::MsgType::kMsgPause: return "kMsgPause";
        case PlaybackChannel::Listener::MsgType::kMsgStop: return "kMsgStop";
        case PlaybackChannel::Listener::MsgType::kMsgNext: return "kMsgNext";
        case PlaybackChannel::Listener::MsgType::kMsgPrevious: return "kMsgPrevious";
        case PlaybackChannel::Listener::MsgType::kMsgFastForward: return "kMsgFastForward";
        case PlaybackChannel::Listener::MsgType::kMsgRewind: return "kMsgRewind";
        case PlaybackChannel::Listener::MsgType::kMsgSeekTo: return "kMsgSeekTo";
        case PlaybackChannel::Listener::MsgType::kMsgMediaButton: return "kMsgMediaButton";
        case PlaybackChannel::Listener::MsgType::kMsgError: return "kMsgError";
        default: return "unknownMsg";
    }
}

void PlaybackChannelListener::onMessage(int msg, int param1, int param2, const MMParam *obj)
{
    fprintf(stderr, "[PlaybackChannelListener::onMessage] msg %s, param1 %d, param2 %d\n",
        getMessageString(msg).c_str(), param1, param2);

    if (!mChannel) {
        ERROR("channel is released");
        return;
    }

    switch(msg) {
        case kMsgPlay:
        {
            mState->setState(PlaybackState::State::kPlaying, 0, 1.0);
            mChannel->setPlaybackState(mState);
            DEBUG("kMsgPlay");
            break;
        }
        case kMsgPause:
        {
            mState->setState(PlaybackState::State::kPaused, 0, 1.0);
            mChannel->setPlaybackState(mState);
            DEBUG("kMsgPause");
            break;
        }
        case kMsgStop:
        {
            mState->setState(PlaybackState::State::kStopped, 0, 1.0);
            mChannel->setPlaybackState(mState);
            DEBUG("kMsgStop");
            break;
        }
        case kMsgNext:
        {
            mState->setState(PlaybackState::State::kNext, 0, 1.0);
            mChannel->setPlaybackState(mState);
            DEBUG("kMsgNext");
            break;
        }
        case kMsgPrevious:
        {
            mState->setState(PlaybackState::State::kPrevious, 0, 1.0);
            mChannel->setPlaybackState(mState);
            DEBUG("kMsgPrevious");
            break;
        }
        case kMsgFastForward:
        {
            mState->setState(PlaybackState::State::kFastForwarding, 0, 1.0);
            mChannel->setPlaybackState(mState);
            DEBUG("kMsgFastForward");
            break;
        }
        case kMsgRewind:
        {
            mState->setState(PlaybackState::State::kRewinding, 0, 1.0);
            mChannel->setPlaybackState(mState);
            DEBUG("kMsgRewind");
            break;
        }
        case kMsgSeekTo:
        {
            int64_t pos = obj->readInt64();
            DEBUG("kMsgSeekTo, %" PRId64 "", pos);
            break;
        }
        case kMsgMediaButton:
        {
            int32_t type = obj->readInt32();
            int32_t keys = obj->readInt32();
            const char * code = obj->readCString();
            int32_t usages = obj->readInt32();
            int32_t repeatCount = obj->readInt32();

            DEBUG("kMsgMediaButton, type %d, keys %d, code %s, usages %d, repeatCount %d",
                type, keys, code, usages, repeatCount);
            break;
        }
        case kMsgError:
        {
            DEBUG("kMsgError, param1 %d", param1);
            break;
        }
        default:
            DEBUG("unknown message, %d", msg);
            break;
    }
}

PlaybackRemoteListener::PlaybackRemoteListener(PlaybackRemote *remote) : mRemote(remote)
{
}

std::string PlaybackRemoteListener::getMessageString(int msg) {
    switch(msg) {
        case PlaybackRemote::Listener::MsgType::kMsgChannelDestroyed: return "kMsgChannelDestroyed";
        case PlaybackRemote::Listener::MsgType::kMsgChannelEvent: return "kMsgChannelEvent";
        case PlaybackRemote::Listener::MsgType::kMsgPlaybackStateChanged: return "kMsgPlaybackStateChanged";
        case PlaybackRemote::Listener::MsgType::kMsgMetadataChanged: return "kMsgMetadataChanged";
        case PlaybackRemote::Listener::MsgType::kMsgPlaylistChanged: return "kMsgPlaylistChanged";
        case PlaybackRemote::Listener::MsgType::kMsgPlaylistTitleChanged: return "kMsgPlaylistTitleChanged";
        case PlaybackRemote::Listener::MsgType::kMsgError: return "kMsgError";
        default: return "unknownMsg";
    }
}

void PlaybackRemoteListener::onMessage(int msg, int param1, int param2, const MMParam *obj)
{
    fprintf(stderr, "[PlaybackRemoteListener::onMessage] msg %s, param1 %d, param2 %d\n",
        getMessageString(msg).c_str(), param1, param2);

    if (!mRemote) {
        ERROR("remote is released");
        return;
    }

    switch(msg) {
        case kMsgChannelDestroyed:
            DEBUG("kMsgChannelDestroyed");
            break;
        case kMsgChannelEvent: {
            const char*event = obj->readCString();
            DEBUG("kMsgChannelEvent: %s", event);
            break;
        }
        case kMsgPlaybackStateChanged: {
            DEBUG("kMsgPlaybackStateChanged\n");
            PlaybackStateSP playbackstate = PlaybackState::create();
            playbackstate->readFromMMParam(const_cast<MMParam *>(obj));
            playbackstate->dump();
            break;
        }
        case kMsgMetadataChanged: {
            DEBUG("kMsgMetadataChanged");
            MediaMetaSP meta = MediaMeta::create();
            meta->readFromMMParam(const_cast<MMParam *>(obj));
            meta->dump();
            break;
        }
        case kMsgPlaylistChanged:
            DEBUG("kMsgPlaylistChanged, playlist: %d", param1);
            break;
        case kMsgPlaylistTitleChanged: {
            const char*title = obj->readCString();
            DEBUG("kMsgPlaylistTitleChanged, title %s", title);
            break;
        }
        case kMsgError: {
            ERROR("kMsgError, param1 %d", param1);
            break;
        }
        default:
            DEBUG("unknown message, %d", msg);
            break;
    }
}

void PlaybackChannelManagerListener::onEnabledChannelsChanged(std::vector<PlaybackRemoteSP> &remotes)
{
    fprintf(stderr, "remotes.size %d\n", remotes.size());
    DEBUG("remotes.size %d", remotes.size());
    for (uint32_t i = 0; i < remotes.size(); i++) {
        remotes[i]->dump();
    }


}


////////////////////////////////////////////////////////////////////////////////////
//PlaybackChannelTestHelper
PlaybackChannelTestHelper::PlaybackChannelTestHelper()
{
    mChannels.resize(2);
    mChannelListeners.resize(2);
}

void PlaybackChannelTestHelper::channelCreate(int32_t num)
{
    if (num > 1)
        return;

    PlaybackChannelSP channel;
    ChannelListenerSP listener;

    channel = PlaybackChannel::create("MusicPlayer", "page://musicplayer.yunos.com/musicplayer");
    if (!channel) {
        ERROR("failed to create PlaybackChannel\n");
        return;
    }
    listener.reset(new PlaybackChannelListener(channel.get()));
    channel->setListener(listener.get());
    bool isEnabled = channel->isEnabled();
    if (!isEnabled) {
        channel->setEnabled(true);
    }

    //mChannels.push_back(channel);
    //mChannelListeners.push_back(listener);
    mChannels[num] = channel;
    mChannelListeners[num] = listener;
}

void PlaybackChannelTestHelper::channelConfigure( bool isNeedMediaButton, int32_t num)
{
    if (num > 1)
        return;

    PlaybackChannelSP channel = mChannels[num];
    if (!channel)
        return;

    uint32_t usages = 0;
    if (isNeedMediaButton) {
        usages |= PlaybackChannelInterface::Usages::kNeedMediaButtonKeys;
    }
    mm_status_t status = channel->setUsages(usages);
    ASSERT(status== MM_ERROR_SUCCESS);

    status = channel->setPlaylist(-1);
        ASSERT(status == MM_ERROR_SUCCESS);
    status = channel->setPlaylistTitle("title");
    ASSERT(status == MM_ERROR_SUCCESS);

    MediaMetaSP meta = MediaMeta::create();
    meta->setInt32("int32", 1);
    meta->setInt64("int64", 2);
    status = channel->setMetadata(meta);
    ASSERT(status == MM_ERROR_SUCCESS);


    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    int64_t nowUs = t.tv_sec * 1000000LL + t.tv_nsec / 1000LL;

    PlaybackStateSP state = PlaybackState::create();
    state->setState(PlaybackState::State::kNone, -1, 1.0, nowUs);
    state->setCapabilities(YUNOS_MM::PlaybackState::Capabilities(PlaybackState::Capabilities::kCapPause |
        PlaybackState::Capabilities::kCapPlay |
        PlaybackState::Capabilities::kCapNext |
        PlaybackState::Capabilities::kCapPrevious |
        PlaybackState::Capabilities::kCapFastForward |
        PlaybackState::Capabilities::kCapRewind |
        PlaybackState::Capabilities::kCapStop |
        PlaybackState::Capabilities::kCapSeekTo));
    // state->dump();
    status = channel->setPlaybackState(state);
    ASSERT(status == MM_ERROR_SUCCESS);

    channel->setEnabled(false);
    bool isEnabled = channel->isEnabled();
    if (!isEnabled) {
        channel->setEnabled(true);
    }
}

void PlaybackChannelTestHelper::channelRelease(int32_t num)
{
    if (num > 1)
        return;
    if (!mChannels[num])
        return;
    DEBUG("set listener to null begin");
    mChannels[num]->setListener(NULL);
    DEBUG("set listener to null end");
    DEBUG("reset listener begin");
    mChannelListeners[num].reset();
    DEBUG("reset listener end");
    DEBUG("reset channel begin");
    mChannels[num].reset();
    DEBUG("reset channel end");
}

void PlaybackChannelTestHelper::controllerCreate(bool useManager, const char *remoteId, int32_t num)
{
    DEBUG("+");
    if (num > 1)
        return;

    if (useManager) {
        mManager = PlaybackChannelManager::getInstance();
        if (!mManager) {
            ERROR("failed to create PlaybackChannelManager\n");
            return;
        }
        mRemotes.clear();
        mm_status_t status = mManager->getEnabledChannels(mRemotes);
        ASSERT(status == MM_ERROR_SUCCESS);
        ASSERT(mRemotes.size() >= 1);
        mRemote = *mRemotes.begin();
        mRemote->dump();
        mManagerListener.reset(new PlaybackChannelManagerListener());
        mManager->addListener(mManagerListener.get());

        mControllerListener.reset(new PlaybackRemoteListener(mRemote.get()));
        mRemote->addListener(mControllerListener.get());
    } else {
        if (!remoteId) {
            ASSERT(0 && "invalid remoteId");
            ERROR("invalid remoteId");
            return;
        }
        DEBUG("getPlaybackRemoteId, remoteId: %s", remoteId);
        mRemote = PlaybackRemote::create(remoteId);
        if (!mRemote) {
            ERROR("failed to create PlaybackRemote\n");
            return;
        }
        mControllerListener.reset(new PlaybackRemoteListener(mRemote.get()));
        mRemote->addListener(mControllerListener.get());
    }
    DEBUG("-");
}

void PlaybackChannelTestHelper::controllerGet()
{
    MediaMetaSP meta = mRemote->getMetadata();
    int32_t i32;
    bool ret = meta->getInt32("int32", i32);
    DEBUG("ret %d, i32 %d", ret, i32);
    int64_t i64;
    ret = meta->getInt64("int64", i64);
    DEBUG("ret %d, i64 %d", ret, i64);


    PlaybackStateSP playbackState = mRemote->getPlaybackState();
    int32_t state = (int32_t)playbackState->getState();
    DEBUG("state %d", state);

    int64_t pos = playbackState->getPosition();
    DEBUG("pos %" PRId64 "", pos);

    double speed = playbackState->getSpeed();
    DEBUG("speed %f", speed);

    int32_t playlist = mRemote->getPlaylist();
    DEBUG("playlist %d", playlist);

    std::string title = mRemote->getPlaylistTitle();
    DEBUG("title %s", title.c_str());

    int32_t usages = mRemote->getUsages();
    DEBUG("usages 0x%0x", usages);
}

void PlaybackChannelTestHelper::controllerTransportControl()
{
    mm_status_t status = mRemote->play();
    ASSERT(status == MM_ERROR_SUCCESS);
    status = mRemote->pause();
    ASSERT(status == MM_ERROR_SUCCESS);
    status = mRemote->seekTo(64*1024*1024*1024LL);
    ASSERT(status == MM_ERROR_SUCCESS);
    status = mRemote->stop();
    ASSERT(status == MM_ERROR_SUCCESS);
    status = mRemote->next();
    ASSERT(status == MM_ERROR_SUCCESS);
    status = mRemote->previous();
    ASSERT(status == MM_ERROR_SUCCESS);
    status = mRemote->fastForward();
    ASSERT(status == MM_ERROR_SUCCESS);
    status = mRemote->rewind();
    ASSERT(status == MM_ERROR_SUCCESS);
}

void PlaybackChannelTestHelper::remoteSendPlaybackEvent(bool ipc)
{
    remoteSendPlaybackEvent1(ipc);
    remoteSendPlaybackEvent2(ipc);
    remoteSendPlaybackEvent3(ipc);
}

void PlaybackChannelTestHelper::remoteSendPlaybackEvent1(bool)
{
    // short press
    PlaybackEventSP event;
    uint32_t state = 1; // down
    uint32_t keyValue = PlaybackEvent::Keys::kMediaPlay;
    uint32_t repeatCount = 0;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    mm_status_t status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);


    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaStop;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);


    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaPause;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaPrevious;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaNext;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);


    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaFastForward;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaRewind;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

}


// short press for HeadsetHook
// PlaybackChannel receives play or pause callback repeatedly
void PlaybackChannelTestHelper::remoteSendPlaybackEvent2(bool)
{
    PlaybackEventSP event;
    uint32_t state = 1; // down
    uint32_t keyValue = PlaybackEvent::Keys::kHeadsetHook;
    uint32_t repeatCount = 0;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    mm_status_t status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    usleep(100*1000);
    state = 1;
    keyValue = PlaybackEvent::Keys::kHeadsetHook;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);


    usleep(100*1000);
    state = 1;
    keyValue = PlaybackEvent::Keys::kHeadsetHook;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);


    usleep(100*1000);
    state = 1;
    keyValue = PlaybackEvent::Keys::kHeadsetHook;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
}


// long press test
void PlaybackChannelTestHelper::remoteSendPlaybackEvent3(bool ipc)
{
    if (ipc)
        return;
    mm_status_t status;
    if (!(mRemote->getUsages() & PlaybackChannelInterface::Usages::kNeedMediaButtonKeys)) {
        int32_t usages = mRemote->getUsages() | PlaybackChannelInterface::Usages::kNeedMediaButtonKeys;
        status = mChannels[0]->setUsages(usages);
        ASSERT(status == MM_ERROR_SUCCESS);
    }
    PlaybackEventSP event;
    uint32_t state = 1; // down
    uint32_t keyValue = PlaybackEvent::Keys::kHeadsetHook;
    uint32_t repeatCount = 0;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    usleep(500*1000);
    state = 3; // longpress
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    usleep(500*1000);
    state = 2; // repeat
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    usleep(500*1000);
    state = 2; // repeat
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);

    usleep(500*1000);
    state = 2; // repeat
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);


    usleep(500*1000);
    state = 0; // up
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mRemote->sendPlaybackEvent(event);
    ASSERT(status ==  MM_ERROR_SUCCESS);
}

void PlaybackChannelTestHelper::controllerRelease()
{
    if (!mRemote)
        return;
    mRemote->removeListener(mControllerListener.get());
    mControllerListener.reset();
    mRemote.reset();
}

void PlaybackChannelTestHelper::managerRelease()
{
    if (!mManager)
        return;
    mManager->removeListener(mManagerListener.get());
    mManagerListener.reset();
    mManager.reset();
}
}
