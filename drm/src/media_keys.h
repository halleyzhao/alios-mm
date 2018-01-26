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

#ifndef __media_keys_h
#define __media_keys_h

#include "media_key_type.h"
#include "media_key_utils.h"

#include <string>
#include <tr1/memory>

namespace YUNOS_DRM {

// TODO refine log system
// TODO type to name functions for verbose log

// TODO To differetiate naming with EME API, change MediaKey to DrmScheme, change KeySystem to UUID?
// TODO MediaKeys object needs a lock to protect std::list<MediaKeySession*> mSessions;

class MediaKeySession;
class DrmEventLooper;
class DrmLock;

typedef std::tr1::shared_ptr<MediaKeySession> MediaKeySessionSP;
typedef std::tr1::shared_ptr<DrmEventLooper> DrmEventLooperSP;

class MediaKeys {

public:
    /**
     * Create a MediaKeys object, return NULL if not supported
     * @param keySystem: generic term for a decryption mechanism
     * and/or content protection provider
     */
    static MediaKeys *createMediaKeys(const char *keySystem);

    /**
     * Query if given key system is supported, and whether the drm
     * plugin can handle the media container format
     * @param keySystem: the name of the key system
     * @param contentType: the format of the media container, e.g. represent by
     * MIME type such as "video/mp4" or "video/webm"
     */
    static bool isTypeSupported(const char *keySystem, const char *contentType);

    /**
     * Get the key system name of this object
     */
    const char* getName() const;

    /**
     * Create media key session object with the specified session type
     */
    MediaKeySession* createSession(MediaKeySessionType sessionType = SessionTypeTemporary);

    /**
     * Provides a server certificate to encrypt messages to license server
     * @param buffer: the server certificate, the contents are key system specific
     */
    bool setServerCertificate(Message &buffer);

    /**
     * Close and clean up all sessions
     */
    virtual ~MediaKeys();

    /**
     * Return MediaKeySession object identified by session id
     */
    virtual MediaKeySession* getSession(const SessionID &sessionId);

friend class MediaKeySession;

private:
    std::string mKeySystem;
    std::list<MediaKeySession*> mSessions;
    Message mCertificate;

    DrmLock mLock;

    DrmEventLooperSP mEventLooper;

    MediaKeys();

    // TODO move mCDM and its funcs into MediaKeySession as static member
    // maybe need an arrary of mCDM, each entry for one drm system.
#if 1
    /**
     * pointer to underlying DRM module
     */
    void* mCDM;
    bool mIsCDMInitialized;
    void* getCDM() { return mCDM; }

    /**
     * Check if underlying Content Decryption Module is ready
     */
    bool isCDMInitialized() { return mIsCDMInitialized; }

    bool setCDM(void *module);
#endif

private:
    MediaKeys(const MediaKeys&);
    MediaKeys & operator=(const MediaKeys&);
};

typedef std::tr1::shared_ptr<MediaKeys> MediaKeysSP;

} // end of namespace YUNOS_DRM
#endif //__media_keys_h
