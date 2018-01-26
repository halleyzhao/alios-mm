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
#include "PlaybackChannelManagerImp.h"
#include <dbus/DError.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>
#include "multimedia/UBusHelper.h"
#include "multimedia/UBusNode.h"
#include "multimedia/UBusMessage.h"


namespace YUNOS_MM {
using namespace yunos;
DEFINE_LOGTAG1(PlaybackChannelManagerImp, [PC])
DEFINE_LOGTAG1(PlaybackChannelManagerProxy, [PC])

enum {
    MSG_CREATE_CHANNEL = 0,
    MSG_DESTROY_CHANNEL,
    MSG_DISPATCH_PLAYBACK_EVENT,
    MSG_GET_ENABLE_CHANNELS,
    MSG_DUMPSYS,
    MSG_ADD_LISTENER,
    MSG_REMOVE_LISTENER,
    MSG_ADD_CHANNEL_INFO,
    MSG_REMOVE_CHANNEL_INFO
};

#define MSG_STR(MSG) do{\
    return ##MSG; \
}while(0)

static int mTimeOut = -1;
////////////////////////////////////////////////////////////////////////////
//PlaybackChannelManagerImp
PlaybackChannelManagerImp::PlaybackChannelManagerImp()
{
    INFO("+\n");
}

PlaybackChannelManagerImp::~PlaybackChannelManagerImp()
{
    INFO("+\n");
    if (mClientNode) {
        removeListener(NULL);
    }
    mClientNode.reset();
}

bool PlaybackChannelManagerImp::connect()
{
    INFO("+\n");
    try {
        mClientNode = ClientNode<PlaybackChannelManagerProxy>::create("msmp");
        if (!mClientNode->init()) {
            ERROR("failed to init\n");
            mClientNode.reset();
            return false;
        }

        INFO("success\n");
        mMessager = new UBusMessage<PlaybackChannelManagerImp>(this);

#ifdef __USING_PLAYBACK_CHANNEL_FUSION__
        mContainerWrapper.reset(new ContainerSessionManagerWrapper);
#endif
        return true;
    } catch (...) {
        ERROR("no mem\n");
        return false;
    }
}
std::string PlaybackChannelManagerImp::createChannel(const char* tag, const char*packageName, int32_t userId)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_CREATE_CHANNEL);
    param->writeCString(tag);
    param->writeCString(packageName);
    param->writeInt32(userId);
    mMessager->sendMsg(param);
    return param->readCString();
}

void PlaybackChannelManagerImp::destroyChannel(const char* channelId)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_DESTROY_CHANNEL);
    param->writeCString(channelId);
    mMessager->sendMsg(param);
}

mm_status_t PlaybackChannelManagerImp::addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_ADD_CHANNEL_INFO);
    param->writeCString(tag);
    param->writeInt32(userId);
    param->writeInt32(callingPid);
    param->writeCString(packageName);
    mMessager->sendMsg(param);
    return param->readInt32();
}

void PlaybackChannelManagerImp::removeChannelInfo(const char*packageName)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_REMOVE_CHANNEL_INFO);
    param->writeCString(packageName);
    mMessager->sendMsg(param);
}

mm_status_t PlaybackChannelManagerImp::dispatchPlaybackEvent(PlaybackEventSP event, bool needWakeLock)
{
    if (!PlaybackEvent::isMediaKey(event->getKey())) {
        ERROR("non-media key event, key %d", event->getKey());
        return MM_ERROR_INVALID_PARAM;
    }
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_DISPATCH_PLAYBACK_EVENT);
    event->writeToMMParam(param.get());
    param->writeInt32(int32_t(needWakeLock));
    mMessager->sendMsg(param);
    mm_status_t status = param->readInt32();

#ifdef __USING_PLAYBACK_CHANNEL_FUSION__
    if(event->isFromHost()) {
        if (status == MM_ERROR_NOT_HANDLED) {
            INFO("media key is from host, will be dispatched to container", status);
            // focus window is host and will dispatch to container
            if (mContainerWrapper) {
                mContainerWrapper->dispatchMediaKeyEvent(event->getType(),
                    event->getKey(),
                    event->getRepeatCount(),
                    event->getUsages(),
                    event->getDownTime(),
                    needWakeLock);
            }
        } else {
            // focus window is host, but return with error,
            // do not dispatch to container
            INFO("media key is from host, handled by host %d", status);
        }
    } else {
        if (status == MM_ERROR_NOT_HANDLED) {
            INFO("media key is from container, will be dispatched to container %d", status);
            // focus window is container and will dispatch to container
        } else {
            INFO("media key is from container, handled by host %d", status);
        }
    }
