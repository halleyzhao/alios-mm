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

#include "media_key_event.h"
#include "media_key_session.h"
#include "media_keys.h"

namespace YUNOS_DRM {

DrmEventLooper::DrmEventLooper(MediaKeys *mediaKeys)
    : mMediaKeys(mediaKeys) {

    if (mediaKeys) {
        std::string name = "Drm Event Looper: ";
        name.append(mediaKeys->getName());
        setName(name.c_str());
    }
}

DrmEventLooper::~DrmEventLooper() {

    if (!mEventQueue.empty()) {
        WARNING("event queue is not empty");
    }

    if (!mMediaKeyEventQueue.empty()) {
        WARNING("MediaKey event queue is not empty");
    }
}

bool DrmEventLooper::postMediaKeyEvent(MediaKeyMessageEvent type,
               const SessionID &sessionId,
               const Message &message) {

    DrmAutoLock lock(mLock);

    if (!mContinue)
        return false;

    MediaKeyEventSP event(new MediaKeyEvent(type, sessionId, message));

    mMediaKeyEventQueue.push_back(event);
    mCondition.signal();

    return true;
}

bool DrmEventLooper::postEvent(Event type,
               int param1,
               const SessionID &sessionId) {

    DrmAutoLock lock(mLock);

    if (!mContinue)
        return false;

    OtherEventSP event(new OtherEvent(type, param1, sessionId));

    mEventQueue.push_back(event);
    mCondition.signal();

    return true;
}

void DrmEventLooper::threadLoop() {

    MediaKeyEventSP keyEvent;
    OtherEventSP event;
    MediaKeySession *session = NULL;

event_looper:

    if (mMediaKeys && mContinue) {
        DrmAutoLock lock(mLock);

        if (mMediaKeyEventQueue.empty() && mEventQueue.empty()) {
            mCondition.wait();
            goto event_looper;
        }

        /* Event takes priority than MediaKeyEvent */
        if (mEventQueue.empty()) {
            keyEvent = mMediaKeyEventQueue.front();
            mMediaKeyEventQueue.pop_front();
        } else {
            event = mEventQueue.front();
            mEventQueue.pop_front();
        }
    } else {
        DrmAutoLock lock(mLock);
        INFO("MediaKey object(%p), mContinue(%d): clear all events", mMediaKeys, mContinue);
        mMediaKeyEventQueue.clear();
        mEventQueue.clear();
    }

    if (event && mMediaKeys && (session = mMediaKeys->getSession(event->sessionId))) {
        DEBUG("drm event looper post event(%d) %s: %d", event->type, eventToString(event->type), event->param1);
        session->notify(event->type,
                        event->param1);
    } else if (event) {
        WARNING("cannot handle event %d for session %p",
            event->type, event->sessionId.data());
    }

    bool sessionValid = false;
    if (keyEvent)
        sessionValid = (session = mMediaKeys->getSession(keyEvent->sessionId));

    if (keyEvent && mMediaKeys && sessionValid) {
        if (keyEvent->type == EventIndividualizationRequest && keyEvent->message.empty()) {
            INFO("possibly callback from plugin where is not good place to call provision, so call it here");
            session->startProvisioning();
        } else {
            DEBUG("session %p, key event(%d) %s", session, keyEvent->type, keyEventToString(keyEvent->type));
            session->mediaKeyEventNotify(keyEvent->type,
                                     keyEvent->message);
        }
    } else if (keyEvent) {
        WARNING("cannot handle mediakey event %d for session %p",
            keyEvent->type, keyEvent->sessionId.data());
    }

    keyEvent.reset();
    event.reset();

    if (mContinue)
        goto event_looper;

    mMediaKeyEventQueue.clear();
    mEventQueue.clear();

    INFO("Leaving Drm Event Looper");
}

} // end of namespace YUNOS_DRM
