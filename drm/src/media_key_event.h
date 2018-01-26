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

#ifndef __media_key_event_h
#define __media_key_event_h

#include "media_key_utils.h"
#include "media_key_type.h"

#include <tr1/memory>

namespace YUNOS_DRM {

class MediaKeys;

class DrmEventLooper : public DrmLooper {
public:
    DrmEventLooper(MediaKeys *mediaKeys);
    ~DrmEventLooper();

    bool postMediaKeyEvent(MediaKeyMessageEvent type,
                   const SessionID &sessionId,
                   const Message &message);

    bool postEvent(Event type,
                   int param1,
                   const SessionID &sessionId);

#if 0
    bool sendMediaKeyEvent(MediaKeyMessageEvent type,
                   const SessionID &sessionId,
                   const &message);

    bool sendEvent(Event type,
                   int param1,
                   const SessionID &sessionId);
#endif

protected:
    virtual void threadLoop();

private:

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

    //SessionID mSessionID;
    MediaKeys *mMediaKeys;
    std::list<MediaKeyEventSP> mMediaKeyEventQueue;
    std::list<OtherEventSP> mEventQueue;

};

} // end of namespace YUNOS_DRM

#endif //__media_key_event_h