#endif
    INFO("%s", status == MM_ERROR_SUCCESS ? "success" : "failed");
    return status;
}

mm_status_t PlaybackChannelManagerImp::getEnabledChannels(std::vector<std::string> &remoteIds)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_GET_ENABLE_CHANNELS);
    mm_status_t status = mMessager->sendMsg(param);
    status = param->readInt32();
    if (status == MM_ERROR_SUCCESS) {
        remoteIds.clear();
        int32_t size = param->readInt32();
        for (int32_t i = 0; i < size; i++) {
            std::string id = param->readCString();
            remoteIds.push_back(id);
        }
    }
    return status;
}

mm_status_t PlaybackChannelManagerImp::getEnabledChannels(std::vector<PlaybackRemoteSP> &remotes)
{
    remotes.clear(); // first to clear vector
    std::vector<std::string> remoteIds;
    remoteIds.clear();
    mm_status_t status = getEnabledChannels(remoteIds);
    if (status == MM_ERROR_SUCCESS) {
        for (auto i = remoteIds.begin(); i != remoteIds.end(); i++) {
            PlaybackRemoteSP remote = PlaybackRemote::create((*i).c_str());
            if (remote) {
                DEBUG("create PlaybackRemote with id: %s success", (*i).c_str());
                remotes.push_back(remote);
            } else {
                ERROR("PlaybackRemote create failed with id: %s", (*i).c_str());
                remotes.clear();
                return MM_ERROR_UNKNOWN;
            }
        }
    }
    return MM_ERROR_SUCCESS;
}

mm_status_t PlaybackChannelManagerImp::handleMsg(MMParamSP param)
{
    int msg = param->readInt32();
    DEBUG("name %s", msgName(msg).c_str());
    bool set = true;

    mm_status_t status = MM_ERROR_SUCCESS;
    switch (msg) {
        case MSG_ADD_CHANNEL_INFO:
        {
            const char *tag = param->readCString();
            int32_t userId = param->readInt32();
            int32_t callingPid = param->readInt32();
            const char* packageName = param->readCString();
            status = mClientNode->node()->addChannelInfo(tag, packageName, userId, callingPid);
            break;
        }
        case MSG_REMOVE_CHANNEL_INFO:
        {
            const char* packageName = param->readCString();
            mClientNode->node()->removeChannelInfo(packageName);
            set = false;
            break;
        }
        case MSG_CREATE_CHANNEL:
        {
            const char* tag = param->readCString();
            const char* packageName = param->readCString();
            int32_t userId = param->readInt32();
            std::string channelId = mClientNode->node()->createChannel(tag, packageName, userId);
            param->writeCString(channelId.c_str());
            set = false;
            break;
        }
        case MSG_DESTROY_CHANNEL:
        {
            const char* channelId = param->readCString();
            mClientNode->node()->destroyChannel(channelId);
            set = false;
            break;
        }
        case MSG_DISPATCH_PLAYBACK_EVENT:
        {
            PlaybackEventSP event = PlaybackEvent::create();
            event->readFromMMParam(param.get());
            bool needWakeLock = (bool)param->readInt32();
            status = mClientNode->node()->dispatchPlaybackEvent(event, needWakeLock);
            break;
        }
        case MSG_GET_ENABLE_CHANNELS:
        {
            std::vector<std::string> remoteIds;
            remoteIds.clear();
            status = mClientNode->node()->getEnabledChannels(remoteIds);
            param->writeInt32(status);
            if (status == MM_ERROR_SUCCESS) {
                DEBUG("remoteIds.size %d", remoteIds.size());
                param->writeInt32(remoteIds.size());
                for (uint32_t i = 0; i < remoteIds.size(); i++) {
                    param->writeCString(remoteIds[i].c_str());
                }
            } else {
                ERROR("getEnabledChannels failed");
            }
            set = false;
            break;
        }
        case MSG_DUMPSYS:
        {
            std::string info = mClientNode->node()->dumpsys();
            param->writeCString(info.c_str());
            set = false;
            break;
        }
        case MSG_ADD_LISTENER:
        {
            Listener *listener = (Listener*)param->readRawPointer();
            INFO("mListener.size %d, listener %p\n", mListener.size(), listener);
            auto it = mListener.find(listener);
            if (it != mListener.end()) {
                DEBUG("listener %p is already exists", listener);
                return MM_ERROR_SUCCESS;
            }
            mListener.insert(std::pair<Listener*, int32_t>(listener, 0));

            if (mListener.size() > 1) {
                return MM_ERROR_SUCCESS;
            }

            status = mMessager->addRule(mMessager);
            if (status != MM_ERROR_SUCCESS) {
                return status;
            }

            // consider userId in future
            set = false;
            return mClientNode->node()->addChannelsListener(mMessager->callbackName().c_str());
        }
        case MSG_REMOVE_LISTENER:
        {
            Listener *listener = (Listener*)param->readRawPointer();
            if (mListener.empty())
                return MM_ERROR_SUCCESS;
            if (listener) {
                auto it = mListener.find(listener);
                if (it == mListener.end()) {
                    ERROR("listener %p is not exists", listener);
                    return MM_ERROR_INVALID_PARAM;
                }
                DEBUG("listener.size %d, remove listener: %p", mListener.size(), it->first);

                mListener.erase(it);
            } else {
                // clear all the listeners
                mListener.clear();
            }

            if (mListener.empty()) {
                mMessager->removeRule();
            }
            set = false;
            return MM_ERROR_SUCCESS;
        }
        default:
        {
            ASSERT(0 && "Should not be here\n");
            break;
        }
    }

    if (set)
        param->writeInt32(status);
    return status;
}

