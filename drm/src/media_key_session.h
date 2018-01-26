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

#ifndef __media_key_session_h
#define __media_key_session_h

#include "media_key_type.h"
#include "media_key_listener.h"

#include <string>
#include <map>
#include <tr1/memory>

namespace YUNOS_DRM {

class MediaKeyStatusMap;
class MediaKeySession;
class MediaKeys;
class MediaDecrypt;
class DrmEventLooper;

typedef std::tr1::shared_ptr<MediaKeyStatusMap> MediaKeyStatusMapSP;
typedef std::tr1::shared_ptr<DrmListener> DrmListenerSP;
typedef std::tr1::shared_ptr<MediaKeySession> MediaKeySessionSP;

// TODO add lock to protect map and others
class MediaKeySession {

public:
    virtual ~MediaKeySession();

    /**
     * Return the session ID of MediaKeySession
     */
    const SessionID& getSessionId() const;

    /**
     * Get the key system name of the session
     */
    const char* getName() const;

    /**
     * Generate a license request based on the initData. If the process
     * success, callback will return a message of type EventLicenseRequest
     * @param initDataType: a string that indicates the format of the accompanying
     * Initialization Data
     * @param initData: container-specific data that is used to generate a license
     * request. It must only contain information related to the keys required to play
     * a given set of stream(s) or media data. It must not contain application data,
     * client-specific data, user-specific data, or executable code
     */
    virtual void generateRequest(char* initDataType, std::vector<uint8_t> &initData) = 0;

    /**
     * Provide messages to underlying drm module. Messages provided by this call will
     * be saved as session data.
     * @param response: a message provided to drm module. The contents are key-system
     * specific. It must not contain executable code.
     */
    virtual void update(Message &response) = 0;

    /**
     * Load the data stored for the specified session
     */
    virtual bool load(const SessionID &sessionId) = 0;

    /**
     * Indicate that the application no longer needs the session and any resources associated
     * with the session should be released. Persisted data should not be released or cleared
     */
    virtual void close() = 0;

    /**
     * Remove stored session data associated with this object
     */
    virtual void remove() = 0;

    /**
     * Return key status map, maybe we should use forEach() for key status query
     */
    const MediaKeyStatusMap* getKeyStatus() { return mKeyMapSP.get(); }

    /*
     * Return the session expiration time in Mill Second
     */
    virtual int64_t expiration() { return mExpiration; }

    /**
     * Set session callback
     */
    bool setListener(DrmListener *listener);

    /**
      * Create or return an existed media decrypt object for the session
      */
    virtual MediaDecrypt* getMediaDecrypt();

    virtual void startProvisioning() {}

    //friend class MediaDecrypt;
    friend class DrmEventLooper;
    friend class MediaKeys;

protected:

    std::string mKeySystem;
    SessionID mSessionId;
    MediaKeySessionType mType;

    bool mClosed;
    bool mUninitialized;

    MediaKeyStatusMapSP mKeyMapSP;
    DrmListenerSP mListener;

    MediaKeys *mMediaKeys;

    MediaDecrypt* mMediaDecrypt;

    int64_t mExpiration;

    DrmEventLooper* getDrmEventLooper();

    virtual MediaDecrypt* createMediaDecryptImpl(MediaKeys*, std::vector<uint8_t>&) = 0;

    void mediaKeyEventNotify(MediaKeyMessageEvent event, const Message &message) {
        if (mListener)
            mListener->onMediaKeyMessage(event, mSessionId, message);
    }

    void notify(Event type, int param1) {
        if (mListener)
            mListener->onMessage(type, param1, mSessionId);
    }

    void *getCDM();
    bool setCDM(void *module);

    bool isCDMInitialized();

    MediaKeySession(MediaKeys*, MediaKeySessionType);

    void setKeyStatus(KeyID &keySetId, MediaKeyStatus status);

private:
    /**
     * Create meida key session from MediaKeys object, return NULL if not success
     * @param sessionType: session type
     */
    static MediaKeySession* create(MediaKeys *mediaKeys, MediaKeySessionType sessionType);

    DRM_DISALLOW_COPY(MediaKeySession);
};

class MediaKeyStatusMap {

public:
    MediaKeyStatusMap() {}
    virtual ~MediaKeyStatusMap() {}

    /**
     * Return media key status for the specified keyId
     */
    MediaKeyStatus get(KeyID &keyId) const;

    /**
     * Return true if the status of the key identified by keyId is known
     */
    bool has(KeyID &keyId) const;

    typedef void (*ForEachCallback)(KeyID, MediaKeyStatus);

    /**
     * Call callback once for each key-value pair present in the MediaKeyStatusMap
     */
    void forEach(ForEachCallback callback);

    //unsigned long size() { return mSize; }
    /**
     * The number of entries of the map, each entry for a key
     */
    unsigned long size() { return mKeyStatusMap.size(); }

friend class MediaKeySession;

private:
    //unsigned long mSize;
    void setKeyStatus(KeyID &keySetId, MediaKeyStatus status) { mKeyStatusMap[keySetId] = status; }
    std::map<KeyID, MediaKeyStatus> mKeyStatusMap;

    DRM_DISALLOW_COPY(MediaKeyStatusMap);
};

} // end of namespace YUNOS_DRM
#endif //__media_key_session_h
