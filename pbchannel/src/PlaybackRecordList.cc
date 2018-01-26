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
#include <algorithm>

#include "PlaybackRecordList.h"
#include "PlaybackRecord.h"
#include <multimedia/PlaybackState.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_debug.h>
#include <multimedia/PlaybackChannel.h>

using namespace yunos;

namespace YUNOS_MM {

DEFINE_LOGTAG1(PlaybackRecordList, [PC])

PlaybackRecordList::PlaybackRecordList()
{
}


PlaybackRecordList::~PlaybackRecordList()
{
}


// all the methods run on PlaybackChannelServiceAdaptor loop thread
mm_status_t PlaybackRecordList::addChannel(PlaybackRecord* record)
{
    DEBUG("record %p, before add to list, list size %d", record, mChannels.size());
    mChannels.push_back(record);
    mEnabledList.clear();

    return MM_ERROR_SUCCESS;
}

mm_status_t PlaybackRecordList::removeChannel(PlaybackRecord* record)
{
    auto it = std::find(mChannels.begin(), mChannels.end(), record);
    if (it == mChannels.end()) {
        ERROR("unknown record %p", record);
        return MM_ERROR_INVALID_PARAM;
    }

    DEBUG("record %p, before erase list size %d", record, mChannels.size());
    mChannels.erase(it);

    if (record == mGlobalPriorityChannel) {
        mGlobalPriorityChannel = NULL;
    }
    mEnabledList.clear();
    return MM_ERROR_SUCCESS;
}

PlaybackRecord* PlaybackRecordList::getDefaultChannel(bool isEnabled)
{
    PlaybackRecord *record = NULL;

    if (mGlobalPriorityChannel && mGlobalPriorityChannel->isEnabled()) {
        VERBOSE("");
        return mGlobalPriorityChannel;
    }

    if (isEnabled) {
        std::list<PlaybackRecord*> channels = getEnabledChannels();
        if (channels.empty()) {
            DEBUG("no enabled channels");
            return NULL;
        }
        record = channels.front();
    } else {
        record = mChannels.front();
    }

    return record;
}

std::list<PlaybackRecord*>& PlaybackRecordList::getEnabledChannels()
{
    if (mEnabledList.empty()) {
        getFilterCollections(0, mEnabledList);
    }
    return mEnabledList;
}

// invoked by PlaybackChannel::setEnabled and PlaybackChannel::setUsages
mm_status_t PlaybackRecordList::updateGlobalPriorityChannel(PlaybackRecord* record)
{
    if ((record->getUsages() & PlaybackChannelInterface::Usages::kGlobalPriority) != 0 &&
        record != mGlobalPriorityChannel) {
        mGlobalPriorityChannel = record;
        DEBUG("got a global priority session");
    }
    mEnabledList.clear();
    return MM_ERROR_SUCCESS;
}

bool PlaybackRecordList::isGlobalPriorityEnabled()
{
    if (mGlobalPriorityChannel && mGlobalPriorityChannel->isEnabled()) {
        return true;
    }
    return false;
}

PlaybackRecord* PlaybackRecordList::globalPriorityChannel()
{
    return mGlobalPriorityChannel;
}

void PlaybackRecordList::getFilterCollections(int32_t usages,
                                                    std::list<PlaybackRecord*> &result)
{
    DEBUG("usages: %d", usages);
    auto enabledIndex = result.begin();
    auto disabledIndex = result.begin();

    for (auto it = mChannels.begin(); it != mChannels.end(); it++) {
        PlaybackRecord* record = *it;
        if ((record->getUsages() & usages) != usages) {
            // skip the wrong usages
            continue;
        }

        // skip disabled channel
        if (!record->isEnabled()) {
            continue;
        }

        if (record->getUsages() & PlaybackChannelInterface::Usages::kGlobalPriority) {
            result.push_front(record);
            enabledIndex++;
            disabledIndex++;
        } else if (record->isPlaybackActive()) {
            // In enabled channels, enable playback has high priority ahead of disactive ones
            // add enabled channel to header
            result.insert(enabledIndex, record);
            enabledIndex++;
            disabledIndex++;
        } else {
            // add channel whose channel is not avtive to tailer
            result.insert(disabledIndex, record);
            disabledIndex++;
        }
    }
}

std::string PlaybackRecordList::dump(const char*prefix)
{
    std::string info;
    std::string info1;
    char newPrefix[64] = {0};
    sprintf(newPrefix, "%s  ", prefix);
    char tmp1[64] = {0};

    char *tmp = new char[32*1024];
    if (!tmp) {
        return "nomem";
    }

    int len = sprintf(tmp, "%sPlaybackRecordList dump info:\n", prefix);
    len += sprintf(tmp + len, "%smChannels.size: %d\n", prefix, mChannels.size());
    getEnabledChannels();
    len += sprintf(tmp + len, "%smEnabledList.size: %d\n", prefix, mEnabledList.size());

    uint32_t i = 0;
    for (auto it = mChannels.begin(); it != mChannels.end(); it++) {
        memset(tmp1, 0, sizeof(tmp1));
        sprintf(tmp1, "\n%smChannels[%d] dump info:\n", prefix, i);
        info1.append(tmp1);
        info1.append((*it)->dump(newPrefix));
        i++;
    }

    info.append(tmp);
    info.append(info1);
    delete []tmp;
    tmp = NULL;
    return info;
}

}
