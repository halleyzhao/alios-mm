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

#ifndef __PLAYBACK_STATE_H__
#define __PLAYBACK_STATE_H__
#include "pointer/BasePtr.h"
#include "pointer/SharedPtr.h"
#include <dbus/DMessage.h>

#include <string>
#include "multimedia/mm_types.h"
#include "multimedia/mm_errors.h"
#include <multimedia/mm_cpp_utils.h>


namespace YUNOS_MM {
class MMParam;
class PlaybackState;
typedef MMSharedPtr<PlaybackState> PlaybackStateSP;

class PlaybackState {
public:
    static const int32_t PLAYBACK_POSITION_UNDEFINED = -1;
    enum State
    {
        kNone = 0,
        kBuffering = 1,
        kPlaying = 2,
        kPaused = 3,
        kStopped = 4,
        kFastForwarding = 5,
        kRewinding = 6,
        kPrevious = 7,
        kNext = 8,
        kError = 9,
    };

    enum Capabilities
    {
        kCapStop = 1 << 0,
        kCapPause = 1 << 1,
        kCapPlay = 1 << 2,
        kCapRewind = 1 << 3,
        kCapPrevious = 1 << 4,
        kCapNext = 1 << 5,
        kCapFastForward = 1 << 6,
        kCapSeekTo = 1 << 7,
        kCapPlayPause = 1 << 8,
    };

public:
    static PlaybackStateSP create();
public:
    /*pos in ms*/
    mm_status_t setState(State state, int64_t pos, float speed, int64_t updateTime = -1);
    int64_t getPosition() {return mPosition;}
    float getSpeed() {return mSpeed;}
    State getState() {return mState;}

    int64_t getBufferPosition() {return mBufferedPosition;}
    mm_status_t setBufferPosition(int64_t pos) { mBufferedPosition = pos; return MM_ERROR_SUCCESS; }

    Capabilities getCapabilities() {return mCapabilitiess;}
    mm_status_t setCapabilities(Capabilities capabilities) {mCapabilitiess = capabilities; return MM_ERROR_SUCCESS;}

    int64_t getUpdateTime() {return mUpdateTime;}
    mm_status_t setUpdateTime(int64_t time) {mUpdateTime = time; return MM_ERROR_SUCCESS;}

    std::string dump(const char *prefix = "");

    bool readFromMMParam(MMParam* param);
    bool writeToMMParam(MMParam* param);

public:
    static bool isActiveState(State state);
    static std::string getStateStr(State state);
    static int64_t getNowUs();
    // for ipc
public:
#if defined(__MM_YUNOS_CNTRHAL_BUILD__) || defined(__MM_YUNOS_YUNHAL_BUILD__)
    //ipc methods
    bool writeToMsg(yunos::SharedPtr<yunos::DMessage> &msg);
    bool readFromMsg(const yunos::SharedPtr<yunos::DMessage> &msg);
#endif

    PlaybackState();

    ~PlaybackState();

private:
    State mState;
    int64_t mPosition;
    float mSpeed;
    int64_t mUpdateTime;
    int64_t mBufferedPosition;
    Capabilities mCapabilitiess;


    // MM_DISALLOW_COPY(PlaybackState)
    DECLARE_LOGTAG()
};

}

#endif /* __PLAYBACK_STATE_H__ */

