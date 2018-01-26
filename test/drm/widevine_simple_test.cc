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

#include "media_keys.h"
#include "media_key_session.h"
#include "media_decrypt.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define Log(format, ...)  printf("[WVSimpleTest]%s(L: %d): " format"\n", __func__, __LINE__, ##__VA_ARGS__)

#define WVClearKey {0x10,0x77,0xEF,0xEC,0xC0,0xB2,0x4D,0x02,0xAC,0xE3,0x3C,0x1E,0x52,0xE2,0xFB,0x4B}
#define WidevineUUID {0xED,0xEF,0x8B,0xA9,0x79,0xD6,0x4A,0xCE,0xA3,0xC8,0x27,0xDC,0xD5,0x1D,0x21,0xED}

bool gClearKey = true;

using namespace YUNOS_DRM;

class TestListener;

class DrmEngineTest {

public:
    DrmEngineTest();
    ~DrmEngineTest() {
        if (mediaKeys)
            delete mediaKeys;
        if(mListener){
            delete mListener;
            mListener = NULL;
        }
    }

private:

    std::string WVUUID;
    MediaKeys *mediaKeys;
    SessionID sessionId;
    MediaKeySession *session;

    DrmLock mLock;
    DrmCondition mCondition;

    TestListener *mListener;

    struct MediaKeyEvent {
        MediaKeyEvent(MediaKeyMessageEvent event, const SessionID &session, const Message &msg)
            : type(event),
              sessionId(session),
              message(msg) {

        }

        MediaKeyMessageEvent type;
        const SessionID sessionId;
        const Message message;
    };

    struct OtherEvent {
        OtherEvent(Event event, int param, const SessionID &session)
            : type(event),
              param1(param),
              sessionId(session) {

        }

        Event type;
        int param1;
        const SessionID sessionId;
    };

    typedef std::tr1::shared_ptr<MediaKeyEvent> MediaKeyEventSP;
    typedef std::tr1::shared_ptr<OtherEvent> OtherEventSP;

    std::list<MediaKeyEventSP> mMediaKeyEventQueue;
    std::list<OtherEventSP> mEventQueue;

public:

friend class TestListener;

public:

#define action_open_session	1 
#define action_close_session	2
#define action_provision	3
#define action_load		5
#define action_restore_keys	6
#define action_check_keys	7
#define action_remove_keys	8
#define action_release_keys	9
#define action_show_properties	10

public:
    bool onAction(int item) {
        switch (item) {
            case action_open_session:
                onOpenSession();
                return true;
            case action_close_session:
                onCloseSession();
                return true;
            case action_provision:
                onPersonalize();
                return true;
            case action_load:
                onLoad();
                return true;
            case action_restore_keys:
                onRestoreKeys();
                return true;
            case action_check_keys:
                onCheckKeys();
                return true;
            case action_remove_keys:
                onRemoveKeys();
                return true;
            case action_release_keys:
                onReleaseKeys();
                return true;
            case action_show_properties:
                onShowProperties();
                return true;
            default:
                ;
        }
        return false;
    }

private:
    void addOutput(std::string &str) {
        Log("%s\n", str.c_str());
    }

    void onPersonalize() {

        //Log("doesn't need explict personalization , Wasabi do that automaticlly");
    }

    /* DrmParam is utils Class provided by YunOS DRM Core */
    void CreateByteArrary(DrmParam &p, std::string &string) {
        int32_t len = p.readInt32();

        if (len <= 0)
            return;

        char *data = new char[len + 1];
        data[len] = '\0';

        p.read(data, len);

        string.append(data);

        delete [] data;
    }

    bool waitKeyEvent(MediaKeyMessageEvent event, Message& message) {

        bool found = false;

    retry:
        DrmAutoLock lock(mLock);
        if (mMediaKeyEventQueue.empty()) {
            mCondition.wait();
            goto retry;
        }

        std::list<MediaKeyEventSP>::iterator it = mMediaKeyEventQueue.begin();

        for (; it != mMediaKeyEventQueue.end(); it++) {
            if ((*it)->type == event) {
                found = true;
                message = (*it)->message;
                // TODO erase item
                break;
            }
            //(*it)->sessionId == session->getSessionId();
        }

        return found;
    }

    bool waitEvent(Event event) {

        bool found = false;

    retry:
        DrmAutoLock lock(mLock);
        if (mEventQueue.empty()) {
            mCondition.wait();
            goto retry;
        }

        std::list<OtherEventSP>::iterator it = mEventQueue.begin();

        for (; it != mEventQueue.end(); it++) {
            if ((*it)->type == event) {
                found = true;
            }
            //(*it)->sessionId == session->getSessionId();
        }

        return found;
    }