bool PlaybackChannelManagerImp::handleSignal(const SharedPtr<DMessage> &msg) {
    if (mListener.empty()) {
        WARNING("mListener is empty, just return");
        return true;
    }

    int32_t status = msg->readInt32();
    if (status == MM_ERROR_SUCCESS) {
        int32_t size = msg->readInt32();
        INFO("callback status %d, manager listener size %d", status, size);
        std::vector <std::string> remoteIds;
        for (int32_t i = 0; i < size; i++) {
            std::string str = msg->readString().c_str();
            remoteIds.push_back(str);
        }

        std::vector<PlaybackRemoteSP> remotes;
        MMParamSP param;
        param.reset(new MMParam);
        param->writeInt32(remoteIds.size());
        for (auto i = remoteIds.begin(); i != remoteIds.end(); i++) {
            param->writeCString((*i).c_str());
            PlaybackRemoteSP remote = PlaybackRemote::create((*i).c_str());
            if (remote) {
                remotes.push_back(remote);
            } else {
                ERROR("PlaybackRemote create failed with id: %s", (*i).c_str());
                remotes.clear();
                return false;
            }
        }

        // Notify listeners
        for (auto it = mListener.begin(); it != mListener.end(); it++) {
            DEBUG("notify listener %p", *it);
            it->first->onEnabledChannelsChanged(remotes);
            // read out in PlaybackChannelManager.js
            it->first->onMessage(0, 0, 0, param.get());
        }
    } else {
        WARNING("no mediasessioin is service");
        return false;
    }

    return true;
}

mm_status_t PlaybackChannelManagerImp::addListener(Listener * listener)
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

void PlaybackChannelManagerImp::removeListener(Listener * listener)
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_REMOVE_LISTENER);
    param->writeRawPointer((uint8_t*)listener);
    mMessager->sendMsg(param);
}

std::string PlaybackChannelManagerImp::dumpsys()
{
    MMParamSP param(new MMParam);
    param->writeInt32(MSG_DUMPSYS);
    mMessager->sendMsg(param);
    return param->readCString();
}

std::string PlaybackChannelManagerImp::msgName(int msg)
{
#define MSG_NAME(MSG) case (MSG): return #MSG;
    switch (msg) {
        MSG_NAME(MSG_CREATE_CHANNEL);
        MSG_NAME(MSG_DESTROY_CHANNEL);
        MSG_NAME(MSG_DISPATCH_PLAYBACK_EVENT);
        MSG_NAME(MSG_GET_ENABLE_CHANNELS);
        MSG_NAME(MSG_DUMPSYS);
        MSG_NAME(MSG_ADD_LISTENER);
        MSG_NAME(MSG_REMOVE_LISTENER);
        MSG_NAME(MSG_ADD_CHANNEL_INFO);
        MSG_NAME(MSG_REMOVE_CHANNEL_INFO);
        default: return "unknown msg";
    }
    return "unknown msg";
}

