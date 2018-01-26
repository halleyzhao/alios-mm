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
//#include <compat/media/surface_texture_client_compat.h>

#include "dbus/DMessage.h"
#include "dbus/DProxy.h"
#include "dbus/DService.h"
#include "dbus/DServiceManager.h"
#include "looper/Looper.h"
#include "pointer/SharedPtr.h"
#include "string/String.h"
#include "DBusPlayerCommon.h"

MM_LOG_DEFINE_MODULE_NAME ("DBusPlayerClient")

#undef LOG_E
#undef LOG_I
#undef LOG_W
#undef LOG_D
#define LOG_E ERROR
#define LOG_I INFO
#define LOG_W WARNING
#define LOG_D DEBUG


using namespace yunos;


class DProxyImpl : public DProxy {
public:
    enum PlayerStatus {
        PlayerStatusInvalid,
        PlayerStatusLoaded,
        PlayerStatusPlaying,
        PlayerStatusPaused,
        PlayerStatusStoped,
    };
    PlayerStatus mPlayerStatus;

    DProxyImpl(const SharedPtr<DService>& service)
        : DProxy(service, String(gObjectPath),
                String(gInterfaceName)) {
        mRequest = 0;
        mReplied = 0;
        mPlayerStatus = PlayerStatusInvalid;
    }

    void setUri(char *uri) {
        SharedPtr<DMessage> msg = obtainMethodCallMessage(String("setUri"));
        String s(uri);
        msg->writeString(s);
        sendMessageWithReply(msg, ReplyHandler(handleSetUri, this));
    }
    void setSurface(char *name, int id) {
        SharedPtr<DMessage> msg = obtainMethodCallMessage(String("setSurface"));
        String s(name);
        msg->writeString(s);
        msg->writeInt32(id);
        sendMessageWithReply(msg, ReplyHandler(handleSetSurface, this));
    }


    static void handleSetUri(void* data, const SharedPtr<DMessage>& msg) {
        DProxyImpl* impl = static_cast<DProxyImpl*>(data);
        int result = msg->readInt32();
        if (result == 0)
            impl->mPlayerStatus = PlayerStatusLoaded;

        LOG_I("the result of handleSetUri: %d\n",result);
    }

    static void handleSetSurface(void* data, const SharedPtr<DMessage>& msg) {
        int result = msg->readInt32();

        LOG_I("the result of handleSetSurface: %d\n",result);
    }


    void play() {
        SharedPtr<DMessage> msg = obtainMethodCallMessage(String("play"));
        sendMessageWithReply(msg, ReplyHandler(handlePlay, this));
    }

    static void handlePlay(void* data, const SharedPtr<DMessage>& msg) {
        DProxyImpl* impl = static_cast<DProxyImpl*>(data);
        int result = msg->readInt32();
        if (result == 0)
            impl->mPlayerStatus = PlayerStatusPlaying;

        LOG_I("the result of handlePlay: %d\n",result);
    }

    void pause() {
        SharedPtr<DMessage> msg = obtainMethodCallMessage(String("pause"));
        sendMessageWithReply(msg, ReplyHandler(handlePause, this));
    }

    static void handlePause(void* data, const SharedPtr<DMessage>& msg) {
        DProxyImpl* impl = static_cast<DProxyImpl*>(data);
        int result = msg->readInt32();
        if (result == 0)
            impl->mPlayerStatus = PlayerStatusPaused;

        LOG_I("the result of handlePause: %d\n",result);
    }

    void stop() {
        SharedPtr<DMessage> msg = obtainMethodCallMessage(String("stop"));
        sendMessageWithReply(msg, ReplyHandler(handleStop, this));
    }

    static void handleStop(void* data, const SharedPtr<DMessage>& msg) {
        DProxyImpl* impl = static_cast<DProxyImpl*>(data);
        int result = msg->readInt32();
        if (result == 0)
            impl->mPlayerStatus = PlayerStatusStoped;

        LOG_I("the result of handleStop: %d\n",result);
    }

    void quit() {
        SharedPtr<DMessage> msg = obtainMethodCallMessage(String("quit"));
        sendMessageWithReply(msg, ReplyHandler(handleQuit, this));
    }

    static void handleQuit(void* data, const SharedPtr<DMessage>& msg) {
        DProxyImpl* impl = static_cast<DProxyImpl*>(data);
        int result = msg->readInt32();
        if (result == 0)
            impl->mPlayerStatus = PlayerStatusInvalid;

        LOG_I("the result of handleQuit: %d\n",result);
    }

/**
* handle signals sent from service.
*/
    virtual bool handleSignal(const SharedPtr<DMessage>& msg) {
        LOG_I("handle signal = %s", msg->signalName().c_str());
        if (msg->signalName() == "signalA") {
            String p = msg->readString();
            LOG_I("SignalA result = %s", p.c_str());
            return true;
        } else if (msg->signalName() == "EOS") {
            String p = msg->readString();
            LOG_I("got EOS: %s", p.c_str());
            Looper::current()->quit();
            LOG_I("quit ...");
            return true;
        }
        return false;
    }

private:
    int mRequest;
    int mReplied;
};

int main(int argc, char **argv) {
    LOG_I("DBusPlayerClient\n");
    static char* input_url = NULL;
    IoLooper looper;
    SharedPtr<DServiceManager> instance = DServiceManager::getInstance();
    SharedPtr<DService> service = instance->getService(String(gServiceName));
    DProxyImpl proxyImpl(service);
    // enable signals.
    proxyImpl.enableSignals();

    // call two remote methods.
    if (argc<2) {
        LOG_E("no input file\n");
        return -1;
    }
    input_url = argv[1];
    LOG_I("\n test inpute url = %s \n",input_url);

#if 0
    SurfaceTextureClientHybris  stc = NULL;
    stc = surface_texture_client_create();
    if(stc == NULL){
        LOG_E("surface texture client create fail!\n");
        return -1;
    }

    char name[128];
    int id;

    //surface_texture_client_register(stc, name, &id);
    LOG_I("register Surface for sharing: service %s[%d]\n", name, id);

    proxyImpl.setSurface(name, id);
#endif
    proxyImpl.setUri(input_url);
    proxyImpl.play();
    proxyImpl.pause();
    proxyImpl.stop();
    proxyImpl.quit();

    // run the message loop.
    Looper::current()->run();

    return 0;
}

