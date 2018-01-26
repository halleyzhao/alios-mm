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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define Log(format, ...)  printf("[MarlinSimpleTest]%s(L: %d): " format"\n", __func__, __LINE__, ##__VA_ARGS__)

// Marlin system ID
#define MarlinID { 0x69, 0xf9, 0x08, 0xaf, 0x48, 0x16, 0x46, 0xea, 0x91, 0x0c, 0xcd, 0x5d, 0xcc, 0xcb, 0x0a, 0x3a }

using namespace YUNOS_DRM;

class TestListener;

class DrmEngineTest {

public:
    DrmEngineTest(bool is_ms3 = false);
    ~DrmEngineTest() {
        if(mListener){
            delete mListener;
            mListener = NULL;
        }
    }

private:

    bool ms3;
    int WASABI_DRM_PLUGIN_VERSION;
    std::string MarlinUUID;
    std::string cid;
    std::string ms3_server_url;
    std::string bb_server_url;
    std::string ms3_url;
    std::string bb_token;
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
        const SessionID &sessionId;
        const Message &message;
    };

    struct OtherEvent {
        OtherEvent(Event event, int param, const SessionID &session)
            : type(event),
              param1(param),
              sessionId(session) {

        }

        Event type;
        int param1;
        const SessionID &sessionId;
    };

    typedef std::tr1::shared_ptr<MediaKeyEvent> MediaKeyEventSP;
    typedef std::tr1::shared_ptr<OtherEvent> OtherEventSP;

    std::list<MediaKeyEventSP> mMediaKeyEventQueue;
    std::list<OtherEventSP> mEventQueue;

public:

friend class TestListener;

public:
    void onStart() {
        if (ms3)
            ms3_url = LoadFromUrl(ms3_server_url);
        else
            bb_token = LoadFromUrl(bb_server_url);
    }

#define action_open_session	1 
#define action_close_session	2
#define action_provision	3
#define action_load_ms3		4
#define action_load_bb		5
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
            case action_load_ms3:
                onLoadMs3();
                return true;
            case action_load_bb:
                onLoadBb();
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
    std::string LoadFromUrl(std::string &url) {
        std::string data, command;
        FILE *fp;

        command = "wget ";
        command.append(url);
        command.append(" -O /data/marlin_temp");
        Log("run command '%s'", command.c_str());
        if (-1 == system(command.c_str())) {
            Log("command fails");
        } else
            Log("command success and return");

        fp = fopen("/data/marlin_temp", "r");
        if (fp == NULL) {
            Log("fail to open temp file for marlin");
            return data;
        }

        char line[1024];
        char *r;

        while ((r = fgets(line, 1024, fp)) != NULL) {
            if (!data.empty()) {
                 data.append(line);
            } else {
                data = line;
            }
        }

        Log("load from url %s:\n", url.c_str());
        Log("\n%s\n", data.c_str());
        //cout << data << endl;
        fclose(fp);

        return data;
    }

    void addOutput(std::string &str) {
        Log("%s\n", str.c_str());
    }

    void onPersonalize() {

        Log("Marlin doesn't need explict personalization , Wasabi do that automaticlly");
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

    void onLoadMs3() {

        if (ms3_url.empty()) {
            Log("MS3 url is NULL");
            return;
        }

        if (mediaKeys == NULL || session == NULL) {
            Log("Drm is not initialized");
            return;
        }

        std::string output = "";
        Message request;
        Message initData;

        std::string temp = "application/dash+xml";
        /* StringToMessage is utils function provided by YunOS DRM Core */
        StringToMessage(temp, initData);
        session->generateRequest((char*)"mime", initData);

        if (!waitKeyEvent(EventLicenseRequest, request)) {
            Log("session generate license requense fail");
            return;
        }

        /* DrmParam is utils Class provided by YunOS DRM Core */
        DrmParam req;
        req.setData(request.data(), request.size());
        req.setDataPosition(0);
        int version = req.readInt32();
        if (version != WASABI_DRM_PLUGIN_VERSION) {
            Log("Unexpected plugin version: %d", version);
            return;
        }
        int keyType = req.readInt32();
        Log("key type is %d", keyType);
        output += "C KeyType: ";
        output += "\n";

        int count = req.readInt32();
        if (count < 0) {
            return;
        }

        output += " name/value pairs:\n";
        bool gotMimeType = false;
        for (int i = 0; i < count; ++i) {
            std::string name;
            std::string value;
            CreateByteArrary(req, name);
            CreateByteArrary(req, value);

            output += "("+name+","+value+")\n";
            if (!gotMimeType && !strcmp(name.c_str(), "mime-type")) {
                if (strcmp(value.c_str(),"application/dash+xml")) {
                    Log("key request has bad mime type");
                    return;
                }
                gotMimeType = true;
            }
        }

        if (!gotMimeType) {
            Log("not got mime type");
            return;
        }

        Log("generate key request:\n");
        addOutput(output);

        DrmParam response;
        response.writeInt32(WASABI_DRM_PLUGIN_VERSION);
        response.writeInt32(keyType);
        response.writeInt32(2);
        temp = "ms3-url";
        response.writeByteArray(temp.length(), (const unsigned char*)temp.data());
        response.writeByteArray(ms3_url.length(), (const unsigned char*)ms3_url.data());
        temp = "content-id";
        response.writeByteArray(temp.length(), (const unsigned char*)temp.data());
        response.writeByteArray(cid.length(), (const unsigned char*)cid.data());
        Message licenseMsg;
        licenseMsg.assign((uint8_t*)response.data(), (uint8_t*)response.data() + response.dataSize());
        session->update(licenseMsg);

        if (!waitEvent(EventKeyStatusesChange)) {
            Log("session update license fail");
            return;
        }

        const MediaKeyStatusMap* statusMap = session->getKeyStatus();
        if (statusMap == NULL) {
            Log("there is nothing about key status");
            return;
        }

        KeyID key;
        /* StringToMessage is utils function provided by YunOS DRM Core */
        StringToMessage(cid, key);
        if (statusMap->has(key)) {
            Log("KeyID(%s) status is %d", cid.c_str(), statusMap->get(key));
        } else {
            ERROR("KeyID(%s) is not exist", cid.c_str());
        }

        Log("MS3 Load: succeeded\n");
        return;
    }

    void onLoadBb() {

        if (bb_token.empty()) {
            Log("No BB token!");
            return;
        }

        if (mediaKeys == NULL || session == NULL) {
            Log("Drm is not initialized");
            return;
        }

        std::string output = "";
        Message request;
        Message initData;

        std::string temp = "application/dash+xml";
        /* StringToMessage is utils function provided by YunOS DRM Core */
        StringToMessage(temp, initData);
        session->generateRequest((char*)"mime", initData);

        if (!waitKeyEvent(EventLicenseRequest, request)) {
            Log("session generate license requense fail");
            return;
        }

        DrmParam req;
        req.setData(request.data(), request.size());
        req.setDataPosition(0);
        int version = req.readInt32();
        if (version != WASABI_DRM_PLUGIN_VERSION) {
            Log("Unexpected plugin version: %d", version);
            return;
        }

        int keyType = req.readInt32();
        Log("keyType is %d", keyType);
        output += "C KeyType: ";
        output += "\n";
        int count = req.readInt32();
        if (count < 0) {
            Log("Unexpected name/value count: %d", count);
            return;
        }
        output += " name/value pairs:\n";
        bool gotMimeType = false;
        for (int i = 0; i < count; ++i) {
            std::string name;
            std::string value;
            CreateByteArrary(req, name);
            CreateByteArrary(req, value);

            output += "("+name+","+value+")\n";
            if (!gotMimeType && !strcmp(name.c_str(), "mime-type")) {
                if (strcmp(value.c_str(),"application/dash+xml")) {
                    Log("key request has bad mime type\n%s\n", output.c_str());
                    return;
                }
                gotMimeType = true;
            }
        }

        if (!gotMimeType) {
            Log("not got mime type");
            return;
        }

        Log("generate key request:\n");
        addOutput(output);

        DrmParam response;
        response.writeInt32(WASABI_DRM_PLUGIN_VERSION);
        response.writeInt32(keyType);
        response.writeInt32(2);
        temp = "bb-token";
        response.writeByteArray(temp.length(), (const unsigned char*)temp.data());
        response.writeByteArray(bb_token.length(), (const unsigned char*)bb_token.data());
        temp = "content-id";
        response.writeByteArray(temp.length(), (const unsigned char*)temp.data());
        response.writeByteArray(cid.length(), (const unsigned char*)cid.data());


        Message licenseMsg;
        licenseMsg.assign((uint8_t*)response.data(), (uint8_t*)response.data() + response.dataSize());
        session->update(licenseMsg);

        if (!waitEvent(EventKeyStatusesChange)) {
            Log("session update license fail");
            return;
        }

        const MediaKeyStatusMap* statusMap = session->getKeyStatus();
        if (statusMap == NULL) {
            Log("there is nothing about key status");
            return;
        }

        KeyID key;
        /* StringToMessage is utils function provided by YunOS DRM Core */
        StringToMessage(cid, key);
        if (statusMap->has(key)) {
            Log("KeyID(%s) status is %d", cid.c_str(), statusMap->get(key));
        } else {
            ERROR("KeyID(%s) is not exist", cid.c_str());
        }

        Log("BB token load: succeeded\n");
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

            if (ms3) {
                Log("delete session explicitly");
                delete session;
            } else
                Log("session is not deleted explicitly, it's done when destroy mediaKeys");

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

    ~TestListener() {}

    virtual void onMediaKeyMessage(MediaKeyMessageEvent type, const SessionID &sessionId, const Message &message) {
        if (!mOwner)
            return;

        DrmAutoLock lock(mOwner->mLock);
        DrmEngineTest::MediaKeyEventSP keyEvent(new DrmEngineTest::MediaKeyEvent(type, sessionId, message));
        mOwner->mMediaKeyEventQueue.push_back(keyEvent);
        mOwner->mCondition.signal();
    }

    virtual void onMessage(Event type, int param1, const SessionID &sessionId) {
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

DrmEngineTest::DrmEngineTest(bool is_ms3)
        : ms3(is_ms3),
          session(NULL),
          mCondition(mLock) {
    WASABI_DRM_PLUGIN_VERSION = 0x00010000;
    uint8_t systemName[] = MarlinID;

    for (size_t i = 0; i < sizeof(systemName); i++)
        MarlinUUID.append(1, systemName[i]);

    cid = "urn:marlin:kid:000102030405060708090a0b0c0d0e0f";
    ms3_server_url = "http://mobile-test.intertrust.com/cgi-bin/get-token?WWR-MS3";
    bb_server_url = "http://mobile-test.intertrust.com/cgi-bin/get-token?WWR-BB";
    mediaKeys = NULL;

    mListener = new TestListener(this);
}

void DrmEngineTest::onOpenSession() {
    if (session != NULL) {
        Log("Error: session is not null");
    } else {
        mediaKeys = MediaKeys::createMediaKeys(MarlinUUID.c_str());

        if (ms3)
            session = mediaKeys->createSession(SessionTypeTemporary);
        else
            session = mediaKeys->createSession(SessionTypePersistentLicense);

        if (session) {
            Log("Open session for Marlin: succeeded");
            session->setListener(mListener);
        }
    }
}

int main() {

    Log("*********************Marlin BB test is to be started*********************");
    DrmEngineTest *test = new DrmEngineTest();
    test->onStart();
    test->onAction(action_open_session);
    test->onAction(action_provision);
    test->onAction(action_load_bb);
    test->onAction(action_close_session);

    delete test;

    Log("\n\n*********************Marlin MS3 test is to be started*********************");
    usleep(1000000*2);
    test = new DrmEngineTest(true);

    test->onStart();
    test->onAction(action_open_session);
    test->onAction(action_provision);
    test->onAction(action_load_ms3);
    test->onAction(action_close_session);

    delete test;

    Log("drm engine test finish");
    return 0;
}
