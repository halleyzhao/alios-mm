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
#include "media_key_event.h"

namespace YUNOS_DRM {

/*static*/ MediaKeys* MediaKeys::createMediaKeys(const char *keySystem) {

    MediaKeys *mediaKeys = new MediaKeys();

    mediaKeys->mKeySystem = keySystem;

    mediaKeys->mEventLooper.reset(new DrmEventLooper(mediaKeys));
    mediaKeys->mEventLooper->setName("DrmEventLooper");

    return mediaKeys;
}

/*static*/ bool MediaKeys::isTypeSupported(const char *keySystem, const char *contentType) {

    // TODO call factory function
    return true;
}

MediaKeys::MediaKeys()
    : mCDM(NULL),
      mIsCDMInitialized(false) {
}

MediaKeys::~MediaKeys() {

    int size = mSessions.size();

    INFO("there %d sessions, destroy them", size);

    while (1) {
        MediaKeySession* session = NULL;
        {
            DrmAutoLock lock(mLock);
            if (mSessions.empty())
                break;

            session = mSessions.front();
            INFO("MediaKeys object(%p) remove session(%p)", this, session);
            mSessions.pop_front();
        }

        if (session && !session->mClosed) {
            WARNING("closed alive session when MediaKeys obj destroy");
            session->remove();
            session->close();
        }
        delete session;
    }

    mEventLooper->stop();
}

const char* MediaKeys::getName() const {

    return mKeySystem.c_str();
}

MediaKeySession* MediaKeys::createSession(MediaKeySessionType sessionType) {

    {
        DrmAutoLock lock(mLock);
        if (mSessions.empty()) {
            INFO("create first session, start event looper");
            mEventLooper->start();
        }
    }

    MediaKeySession* session = MediaKeySession::create(this, sessionType);

    DrmAutoLock lock(mLock);
    mSessions.push_back(session);
    return session;
}

bool MediaKeys::setServerCertificate(Message &buffer) {

    mCertificate = buffer;
    return true;
}

MediaKeySession* MediaKeys::getSession(const SessionID &sessionId) {
    MediaKeySession* s;
    if (sessionId.size() != sizeof(s)) {
        WARNING("invalid session id, size is %d", sessionId.size());
        return NULL;
    }

    s = *(MediaKeySession**)sessionId.data();

    DrmAutoLock lock(mLock);
    std::list<MediaKeySession*>::iterator it = mSessions.begin();

    while (it != mSessions.end()) {
        if ((*it) == s)
            return s;
        it++;
    }

    WARNING("session does not exist %p", s);

    return NULL;
}

bool MediaKeys::setCDM(void *module) {

    DrmAutoLock lock(mLock);

    if (isCDMInitialized()) {
        INFO("CDM is already initialized, ignore this one");
        return false;
    }

    mCDM = module;
    mIsCDMInitialized = true;

    return true;
}

} // end of namespace YUNOS_DRM
