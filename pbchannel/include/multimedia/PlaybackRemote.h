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

#ifndef __PLAYBACK_REMOTE_H__
#define __PLAYBACK_REMOTE_H__

#include <map>
#include <string>
#include "multimedia/mm_types.h"
#include "multimedia/mm_errors.h"
#include "multimedia/mmlistener.h"
#include "multimedia/media_meta.h"
#include "multimedia/PlaybackRemoteInterface.h"


namespace YUNOS_MM {


class PlaybackRemote;
typedef MMSharedPtr<PlaybackRemote> PlaybackRemoteSP;

class PlaybackRemote : public PlaybackRemoteInterface {
public:
    class Listener : public MMListener {
        public:
            Listener(){}
            virtual ~Listener(){}

            enum MsgType {
                /**
                 * onChannelDestroyed callback
                 * params: none.
                 */
                kMsgChannelDestroyed = 0,
                /**
                 * onChannelEvent callback
                 * params:
                 *      param1: event
                 */
                kMsgChannelEvent = 1,
                /**
                 * onPlaybackStateChanged callback
                 * params:
                 *      param1: new state
                 */
                kMsgPlaybackStateChanged = 2,
                /**
                 * onMetadataChanged callback
                 * params:
                 *      param1: new meta
                 */
                kMsgMetadataChanged = 3,
                /**
                 * onQueueChanged callback
                 * params:
                 *     param1: new playlist
                 */
                kMsgPlaylistChanged = 4,
                /**
                 * onQueueTitleChanged callback
                 * params:
                 *      param1: new playlist title
                 */
                kMsgPlaylistTitleChanged = 5,
                /**
                 * onError callback
                 * params:
                 *     param1: error code.
                 */
                kMsgError = 6
            };
    };
public:
    virtual ~PlaybackRemote() {}

    /**
     * Note: this remoteId is got from PlaybackChannel::getPlaybackRemoteId method,
     * with this id so PlaybackRemote proxy can communicate with its adaptor.
     */
    static PlaybackRemoteSP create(const char*remoteId);
    virtual mm_status_t addListener(Listener * listener) = 0;
    // Listener should be added to list before
    // NULL pointer will remove all the listeners
    virtual void removeListener(Listener * listener)  = 0;
    virtual void dump() = 0;

protected:
    PlaybackRemote() {}

private:
    MM_DISALLOW_COPY(PlaybackRemote)
    DECLARE_LOGTAG()
};

}//__PLAYBACK_REMOTE_H__

#endif /* __PlaybackRemote_h__ */

