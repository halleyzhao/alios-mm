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

#ifndef __PLAYBACK_CHANNEL_H__
#define __PLAYBACK_CHANNEL_H__

#include <map>
#include <string>
#include "multimedia/mm_types.h"
#include "multimedia/mm_errors.h"
#include "multimedia/mmlistener.h"
#include "multimedia/media_meta.h"
#include "multimedia/PlaybackChannelInterface.h"
#include "multimedia/PlaybackEvent.h"

namespace YUNOS_MM {

class PlaybackChannel;
typedef MMSharedPtr<PlaybackChannel> PlaybackChannelSP;

class PlaybackChannel : public PlaybackChannelInterface {

public:
    class Listener : public MMListener {
        public:
            Listener(){}
            virtual ~Listener(){}

            enum MsgType {
                /**
                 * onPlay callback
                 * params: none.
                 */
                kMsgPlay = 0,
                /**
                 * onPause callback
                 * params: none.
                 */
                kMsgPause = 1,
                /**
                 * onStop callback
                 * params: none.
                 */
                kMsgStop = 2,
                /**
                 * onSkipToNext callback
                 * params: none.
                 */
                kMsgNext = 3,
                /**
                 * onSkipToPrevious callback
                 * params: none.
                 */
                kMsgPrevious = 4,
                /**
                 * OnFastForward callback
                 * params: none.
                 */
                kMsgFastForward = 5,
                /**
                 * OnRewind callback
                 * params: none.
                 */
                kMsgRewind = 6,
                /**
                 * OnSeekTo callback
                 * params:
                 *     param1: seek pos.
                 */
                kMsgSeekTo = 7,
                /**
                 * onPlaybackEvent callback
                 * params:
                 *      param1: MediaButton key.
                 */
                kMsgMediaButton = 8,
                /**
                 * onError callback
                 * params:
                 *      param1: error code.
                 */
                kMsgError = 9
            };
    };

public:
    static PlaybackChannelSP create(const char *appTag, const char* packageName);
    virtual ~PlaybackChannel() {}

public:
    virtual mm_status_t setListener(Listener * listener) = 0;
    virtual bool isEnabled() = 0;
    virtual void dump() = 0;
    static bool releaseChannelFunc(PlaybackChannel *channel);

protected:
    PlaybackChannel() {}

private:
    MM_DISALLOW_COPY(PlaybackChannel)
    DECLARE_LOGTAG()
};


}

#endif /* __PLAYBACK_CHANNEL_H__ */

