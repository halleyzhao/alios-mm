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
#include <multimedia/PlaybackEvent.h>

using namespace yunos;

namespace YUNOS_MM {

DEFINE_LOGTAG1(PlaybackEvent, [PC])
#define kStateRelased 0
#define kStatePressed 1
#define kStateRepeated 2
#define kStateLongPressed 3
#define kStateCancel 4

PlaybackEvent::PlaybackEvent()
{
}

PlaybackEvent::PlaybackEvent(uint32_t state, uint32_t key, uint32_t repeatCount, uint64_t downTime)
                    : mKey(key)
                    , mCode("null")
                    , mRepeatCount(repeatCount)
                    , mDownTime(downTime)
                    , mIsFromHost(true)
{
    mUsages = kFlagNone;
    if (state == kStatePressed ||
        state == kStateRepeated ||
        state == kStateLongPressed) {
        mType = kKeyDown;
        //if (state == kStateRepeated || state == kStateLongPressed) {
        //    mUsages |= kFlagLongPress;
        //}
    } else if (state == kStateRelased) {
        mType = kKeyUp;
    } else if (state == kStateCancel) {

    }
    if (repeatCount) {
        mUsages |= kFlagLongPress;
    }

}

PlaybackEvent::PlaybackEvent(PlaybackEventSP event)
{
    if (!event) {
        ASSERT(0);
        ERROR("param is invalid");
        return;
    }
    mType = event->getType();
    mKey = event->getKey();
    mCode = event->getCode();
    mUsages = event->getUsages();
    mRepeatCount = event->getRepeatCount();
    mDownTime = event->getDownTime();
    mIsFromHost = event->isFromHost();
}

PlaybackEvent::PlaybackEvent(uint32_t type, uint32_t key, const char* code, uint32_t repeatCount,
                        uint32_t flags, uint64_t downTime, bool isFromHost)
                  : mType(type)
                  , mKey(key)
                  , mCode(code == NULL ? "null" : code)
                  , mUsages(flags)
                  , mRepeatCount(repeatCount)
                  , mDownTime(downTime)
                  , mIsFromHost(isFromHost)
{

}

PlaybackEvent::PlaybackEvent(uint32_t type, uint32_t key, const char* code, uint32_t flags, uint64_t downTime)
                  : mType(type)
                  , mKey(key)
                  , mCode(code == NULL ? "null" : code)
                  , mUsages(flags)
                  , mRepeatCount(0)
                  , mDownTime(downTime)
                  , mIsFromHost(true)
{

}

PlaybackEvent::~PlaybackEvent()
{
}

/*static*/PlaybackEventSP PlaybackEvent::create()
{
    PlaybackEventSP event;
    event.reset(new PlaybackEvent());
    return event;
}

bool PlaybackEvent::writeToMsg(SharedPtr<DMessage> &msg)
{
    msg->writeInt32(mType);
    msg->writeInt32(mKey);
    msg->writeString(yunos::String(mCode.c_str()));
    msg->writeInt32(mUsages);
    msg->writeInt32(mRepeatCount);
    msg->writeInt64(mDownTime);
    msg->writeInt32((int32_t)mIsFromHost);

    return true;
}

bool PlaybackEvent::readFromMsg(const SharedPtr<DMessage> &msg)
{
    mType = msg->readInt32();
    mKey = msg->readInt32();
    mCode = msg->readString().c_str();
    mUsages = msg->readInt32();
    mRepeatCount = msg->readInt32();
    mDownTime = msg->readInt64();
    mIsFromHost = msg->readInt32() != 0;

    return true;
}

bool PlaybackEvent::readFromMMParam(MMParam* param)
{
    if (!param)
        return false;

    mType = param->readInt32();
    mKey = param->readInt32();
    mCode = param->readCString();
    mUsages = param->readInt32();
    mRepeatCount = param->readInt32();
    mDownTime = param->readInt64();
    mIsFromHost = param->readInt32() != 0;
    return true;
}


bool PlaybackEvent::writeToMMParam(MMParam* param)
{
    if (!param) {
        return false;
    }
    param->writeInt32(mType);
    param->writeInt32(mKey);
    param->writeCString(mCode.c_str());
    param->writeInt32(mUsages);
    param->writeInt32(mRepeatCount);
    param->writeInt64(mDownTime);
    param->writeInt32((int32_t)mIsFromHost);

    return true;
}

void PlaybackEvent::dump()
{
    DEBUG("{ mType: %d, mKey: %d, mCode: \"%s\", mUsages: 0x%0x, mRepeatCount: %d, mDownTime: %" PRId64 ", mIsFromHost: %d }",
        mType, mKey, mCode.c_str(), mUsages, mRepeatCount, mDownTime, mIsFromHost);
}


/*static*/bool PlaybackEvent::isMediaKey(uint32_t key)
{
    switch (key) {
        case kMediaNext:
        case kMediaPrevious:
        case kMediaPlayPause:
        case kMediaStop:
        case kMediaPlay:
        case kMediaPause:
        case kMediaFastForward:
        case kMediaRewind:
        case kHeadsetHook:
            return true;
        default:
            return false;
    }
    return false;
}

/*static*/bool PlaybackEvent::isVoiceKey(uint32_t key)
{
    if (key == kHeadsetHook) {
        return true;
    }
    return false;
}


}
