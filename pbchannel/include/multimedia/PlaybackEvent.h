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

#ifndef __PLAYBACK_EVENT_H__
#define __PLAYBACK_EVENT_H__
#include "pointer/BasePtr.h"
#include "pointer/SharedPtr.h"
#include <dbus/DMessage.h>

#include <string>
#include "multimedia/mm_types.h"
#include "multimedia/mm_errors.h"
#include <multimedia/mm_cpp_utils.h>


namespace YUNOS_MM {
class PlaybackEvent;
typedef MMSharedPtr<PlaybackEvent> PlaybackEventSP;
class MMParam;

class PlaybackEvent {
public:
    enum Types
    {
        kKeyUp = 0,
        kKeyDown = 1
    };

    enum Usages
    {
        kFlagNone = 0,
        kFlagLongPress = 1 << 0
    };

    enum Keys
    {
        kMediaNext = 163,
        kMediaPlayPause = 164,
        kMediaPrevious = 165,
        kMediaStop = 166,
        kMediaRewind = 168,

        kMediaPlay = 200,
        kMediaPause = 201,
        kMediaFastForward = 208,

        kHeadsetHook = 226
    };

public:

    uint32_t getType() {return mType;}
    mm_status_t setType(uint32_t type) {mType = type; return MM_ERROR_SUCCESS;}

    uint32_t getKey() {return mKey;}
    mm_status_t setKey(uint32_t key) {mKey = key; return MM_ERROR_SUCCESS;}

    std::string getCode() {return mCode;}
    mm_status_t setCode(const char * code) {mCode = (code == NULL ? "null" : code); return MM_ERROR_SUCCESS;}

    uint32_t getUsages() {return mUsages;}
    mm_status_t setUsages(uint32_t flags) {mUsages = flags; return MM_ERROR_SUCCESS;}

    uint32_t getRepeatCount() { return mRepeatCount; }
    mm_status_t setRepeatCount(uint32_t count) { mRepeatCount = count; return MM_ERROR_SUCCESS; }

    uint64_t getDownTime() { return mDownTime; }
    mm_status_t setDownTime(uint64_t downTime) { mDownTime = downTime; return MM_ERROR_SUCCESS; }

    // focus window is host or container
    bool isFromHost() { return mIsFromHost; }

public:
    static bool isMediaKey(uint32_t key);
    static bool isVoiceKey(uint32_t key);
    void dump();
    static PlaybackEventSP create();
    // for ipc
public:
    //ipc methods
    bool writeToMsg(yunos::SharedPtr<yunos::DMessage> &msg);
    bool readFromMsg(const yunos::SharedPtr<yunos::DMessage> &msg);
    bool readFromMMParam(MMParam* param);
    bool writeToMMParam(MMParam* param);

public:
    PlaybackEvent(PlaybackEventSP event);
    PlaybackEvent();
    PlaybackEvent(uint32_t state, uint32_t key, uint32_t repeatCount, uint64_t downTime);
    PlaybackEvent(uint32_t type, uint32_t key, const char* code, uint32_t flags, uint64_t downTime);
    PlaybackEvent(uint32_t type, uint32_t key, const char* code, uint32_t repeatCount,
        uint32_t flags, uint64_t downTime, bool isFromHost = true);
    ~PlaybackEvent();

private:
    uint32_t mType;
    uint32_t mKey; //key value
    std::string mCode; //key description
    int32_t mUsages; //flags to indicate long press or short press
    int32_t mRepeatCount;
    uint64_t mDownTime;
    bool mIsFromHost;

    // MM_DISALLOW_COPY(PlaybackEvent)
    DECLARE_LOGTAG()
};

}

#endif /* __PLAYBACK_EVENT_H__ */

