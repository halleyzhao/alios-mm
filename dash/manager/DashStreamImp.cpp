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

#include "DashStreamImp.h"
#include "SegmentBuffer.h"

#include <multimedia/mm_debug.h>
#include <libdash.h>
#include <AdaptationSetHelper.h>

#include "DashUtils.h"

using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

using namespace dash::mpd;

namespace YUNOS_MM {

namespace YUNOS_DASH {

//#define VERBOSE WARNING

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

DEFINE_LOGTAG(DashStreamImp)

#define SEGMENT_BUFFER_SIZE 2

DashStreamImp::DashStreamImp(char* mpd, StreamType type)
    : mType(type),
      mMpd(NULL),
      mOwnMpd(true),
      mRepresentationSelected(false),
      mPeriod(NULL),
      mAdaptationSet(NULL),
      mRepresentation(NULL),
      mStartSegIdx(0) {

    ENTER1();

    mMpdFacotry = CreateDashManager();
    if (mMpdFacotry)
        mMpd = mMpdFacotry->Open(mpd);

    EXIT1();
}

DashStreamImp::DashStreamImp(dash::mpd::IMPD *mpd, StreamType type)
    : mType(type),
      mMpd(NULL),
      mOwnMpd(false),
      mRepresentationSelected(false),
      mPeriod(NULL),
      mAdaptationSet(NULL),
      mRepresentation(NULL) {

    ENTER1();

    mMpdFacotry = NULL;
    mMpd = mpd;

    EXIT1();
}

DashStreamImp::~DashStreamImp() {
    ENTER1();

    if (mMpd && mOwnMpd) {
        delete mMpd;
    }

    if (mMpdFacotry)
        mMpdFacotry->Delete();

    mSegmentBuffer.reset();

    EXIT1();
}

SegmentBuffer* DashStreamImp::createSegmentBuffer() {
    ENTER1();

    MMAutoLock lock(mLock);

    if (!mManager) {
        INFO("create dash manager for stream type %d", mType);
        mManager.reset(new DASHManager(SEGMENT_BUFFER_SIZE, this, mMpd));
    }

    if (!mSegmentBuffer)
        mSegmentBuffer.reset(SegmentBuffer::create(mManager));

    return mSegmentBuffer.get();
}

mm_status_t DashStreamImp::start() {
     ENTER1();

    MMAutoLock lock(mLock);
    if (!mMpd) {
        ERROR("mpd is not provided");
        EXIT_AND_RETURN1(MM_ERROR_NOT_INITED);
    }

    if (!mManager) {
        INFO("create dash manager for stream type %d", mType);
        mManager.reset(new DASHManager(SEGMENT_BUFFER_SIZE, this, mMpd));
    }

    if (!mRepresentationSelected) {
        INFO("representation is not selected when start");
        if (setRepresentation(0, 0, 0) != MM_ERROR_SUCCESS)
            WARNING("fail to select representation");
    }

    if (!mManager->Start())
        EXIT_AND_RETURN1(MM_ERROR_OP_FAILED);

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

void DashStreamImp::stop() {
    ENTER1();

    MMAutoLock lock(mLock);
    stop_l();

    EXIT1();
}

void DashStreamImp::stop_l() {
    ENTER1();

    if (mManager)
        mManager->Stop();

    EXIT1();
}

uint32_t DashStreamImp::getPosition() {
    ENTER1();

    MMAutoLock lock(mLock);
    if (mManager)
        return mManager->GetPosition();

    EXIT_AND_RETURN1(0xffffffff);
}

void DashStreamImp::setPosition(uint32_t index) {
    ENTER1();

    MMAutoLock lock(mLock);
    if (mManager)
        mManager->SetPosition(index);

    EXIT1();
}

void DashStreamImp::clear() {
    ENTER1();
    MMAutoLock lock(mLock);

    if (mManager)
        mManager->Clear();

    EXIT1();
}

void DashStreamImp::clearTail() {
    ENTER1();
    MMAutoLock lock(mLock);

    if (mManager)
        mManager->ClearTail();

    EXIT1();
}

mm_status_t DashStreamImp::seek(int64_t msec) {
    ENTER1();
    MMAutoLock lock(mLock);

    if (!mMpd) {
        ERROR("mpd is not provided");
        EXIT_AND_RETURN1(MM_ERROR_NOT_INITED);
    }

    int periodIndex = 0;
    int peroidStartMs = 0;
    DEBUG("seek to %" PRId64 "", msec);
    mm_status_t ret = DashUtils::getPeroidFromTimeMs(mMpd, periodIndex, peroidStartMs, msec);
    if (ret != MM_ERROR_SUCCESS) {
        EXIT_AND_RETURN1(ret);
    }
    DEBUG("period %d, period start %dms", periodIndex, peroidStartMs);
    IPeriod *period = mMpd->GetPeriods().at(periodIndex);
    IAdaptationSet* adaptationSet = NULL;
    IRepresentation* representation = NULL;
    ret = DashUtils::selectRepresentationFromPeroid(period,
                                                    mAdaptationSet,
                                                    mRepresentation,
                                                    adaptationSet,
                                                    representation);
    if (ret != MM_ERROR_SUCCESS) {
        EXIT_AND_RETURN1(ret);
    }
    int segmentNumber = 0;
    ret = DashUtils::getSegmentWithTimeMs(mMpd,
                                          period,
                                          adaptationSet,
                                          representation,
                                          peroidStartMs,
                                          msec,
                                          segmentNumber);
    if (ret != MM_ERROR_SUCCESS) {
        EXIT_AND_RETURN1(ret);
    }

    DEBUG("seek to segment idx %d", segmentNumber);
    mPeriod = period;
    mAdaptationSet = adaptationSet;
    mRepresentation = representation;
    mManager->SetRepresentation(mPeriod, mAdaptationSet, mRepresentation, true, segmentNumber);
    mStartSegIdx = segmentNumber;

    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
}

mm_status_t DashStreamImp::setRepresentation(int period, int adaptationSet, int representation, bool flush) {
    ENTER1();
    MMAutoLock lock(mLock);

    if (!mMpd) {
        ERROR("mpd is not provided");
        EXIT_AND_RETURN1(MM_ERROR_NOT_INITED);
    }

    IPeriod *curPeriod = mMpd->GetPeriods().at(period);

    std::vector<IAdaptationSet*> adaptationSets;

    if (mType == kStreamTypeVideo)
        adaptationSets = AdaptationSetHelper::GetVideoAdaptationSets(curPeriod);
    else if (mType == kStreamTypeAudio)
        adaptationSets = AdaptationSetHelper::GetAudioAdaptationSets(curPeriod);
    else {
        ERROR("stream type %d is not supported", mType);
        EXIT_AND_RETURN1(MM_ERROR_UNSUPPORTED);
    }

    IAdaptationSet* curAdaptationSet = adaptationSets.at(adaptationSet);

    IRepresentation* curRepresentation =
        curAdaptationSet->GetRepresentation().at(representation);

    if (!mManager) {
        INFO("create dash manager for stream type %d", mType);
        mManager.reset(new DASHManager(SEGMENT_BUFFER_SIZE, this, mMpd));
    }

    INFO("dash stream type is %d, selected mime type %s", mType, curAdaptationSet->GetMimeType().c_str());

    mRepresentationSelected = true;
    mPeriod = curPeriod;
    mAdaptationSet = curAdaptationSet;
    mRepresentation = curRepresentation;

    size_t size = DashUtils::getSegmentNumber(mMpd, period, curAdaptationSet, curRepresentation);
    INFO("type %d period %d adaptationSet %d representation %d has %d segments",
         mType, period, adaptationSet, representation, size);
    mManager->SetRepresentation(curPeriod, curAdaptationSet, curRepresentation, flush, 0, (int)size);
    //mStartSegIdx = 0;

    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
}

bool DashStreamImp::setStreamType(StreamType type) {
    ENTER1();
    MMAutoLock lock(mLock);

    if (mType == kStreamTypeUndefined)
        mType = type;
    else
        EXIT_AND_RETURN1(false);

    EXIT_AND_RETURN1(true);
}

bool DashStreamImp::getRepresentationInfo(MMParamSP param) {
    return DashUtils::getRepresentationInfo(mMpd, param);
}

mm_status_t DashStreamImp::getDuration(int64_t &durationMs) {
    ENTER1();

    dash::mpd::IMPD *mpd = mMpd;;

    if (!mpd) {
        ERROR("no media presentation description");
        EXIT_AND_RETURN1(MM_ERROR_NOT_INITED);
    }

    if (mpd->GetMediaPresentationDuration().empty()) {
        INFO("duration is unknonwn");
        EXIT_AND_RETURN1(MM_ERROR_UNSUPPORTED);
    }

    durationMs = (TimeResolver::GetDurationInSec(mpd->GetMediaPresentationDuration()) * 1000);

    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
}

bool DashStreamImp::hasMediaType(StreamType type, int period) {
    ENTER1();

    dash::mpd::IMPD *mpd = mMpd;;

    if (!mpd) {
        ERROR("no media presentation description");
        EXIT_AND_RETURN1(false);
    }

    std::vector<IAdaptationSet*> adaptationSets;

    if (period >= (int)mMpd->GetPeriods().size())
        EXIT_AND_RETURN1(false);

    IPeriod *curPeriod = mMpd->GetPeriods().at(period);
    if (!curPeriod)
        EXIT_AND_RETURN1(false);

    if (type == kStreamTypeVideo)
        adaptationSets = AdaptationSetHelper::GetVideoAdaptationSets(curPeriod);
    else if (type == kStreamTypeAudio)
        adaptationSets = AdaptationSetHelper::GetAudioAdaptationSets(curPeriod);
    else
        EXIT_AND_RETURN1(false);

    if (adaptationSets.empty())
        EXIT_AND_RETURN1(false);

    EXIT_AND_RETURN1(true);
}

void DashStreamImp::OnEndOfPeriod() {
    ENTER1();

    MMAutoLock lock(mLock);

    if (!mMpd || !mPeriod || !mAdaptationSet || !mRepresentation) {
        ERROR("mpd is %p, period is %p, adaptationSet is %p, represneation is %p",
            mMpd, mPeriod, mAdaptationSet, mRepresentation);
        stop_l();
        EXIT1();
    }

    int periodIdx = (int)DashUtils::getPeroidIndex(mMpd, mPeriod);
    if (periodIdx < 0) {
        stop_l();
        EXIT1();
    }

    if (periodIdx >= (int)(mMpd->GetPeriods().size() - 1)) {
        INFO("reach to end of entire media representation");
        stop_l();
        EXIT1();
    }

    IPeriod *period = mMpd->GetPeriods().at(++periodIdx);
    IAdaptationSet* adaptationSet = NULL;
    IRepresentation* representation = NULL;
    mm_status_t ret = DashUtils::selectRepresentationFromPeroid(period,
                                                    mAdaptationSet,
                                                    mRepresentation,
                                                    adaptationSet,
                                                    representation);
    if (ret != MM_ERROR_SUCCESS) {
        stop_l();
        EXIT1();
    }

    mPeriod = period;
    mAdaptationSet = adaptationSet;
    mRepresentation = representation;

    size_t size = DashUtils::getSegmentNumber(mMpd, periodIdx, adaptationSet, representation);
    INFO("media type %d, moving to period %p adaptationSet %p representation %p which has %d segments",
         mType, period, adaptationSet, representation, size);

    lock.unlock(); /* avoid download init segment with lock */
    mManager->SetRepresentation(mPeriod, mAdaptationSet, mRepresentation, false, 0, (int)size);
    lock.lock();
    mStartSegIdx = 0;

    EXIT();
}

mm_status_t DashStreamImp::getCurrentSegmentStart(int &period, int64_t &startUs, int64_t &durationUs) {
    ENTER1();

    MMAutoLock lock(mLock);

    if (!mMpd || !mPeriod) {
        ERROR("dash is not init, mpd %p period %p", mMpd, mPeriod);
        EXIT_AND_RETURN1(MM_ERROR_NOT_INITED);
    }

    size_t idx = DashUtils::getPeroidIndex(mMpd, mPeriod);
    if (idx < 0) {
        ERROR("invalid period index %d", (int)idx);
        EXIT_AND_RETURN1(MM_ERROR_SOURCE);
    }
    period = (int)idx;

    int msec, durationMs;

    mm_status_t status = DashUtils::getPeroidTimeRange(mMpd, period, msec, durationMs);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("invalid period time, status %d", status);
        EXIT_AND_RETURN1(status);
    }

    INFO("period start %d, segment idx %d", msec, mStartSegIdx);
    if (mStartSegIdx <= 0) {
        startUs = msec * 1000;
        durationUs = durationMs * 1000;
        EXIT_AND_RETURN1(status);
    }

    int segDurationMs;
    for (int i = 0; i < mStartSegIdx; i++) {
        status = DashUtils::getSegmentDuration(mMpd, period, mAdaptationSet, mRepresentation, i, segDurationMs);
        if (status != MM_ERROR_SUCCESS) {
            WARNING("fail to get segment duration");
            break;
        }
        msec += segDurationMs;
    }

    startUs = msec * 1000;
    durationUs = durationMs * 1000;
    EXIT_AND_RETURN(status);
}

} // end of namespace YUNOS_DASH

} // end of namespace YUNOS_MM
