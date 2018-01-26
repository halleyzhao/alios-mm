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

#ifndef __PLAYBACK_RECORD_LIST_H__
#define __PLAYBACK_RECORD_LIST_H__

#include <string>
#include <list>
#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>

#include "PlaybackRecord.h"
#include <multimedia/PlaybackState.h>

namespace YUNOS_MM {


class PlaybackRecordList;
typedef MMSharedPtr<PlaybackRecordList> PlaybackRecordListSP;

class PlaybackRecord;
class PlaybackState;
class PlaybackRecordList {
public:
    PlaybackRecordList();
    ~PlaybackRecordList();

public:
    mm_status_t addChannel(PlaybackRecord* record);
    mm_status_t removeChannel(PlaybackRecord* record);

    std::list<PlaybackRecord*>& getEnabledChannels();
    PlaybackRecord* getDefaultChannel(bool isEnabled);
    std::string dump(const char*prefix = "");
    mm_status_t updateGlobalPriorityChannel(PlaybackRecord* record);
    bool isGlobalPriorityEnabled();
    PlaybackRecord* globalPriorityChannel();

private:
    void getFilterCollections(int32_t flags,
                                std::list<PlaybackRecord*> &result);
private:
    std::list<PlaybackRecord*> mChannels;
    std::list<PlaybackRecord*> mEnabledList;
    PlaybackRecord* mGlobalPriorityChannel = NULL;

private:
    MM_DISALLOW_COPY(PlaybackRecordList)
    DECLARE_LOGTAG()
};


}

#endif //__PLAYBACK_RECORD_LIST_H__
