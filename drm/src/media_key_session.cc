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

#include "media_key_session.h"
#include "media_keys.h"
#include "media_key_factory.h"

namespace YUNOS_DRM {

#define ENTER() INFO(">>>\n")
#define EXIT() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

/*static*/ MediaKeySession* MediaKeySession::create(MediaKeys *mediaKeys, MediaKeySessionType sessionType) {
    ENTER();
#if 0
    if (!mediaKeys->isCDMInitialized()) {
        INFO("Initialize CDM");

        mediaKeys->setCDM(DrmSessionFactory::loadDrmModule(mediaKeys));
    }
#endif

    MediaKeySession* session = DrmSessionFactory::createSession(mediaKeys, sessionType);

    if (!session) {
        ERROR("fail to create media key session");
        return session;
    }
    //session->mSessionId.assign((uint8_t*)&session, (uint8_t*)&session + sizeof(session));

    INFO("contruct MediaKeySession %p with sessionID %p", session, *(void **)(session->mSessionId.data()));

    return session;
}

MediaKeySession::MediaKeySession(MediaKeys* mediaKeys, MediaKeySessionType type)
    : mType(type),
      mClosed(false),
      mUninitialized(true),
      mMediaKeys(mediaKeys),
      mMediaDecrypt(NULL),
      mExpiration(-1) {

    ENTER();
    if (mediaKeys)
        mKeySystem = mediaKeys->getName();

    void *tmp = this;
    mSessionId.assign((uint8_t*)&tmp, (uint8_t*)&tmp + sizeof(tmp));
    mKeyMapSP.reset(new MediaKeyStatusMap);
    EXIT();
}

MediaKeySession::~MediaKeySession() {
    ENTER();

    if (!mMediaKeys)
        return;

    DrmAutoLock lock(mMediaKeys->mLock);
    bool found = false;
    std::list<MediaKeySession*>::iterator it = mMediaKeys->mSessions.begin();
    for (; it != mMediaKeys->mSessions.end();) {
        if (this == (*it)) {
            INFO("MediaKeys object(%p) removes session(%p)", mMediaKeys, this);
            it = mMediaKeys->mSessions.erase(it);
            found = true;
            break;
        }
    }

    if (!found)
        WARNING("session is already removed from MediaKeys object");

    EXIT();
}

const SessionID& MediaKeySession::getSessionId() const {
    return mSessionId;
}

bool MediaKeySession::setListener(DrmListener *listener) {


    mListener.reset(listener);

    return true;
}

const char* MediaKeySession::getName() const {
    return mKeySystem.c_str();
}

MediaDecrypt* MediaKeySession::getMediaDecrypt() {

    if (mMediaDecrypt)
        return mMediaDecrypt;

    mMediaDecrypt = createMediaDecryptImpl(mMediaKeys, mSessionId);

    return mMediaDecrypt;
}

DrmEventLooper* MediaKeySession::getDrmEventLooper() {
    return (mMediaKeys == NULL) ? NULL : mMediaKeys->mEventLooper.get();
}

void* MediaKeySession::getCDM() {
    return mMediaKeys ? mMediaKeys->getCDM() : NULL;
}

bool MediaKeySession::setCDM(void *module) {

    if (!mMediaKeys) {
        ERROR("Cannot set CDM as MediaKeys object is NULL");
        return false;
    }

    return mMediaKeys->setCDM(module);
}


bool MediaKeySession::isCDMInitialized() {

    if (!mMediaKeys)
        return false;

    return mMediaKeys->isCDMInitialized();
}

void MediaKeySession::setKeyStatus(KeyID &keySetId, MediaKeyStatus status) {
    mKeyMapSP->setKeyStatus(keySetId, status);
}

MediaKeyStatus MediaKeyStatusMap::get(KeyID &keyId) const {

    std::map<KeyID, MediaKeyStatus>::const_iterator it;

    it = mKeyStatusMap.find(keyId);

    if (it == mKeyStatusMap.end())
        return MediaKeyUnknown;

    return it->second;
}

bool MediaKeyStatusMap::has(KeyID &keyId) const {

    std::map<KeyID, MediaKeyStatus>::const_iterator it;

    it = mKeyStatusMap.find(keyId);

    return !(it == mKeyStatusMap.end());
}

void MediaKeyStatusMap::forEach(ForEachCallback callback) {

    if (!callback)
        return;

    std::map<KeyID, MediaKeyStatus>::iterator it;

    it = mKeyStatusMap.begin();

    for (; it != mKeyStatusMap.end(); it++)
        callback(it->first, it->second);
}

} // end of namespace YUNOS_DRM
