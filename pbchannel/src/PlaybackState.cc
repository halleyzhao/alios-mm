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
#include <multimedia/mmparam.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_debug.h>
#include <multimedia/PlaybackState.h>

using namespace yunos;

namespace YUNOS_MM {

DEFINE_LOGTAG1(PlaybackState, [PC])

PlaybackState::PlaybackState() : mState(State::kNone)
                                   , mPosition(0)
                                   , mSpeed(1.0f)
                                   , mUpdateTime(0)
                                   , mBufferedPosition(0)
                                   , mCapabilitiess(Capabilities(0))
{
}


PlaybackState::~PlaybackState()
{
}

/*static*/ PlaybackStateSP PlaybackState::create()
{
    PlaybackStateSP state;
    state.reset(new PlaybackState);
    return state;
}

mm_status_t PlaybackState::setState(State state, int64_t pos, float speed, int64_t updateTime)
{
    mState = state;
    mPosition = pos;
    mSpeed = speed;
    if (updateTime == -1) {// cilent is not set updateTime
        mUpdateTime = getNowUs();
    } else {
        mUpdateTime = updateTime;
    }
    return MM_ERROR_SUCCESS;
}

bool PlaybackState::writeToMsg(SharedPtr<DMessage> &msg)
{
    msg->writeInt32((int32_t)mState);
    msg->writeInt64(mPosition);
    msg->writeDouble((double)mSpeed);
    msg->writeInt64(mUpdateTime);
    msg->writeInt64(mBufferedPosition);
    msg->writeInt64(mCapabilitiess);

    return true;
}

bool PlaybackState::readFromMsg(const SharedPtr<DMessage> &msg)
{
    mState = (State)msg->readInt32();
    mPosition = msg->readInt64();
    mSpeed = (float)msg->readDouble();
    mUpdateTime = msg->readInt64();
    mBufferedPosition = msg->readInt64();
    mCapabilitiess = (Capabilities)msg->readInt64();

    return true;
}

bool PlaybackState::readFromMMParam(MMParam* param)
{
    if (!param)
        return false;

    mState = (PlaybackState::State)param->readInt32();
    mPosition = param->readInt64();
    mSpeed = param->readFloat();
    mUpdateTime = param->readInt64();
    mBufferedPosition = param->readInt64();
    mCapabilitiess = (PlaybackState::Capabilities)param->readInt64();
    return true;
}


bool PlaybackState::writeToMMParam(MMParam* param)
{
    if (!param) {
        return false;
    }
    param->writeInt32((int32_t)mState);
    param->writeInt64(mPosition);
    param->writeFloat(mSpeed);
    param->writeInt64(mUpdateTime);
    param->writeInt64(mBufferedPosition);
    param->writeInt64((int64_t)mCapabilitiess);
    return true;
}

/*static*/bool PlaybackState::isActiveState(State state)
{
    switch (state) {
        case PlaybackState::State::kFastForwarding:
        case PlaybackState::State::kRewinding:
        case PlaybackState::State::kPrevious:
        case PlaybackState::State::kNext:
        case PlaybackState::State::kBuffering:
        case PlaybackState::State::kPlaying:
            return true;
        default:
            return false;
    }
    return false;
}

/*static*/ std::string PlaybackState::getStateStr(State state)
{
    switch (state) {
        case PlaybackState::State::kNone:
            return "kNone";
        case PlaybackState::State::kStopped:
            return "kStopped";
        case PlaybackState::State::kPaused:
            return "kPaused";
        case PlaybackState::State::kPlaying:
            return "kPlaying";
        case PlaybackState::State::kFastForwarding:
            return "kFastForwarding";
        case PlaybackState::State::kRewinding:
            return "kRewinding";
        case PlaybackState::State::kBuffering:
            return "kBuffering";
        case PlaybackState::State::kError:
            return "kError";
        case PlaybackState::State::kPrevious:
            return "kPrevious";
        case PlaybackState::State::kNext:
            return "kNext";
        default:
            return "unknow state";
    }
}

/*static*/ int64_t PlaybackState::getNowUs()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000LL + t.tv_nsec / 1000LL;
}

std::string PlaybackState::dump(const char *prefix)
{
    std::string info;
    char tmp[1024] = {0};

    int len = sprintf(tmp, "%s {mState: %s, ", prefix, getStateStr(mState).c_str());
    len += sprintf(tmp + len, "mPosition: %" PRId64 ", ", mPosition);
    len += sprintf(tmp + len, "mSpeed: %f, ", mSpeed);

    len += sprintf(tmp + len, "mUpdateTime: %" PRId64 ", ", mUpdateTime);
    len += sprintf(tmp + len, "mBufferedPosition: %" PRId64 ", ", mBufferedPosition);
    len += sprintf(tmp + len, "mCapabilitiess: 0x%0x }\n", mCapabilitiess);
    DEBUG("%s", tmp);

    info.append(tmp);
    return info;
}
}
