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
#include <dbus/dbus.h>

#include "dbus/DAdaptor.h"
#include "dbus/DMessage.h"
#include "dbus/DService.h"
#include "dbus/DServiceManager.h"
#include "looper/Looper.h"
#include "pointer/SharedPtr.h"
#include "string/String.h"
#include "DBusPlayerCommon.h"
#include "pthread.h"
#include "mediacodecPlay.h"

MM_LOG_DEFINE_MODULE_NAME ("DBusServiceService")

#undef LOG_E
#undef LOG_I
#undef LOG_W
#undef LOG_D
#define LOG_E ERROR
#define LOG_I INFO
#define LOG_W WARNING
#define LOG_D DEBUG


using namespace yunos;

class DAdaptorImpl;
void* playThread(void *arg);

class DAdaptorImpl : public DAdaptor {
public:
    DAdaptorImpl(const SharedPtr<DService>& service)
        : DAdaptor(service, String(gObjectPath), String(gInterfaceName))
          , mSignals(0)
          , mID(0)
          , mIsEOS(false)
          , mPlayerThread(0) {
    }

    virtual bool handleMethodCall(const SharedPtr<DMessage>& msg) {
        LOG_I("receive a method call = %s", msg->methodName().c_str());
        if (msg->methodName() == "Add") {
            int32_t p1 = msg->readInt32();
            LOG_I("first argument = %d", p1);
            String p2 = msg->readString();
            int32_t size;
            int8_t* p3 = msg->readByteBuffer(size);
            String result = add(p1, p2, p3, size);
            SharedPtr<DMessage> reply = DMessage::makeMethodReturn(msg);
            reply->writeString(result);
            sendMessage(reply);
            return true;
        } else if (msg->methodName() == "setUri") {
            int ret = setUri(msg);
            SharedPtr<DMessage> reply = DMessage::makeMethodReturn(msg);
            reply->writeInt32(ret);
            sendMessage(reply);
            return true;
        } else if (msg->methodName() == "play") {
            int ret = play(msg);
            SharedPtr<DMessage> reply = DMessage::makeMethodReturn(msg);
            reply->writeInt32(ret);
            sendMessage(reply);
            return true;
        } else if (msg->methodName() == "pause") {
            int ret = pause(msg);
            SharedPtr<DMessage> reply = DMessage::makeMethodReturn(msg);
            reply->writeInt32(ret);
            sendMessage(reply);
            return true;
        } else if (msg->methodName() == "stop") {
            int ret = stop(msg);
            SharedPtr<DMessage> reply = DMessage::makeMethodReturn(msg);
            reply->writeInt32(ret);
            sendMessage(reply);
            return true;
        } else if (msg->methodName() == "quit") {
            int ret = quit(msg);
            SharedPtr<DMessage> reply = DMessage::makeMethodReturn(msg);
            reply->writeInt32(ret);
            sendMessage(reply);
            return true;
        } else if (msg->methodName() == "setSurface") {
            int ret = setSurface(msg);
            SharedPtr<DMessage> reply = DMessage::makeMethodReturn(msg);
            reply->writeInt32(ret);
            sendMessage(reply);
            return true;
        }

        LOG_W("can't understand method call: %s", msg->methodName().c_str());
        return false;
    }

    int setUri(const SharedPtr<DMessage>& msg)
    {
        mUri = msg->readString();
        LOG_I("setUri uri=%s\n", mUri.c_str());

        if (pthread_create(&mPlayerThread, NULL, playThread, this) != 0) {
            LOG_E("fail to create input worker thread");
            return -1;
        }

        return 0;
    }
    int play(const SharedPtr<DMessage>& msg)
    {
        LOG_I("play\n");

        return 0;
    }
    int pause(const SharedPtr<DMessage>& msg)
    {
        LOG_I("pause\n");

        return 0;
    }
    int stop(const SharedPtr<DMessage>& msg)
    {
        LOG_I("stop\n");

        return 0;
    }
    int quit(const SharedPtr<DMessage>& msg)
    {
        LOG_I("quit\n");

        return 0;
    }
    int setSurface(const SharedPtr<DMessage>& msg)
    {
        mSurface = msg->readString();
        mID = msg->readInt32();
        LOG_I("setSurface uri=%s id=%d\n", mSurface.c_str(), mID);

        return 0;
    }

    String add(int32_t data, const String& str, int8_t* array, int32_t size) {
        return String::format("result = %d + %s + %d + %d",
                data, str.c_str(), array[0], array[1]);
    }

    /**
     * test to send a signalA to client.
     */
    void sendSignalA() {
        mSignals++;
        SharedPtr<DMessage> msg = obtainSignalMessage(String("signalA"));
        msg->writeString(String::format("hello-signal-%d", mSignals));
        LOG_I("send signal message = %d", mSignals);
        sendMessage(msg);
    }

    /**
     * send EOS to client.
     */
    void sendSignalEOS() {
        SharedPtr<DMessage> msg = obtainSignalMessage(String("EOS"));
        msg->writeString(String("end-of-stream"));
        LOG_I("send EOS signal message");
        sendMessage(msg);
    }

    String getUri() {
        return mUri;
    }
    String getSurface() {
        return mSurface;
    }
    int getID() {
        return mID;
    }
    void setEOS(bool isEOS) {
        mIsEOS = isEOS;
    }
    bool getEOS() {
        return mIsEOS;
    }
private:
    int mSignals;
    pthread_t mPlayerThread;
    String mUri;
    String mSurface;
    int mID; /* id for Surface */
    bool mIsEOS;
};

// Used to send signals timely.
static void handleTimer(void* data) {
    DAdaptorImpl* impl = static_cast<DAdaptorImpl*>(data);
    impl->sendSignalA();
    if (impl->getEOS()) {
        impl->sendSignalEOS();
        impl->setEOS(false);
    }
    Looper::current()->sendDelayedTask(Task(handleTimer, data), 2000);
}

void* playThread(void *arg) {
    class DAdaptorImpl *adaptor = (class DAdaptorImpl *) arg;
    playVideo(adaptor->getUri().c_str(), (void *)adaptor->getSurface().c_str(), adaptor->getID());
    adaptor->setEOS(true);
    return NULL;
}

int main(int argc, char **argv) {
    // run as a dbus service.
    LOG_I("DBusPlayerService");

    IoLooper looper;

    SharedPtr<DServiceManager> instance = DServiceManager::getInstance();
    SharedPtr<DService> service = instance->registerService(String(gServiceName));
    DAdaptorImpl adaptor(service);
    adaptor.publish();

    looper.sendDelayedTask(Task(handleTimer, static_cast<void*>(&adaptor)), 2000);
    looper.run();

    return 0;
}