////////////////////////////////////////////////////////////////////////////////////
PlaybackChannelManagerProxy::PlaybackChannelManagerProxy(const yunos::SharedPtr<yunos::DService>& service,
    yunos::String serviceName, yunos::String pathName, yunos::String iface, void *arg)
        : yunos::DProxy(service, yunos::String(PlaybackChannelManagerInterface::pathName())
        , yunos::String(PlaybackChannelManagerInterface::iface()))
        //, mImp(static_cast<PlaybackChannelManagerImp*>(arg))
{
    INFO("+\n");
}

PlaybackChannelManagerProxy::~PlaybackChannelManagerProxy()
{
    INFO("+\n");
}

mm_status_t PlaybackChannelManagerProxy::addChannelInfo(const char* tag, const char*packageName, int32_t userId, int32_t callingPid)
{
    CHECK_OBTAIN_MSG_RETURN1("addChannelInfo", MM_ERROR_NO_MEM);
    msg->writeString(yunos::String(tag));
    msg->writeString(yunos::String(packageName));
    msg->writeInt32(userId);
    msg->writeInt32(callingPid);

    CHECK_REPLY_MSG_RETURN_INT();
}

void PlaybackChannelManagerProxy::removeChannelInfo(const char*packageName)
{
    CHECK_OBTAIN_MSG_RETURN0("removeChannelInfo");
    msg->writeString(yunos::String(packageName));

    HANDLE_REPLY_MSG_RETURN0();
}

void PlaybackChannelManagerProxy::destroyChannel(const char* channelId)
{
    CHECK_OBTAIN_MSG_RETURN0("destroyChannel");
    msg->writeString(yunos::String(channelId));

    HANDLE_REPLY_MSG_RETURN0();
}
std::string PlaybackChannelManagerProxy::createChannel(const char* tag, const char *packageName, int32_t userId)
{
    CHECK_OBTAIN_MSG_RETURN1("createChannel", INVALID_CHANNEL_ID);

    // DEBUG("%s %s %d\n", tag, packageName, userId);

    msg->writeString(yunos::String(tag));
    msg->writeString(yunos::String(packageName));
    msg->writeInt32(userId);

    CHECK_REPLY_MSG_RETURN_STRING(INVALID_CHANNEL_ID);
}

mm_status_t PlaybackChannelManagerProxy::dispatchPlaybackEvent(PlaybackEventSP event, bool needWakeLock)
{
    CHECK_OBTAIN_MSG_RETURN1("dispatchPlaybackEvent", MM_ERROR_NO_MEM);

    event->writeToMsg(msg);
    msg->writeBool(needWakeLock);
    #if 0
    mTimeOut = 3000; //ms
    CHECK_REPLY_MSG_RETURN_INT();
    mTimeOut = -1;
    #else
    bool ret = sendMessage(msg);
    return ret == true ? MM_ERROR_SUCCESS : MM_ERROR_UNKNOWN;
    #endif
}

mm_status_t PlaybackChannelManagerProxy::getEnabledChannels(std::vector<std::string> &remoteIds)
{
    CHECK_OBTAIN_MSG_RETURN1("getEnabledChannels", MM_ERROR_NO_MEM);

    HANDLE_INVALID_REPLY_RETURN_INT();

    int32_t status = reply->readInt32();
    if (status == MM_ERROR_SUCCESS) {
        int32_t size = reply->readInt32();
        for (int32_t i = 0; i < size; i++) {
            std::string str = reply->readString().c_str();
            remoteIds.push_back(str);
        }
    } else {
        ERROR("error occur");
    }
    return status;
}

mm_status_t PlaybackChannelManagerProxy::addChannelsListener(const char*callbackName)
{
    CHECK_OBTAIN_MSG_RETURN1("addChannelsListener", MM_ERROR_NO_MEM);

    msg->writeString(yunos::String(callbackName));

    CHECK_REPLY_MSG_RETURN_INT();
}

std::string PlaybackChannelManagerProxy::dumpsys()
{
    SEND_UBUS_MESSAGE_PARAM0_RETURNSTR("dumpsys");
}


#if 0
// Called in msg thread
void PlaybackChannelManagerProxy::onDeath(const DLifecycleListener::DeathInfo& deathInfo)
{
    INFO("media_service die: %s, mSeq %d",
        deathInfo.mName.c_str(), deathInfo.mSeq);

    if (!mImp || !mImp->mListener) {
        WARNING("media channel is already destroyed");
        return;
    }

    DProxy::onDeath(deathInfo);
    mImp->mListener->onMessage(PlaybackChannelManager::Listener::kMsgError, MM_ERROR_SERVER_DIED, 0, NULL);

}
#endif
}