    void onOpenSession();

    void onLoad() {
        if (!session) {
            Log("session is empty");
            return;
        }

        Message request;
        std::vector<uint8_t> initData;
        initData.resize(16);
        for (int i = 0; i < 16; i++)
            initData[i] = i + 'a';

        session->generateRequest("webm", initData);

        if (!waitKeyEvent(EventLicenseRequest, request)) {
            Log("session generate license requense fail");
            return;
        }

        MediaDecrypt* decrypt = session->getMediaDecrypt();
        if (!decrypt) {
            Log("fail to create decrypt object");
            return;
        }

        bool ret = decrypt->requireSecureDecoder("webm");
        Log("check webm support: %d", ret);
    }

    void onRestoreKeys() {
    }

    void onCheckKeys() {
    }

    void onRemoveKeys() {

        if (mediaKeys == NULL || session == NULL) {
            Log("Drm is not initialized");
            return;
        }

        session->close();
        session->remove();

        // check callback needed?

        Log("Remove keys: succeeded");
    }

    void onReleaseKeys() {
    }

    void onShowProperties() {
    }

    void onCloseSession() {
        if (session == NULL) {
            Log("Error: session is null");
        } else {
            if (session) {
                session->remove();
                session->close();
            }

            delete mediaKeys;

            session = NULL;
            mediaKeys = NULL;
            Log("Close session: succeeded");
        }
    }

};

// Implement TestListener as state machine, status changing is driven by event
class TestListener : public DrmListener {

public:
    TestListener(DrmEngineTest *owner)
        : mOwner(owner) {

    }

    ~TestListener() {Log("destruct TestListener");}

    virtual void onMediaKeyMessage(MediaKeyMessageEvent type, const SessionID &sessionId, const Message &message) {
        Log("key message %d", type);
        if (!mOwner)
            return;

        DrmAutoLock lock(mOwner->mLock);
        DrmEngineTest::MediaKeyEventSP keyEvent(new DrmEngineTest::MediaKeyEvent(type, sessionId, message));
        mOwner->mMediaKeyEventQueue.push_back(keyEvent);
        mOwner->mCondition.signal();
    }

    virtual void onMessage(Event type, int param1, const SessionID &sessionId) {
        Log("event message %d", type);
        if (!mOwner)
            return;

        DrmAutoLock lock(mOwner->mLock);
        DrmEngineTest::OtherEventSP event(new DrmEngineTest::OtherEvent(type, param1, sessionId));
        mOwner->mEventQueue.push_back(event);
        mOwner->mCondition.signal();
    }

private:
    DrmEngineTest *mOwner;
};

DrmEngineTest::DrmEngineTest()
        : session(NULL),
          mCondition(mLock) {
    uint8_t systemName[] = WVClearKey;
    uint8_t systemName1[] = WidevineUUID;

    for (size_t i = 0; i < sizeof(systemName); i++) {
        if (gClearKey)
            WVUUID.append(1, systemName[i]);
        else
            WVUUID.append(1, systemName1[i]);
    }

    mediaKeys = NULL;
    mListener = new TestListener(this);
}

void DrmEngineTest::onOpenSession() {
    if (session != NULL) {
        Log("Error: session is not null");
    } else {
        mediaKeys = MediaKeys::createMediaKeys(WVUUID.c_str());

        session = mediaKeys->createSession(SessionTypeTemporary);

        Log("session %p", session);
        if (session) {
            Log("Open widevine session: succeeded");
            session->setListener(mListener);
        }
    }
}

void parseCommandLine(int argc, char **argv)
{
    int res;
    while ((res = getopt(argc, argv, "cw")) >= 0) {
        switch (res) {
            case 'c':
                gClearKey = true;
                break;
            case 'w':
                gClearKey = false;
                break;
            default:
                Log("usage: -c|w");
                break;
        }
    }
}


int main(int argc, char *argv[]) {

    parseCommandLine(argc, argv);

    Log("*********************Widevine Simple Test Started*********************");
    DrmEngineTest *test = new DrmEngineTest();
    Log("open");
    test->onAction(action_open_session);
    //test->onAction(action_provision);
    Log("load");
    test->onAction(action_load);
    //test->onAction(action_close_session);

    delete test;

    return 0;
}
