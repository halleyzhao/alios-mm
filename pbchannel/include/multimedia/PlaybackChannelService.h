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

#ifndef __PLAYBACK_CHANNEL_SERVICE_H__
#define __PLAYBACK_CHANNEL_SERVICE_H__

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>

namespace YUNOS_MM {
class PlaybackChannelService;
typedef MMSharedPtr<PlaybackChannelService> PlaybackChannelServiceSP;

class PlaybackChannelService {
public:
    static PlaybackChannelServiceSP publish();

public:
    virtual ~PlaybackChannelService(){}


protected:
    PlaybackChannelService(){}

protected:
    DECLARE_LOGTAG()
    MM_DISALLOW_COPY(PlaybackChannelService)
};


}

#endif //__PLAYBACK_CHANNEL_SERVICE_H__