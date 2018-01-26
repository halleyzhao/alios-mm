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

#ifndef __media_key_listener_h
#define __media_key_listener_h

namespace YUNOS_DRM {

/**
 * Application should inherit this class. It's suggested to instance one single listener
 * object for one MediaKeys object, multiple key sessions of the same MediaKeys object can
 * share this listener object. As session id is alway returned to signal which session the
 * callback is from.
 */
class DrmListener {
public:
    DrmListener(){}
    virtual ~DrmListener(){}

public:
    /**
     * Callback to notify event about media key, client should copy but not hold
     * reference to 'sessionId' and 'message' whose memory might be revoked
     * @param type: media key event type
     * @param sessionID: the session id associated with the event
     * @param message: the content of the event
     */
    virtual void onMediaKeyMessage(MediaKeyMessageEvent type, const SessionID &sessionId, const Message &message) = 0;

    /**
     * Callback to notify event, client should not hold reference to 'sessionId' whose
     * memory might be revoked
     * @param type: event type
     * @param param1:
     * @param sessionID: the session id associated with the event
     */
    virtual void onMessage(Event type, int param1, const SessionID &sessionId) = 0;
};

} // end of namespace YUNOS_DRM
#endif //__media_key_listener_h
