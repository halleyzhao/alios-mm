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

#include "DashUtils.h"
#include "DashStream.h"

#include <multimedia/mm_debug.h>
#include <libdash.h>
#include <AdaptationSetHelper.h>
#include <DASHManager.h>

#include <string>

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

DEFINE_LOGTAG(DashUtils)

#define INVALID_TIMESTAMPT (-0xffffffff)

#define CHECK_MPD(p)                                  \
    if (!p) {                                         \
        ERROR("mpd is null");                         \
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);      \
    }

/* static */
DashUtils::RepresentationType DashUtils::getRepresentationType(IPeriod *period,
                                                    IAdaptationSet* adaptationSet,
                                                    IRepresentation* representation,
                                                    ISegmentBase **base) {

    if (!period || !adaptationSet || !representation)
        return kSegmentUndefined;

    /* check on Representation Level */
    if (representation->GetSegmentList()) {
        if (base)
            *base = representation->GetSegmentList();
        return kSegmentList;
    }

    if (representation->GetSegmentTemplate()) {
        if (base)
            *base = representation->GetSegmentTemplate();
        return kSegmentTemplate;
    }

    if (representation->GetSegmentBase() || representation->GetBaseURLs().size() > 0) {
        if (base)
            *base = representation->GetSegmentBase();
        return kSingleSegment;
    }

    /* check on AdaptationSet Level */
    if (adaptationSet->GetSegmentList()) {
        if (base)
            *base = adaptationSet->GetSegmentList();
        return kSegmentList;
    }

    if (adaptationSet->GetSegmentTemplate()) {
        if (base) {
            *base = adaptationSet->GetSegmentTemplate();
        }
        return kSegmentTemplate;
    }

    if (adaptationSet->GetSegmentBase()) {
        if (base)
            *base = adaptationSet->GetSegmentBase();
        return kSingleSegment;
    }

    /* check on Period Level */
    if (period->GetSegmentList()) {
        if (base)
            *base = period->GetSegmentList();
        return kSegmentList;
    }

    if (period->GetSegmentTemplate()) {
        if (base)
            *base = period->GetSegmentTemplate();
        return kSegmentTemplate;
    }

    if (period->GetSegmentBase()) {
        if (base)
            *base = period->GetSegmentBase();
        return kSingleSegment;
    }

    return kSegmentUndefined;
}

#define ASSIGN_IF_POSITIVE(a, b)                \
    if (b > 0)                                  \
        a = b;                                  \

mm_status_t DashUtils::getSegmentDuration(dash::mpd::IMPD *mpd,
                               int periodIndex,
                               IAdaptationSet* adaptationSet,
                               IRepresentation* representation,
                               int index,
                               int &durationMs) {

    CHECK_MPD(mpd);

    IPeriod* period = mpd->GetPeriods().at(periodIndex);

    if (!period || !adaptationSet || !representation || index < 0)
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);

    //if (index >= (int)adaptationSet->GetRepresentation().size())
        //EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);

    RepresentationType type = getRepresentationType(period, adaptationSet, representation);

    if (type == kSegmentUndefined)
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);
    else if (type == kSingleSegment) { /* period duration */
        int msec;
        if (index != 0)
            EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);
        mm_status_t status = getPeroidTimeRange(mpd, periodIndex, msec, durationMs);
        EXIT_AND_RETURN(status);
    } else {
        IMultipleSegmentBase* multipleSegment;
        uint32_t timescale = 0;
        uint32_t duration = 0;

        ISegmentBase* segmentBase = period->GetSegmentBase();
        if (segmentBase)
            ASSIGN_IF_POSITIVE(timescale, segmentBase->GetTimescale());

        if (type == kSegmentList) {
            multipleSegment = period->GetSegmentList();
            if (multipleSegment) {
                ASSIGN_IF_POSITIVE(duration, multipleSegment->GetDuration());
                ASSIGN_IF_POSITIVE(timescale, multipleSegment->GetTimescale());
            }

            multipleSegment = adaptationSet->GetSegmentList();
            if (multipleSegment) {
                ASSIGN_IF_POSITIVE(duration, multipleSegment->GetDuration());
                ASSIGN_IF_POSITIVE(timescale, multipleSegment->GetTimescale());
            }

            multipleSegment = representation->GetSegmentList();
            if (multipleSegment) {
                ASSIGN_IF_POSITIVE(duration, multipleSegment->GetDuration());
                ASSIGN_IF_POSITIVE(timescale, multipleSegment->GetTimescale());
            }
        } else { /* type == kSegmentTemplate */
            multipleSegment = period->GetSegmentTemplate();
            if (multipleSegment) {
                ASSIGN_IF_POSITIVE(duration, multipleSegment->GetDuration());
                ASSIGN_IF_POSITIVE(timescale, multipleSegment->GetTimescale());
            }

            multipleSegment = adaptationSet->GetSegmentTemplate();
            if (multipleSegment) {
                ASSIGN_IF_POSITIVE(duration, multipleSegment->GetDuration());
                ASSIGN_IF_POSITIVE(timescale, multipleSegment->GetTimescale());
            }

            multipleSegment = representation->GetSegmentTemplate();
            if (multipleSegment) {
                ASSIGN_IF_POSITIVE(duration, multipleSegment->GetDuration());
                ASSIGN_IF_POSITIVE(timescale, multipleSegment->GetTimescale());
            }
        }
        if (!duration || !timescale)
            EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);


        durationMs = (int) (duration * 1000 / timescale);

        int msec, periodDuration;
        mm_status_t status = getPeroidTimeRange(mpd, periodIndex, msec, periodDuration);
        if (status != MM_ERROR_SUCCESS)
            EXIT_AND_RETURN1(status);

        int segmentNum = ceil(1.0f * periodDuration / durationMs);

        if (index >= segmentNum)
            EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);
        else if (index < segmentNum - 1)
            EXIT_AND_RETURN(MM_ERROR_SUCCESS);
        else /* last segment of multiple segment representation */
            durationMs = (periodDuration - durationMs * (segmentNum - 1));
    }

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

/* static */
bool DashUtils::getRepresentationInfo(dash::mpd::IMPD *mpd, MMParamSP param) {
    ENTER1();
    CHECK_MPD(mpd);

    if ( !param ) {
        ERROR("no mem\n");
        return false;
    }

    if (!mpd) {
        ERROR("no media presentation description");
        return false;
    }

    bool dynamic = false;
    const std::string &type = mpd->GetType();
    if (type.find("dynamic") != std::string::npos)
        dynamic = true;

    param->writeInt32(dynamic);
    INFO("dynamic: %d\n", dynamic);
    if (dynamic) {
        param->writeInt32(0);
        param->writeInt32(0);
    }

    int count = mpd->GetPeriods().size();

    if ( count == 0 ) {
        INFO("no period found\n");
        return false;
    }

    // period count
    param->writeInt32(count);
    DEBUG("period count: %d\n", count);

    for ( int i = 0; i < count; ++i) {
        IPeriod *period = mpd->GetPeriods().at(i);
        if (!period) {
            ERROR("period %d is NULL", i);
            return false;
        }
        int val = (TimeResolver::GetDurationInSec(period->GetStart()) * 1000);
        param->writeInt32(val);
        DEBUG("period[%d] start %dms\n", i, val);

        val = (TimeResolver::GetDurationInSec(period->GetDuration()) * 1000);
        param->writeInt32(val);
        DEBUG("period[%d] duration %dms\n", i, val);

        val = 0;
        std::vector<IAdaptationSet*> videoAdaptationSets;
        std::vector<IAdaptationSet*> audioAdaptationSets;

        videoAdaptationSets = AdaptationSetHelper::GetVideoAdaptationSets(period);
        if (!videoAdaptationSets.empty())
            val++;

        audioAdaptationSets = AdaptationSetHelper::GetAudioAdaptationSets(period);
        if (!audioAdaptationSets.empty())
            val++;

        if (val == 0) {
            WARNING("period %d has no media content", i);
        }
        param->writeInt32(val);
        VERBOSE("period[%d] stream count: %d\n", i, val);

        if (!videoAdaptationSets.empty()) {
            // match component::kMediaTypeVideo
            param->writeInt32(DashStream::kStreamTypeVideo + 1);
            param->writeInt32(videoAdaptationSets.size());
        }

        for (int j = 0; j < (int)videoAdaptationSets.size(); j++) {
            IAdaptationSet* adaptationSet = videoAdaptationSets.at(j);
            if (!adaptationSet) {
                WARNING("period %d video adaptationSet %d is null", i, j);
                return false;
            }
            param->writeCString(adaptationSet->GetLang().c_str());
            param->writeInt32(adaptationSet->GetGroup());
            val = adaptationSet->GetRepresentation().size();
            param->writeInt32(val);
            for (int k = 0; k < (int)adaptationSet->GetRepresentation().size(); k++) {
                IRepresentation *rep = adaptationSet->GetRepresentation().at(k);
                if (!rep) {
                    WARNING("period %d video adaptationSet %d rep %d is null", i, j, k);
                    return false;
                }

                const char* mime = NULL;
                if (!(rep->GetMimeType().empty()))
                    mime = rep->GetMimeType().c_str();
                else
                    mime = adaptationSet->GetMimeType().c_str();

                DEBUG("period%d adaptationSet%d representation%d mime: %s\n",
                         i, j, k, mime);

                param->writeCString(mime);
                param->writeInt32(rep->GetBandwidth());

                const std::vector<std::string>& codecs = rep->GetCodecs();
                param->writeInt32(codecs.size());
                for (int l = 0; l < (int)codecs.size(); l++)
                    param->writeCString(codecs.at(l).c_str());
            }
        }

        if (!audioAdaptationSets.empty()) {
            // match component::kMediaTypeAudio
            param->writeInt32(DashStream::kStreamTypeAudio + 1);
            param->writeInt32(audioAdaptationSets.size());
        }

        for (int j = 0; j < (int)audioAdaptationSets.size(); j++) {
            IAdaptationSet* adaptationSet = audioAdaptationSets.at(j);
            if (!adaptationSet) {
                WARNING("period %d audio adaptationSet %d is null", i, j);
                return false;
            }
            param->writeCString(adaptationSet->GetLang().c_str());
            param->writeInt32(adaptationSet->GetGroup());
            val = adaptationSet->GetRepresentation().size();
            param->writeInt32(val);
            for (int k = 0; k < (int)adaptationSet->GetRepresentation().size(); k++) {
                IRepresentation *rep = adaptationSet->GetRepresentation().at(k);
                if (!rep) {
                    WARNING("period %d audio adaptationSet %d rep %d is null", i, j, k);
                    return false;
                }

                const char* mime = NULL;
                if (!(rep->GetMimeType().empty()))
                    mime = rep->GetMimeType().c_str();
                else
                    mime = adaptationSet->GetMimeType().c_str();

                DEBUG("period%d adaptationSet%d representation%d mime: %s\n",
                         i, j, k, mime);

                param->writeCString(mime);
                param->writeInt32(rep->GetBandwidth());

                const std::vector<std::string>& codecs = rep->GetCodecs();
                param->writeInt32(codecs.size());
                for (int l = 0; l < (int)codecs.size(); l++)
                    param->writeCString(codecs.at(l).c_str());
            }
        }
    }

    EXIT_AND_RETURN1(true);
}

/* static */
mm_status_t DashUtils::getPeroidTimeRange(dash::mpd::IMPD *mpd, int period, int &msec, int &durationMs) {
    ENTER();
    CHECK_MPD(mpd);

    if (mpd->GetPeriods().empty()) {
        ERROR("mpd has no period");
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);
    }

    if (period >= (int)mpd->GetPeriods().size()) {
        ERROR("period %d is out of range %d", period, mpd->GetPeriods().size());
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);
    }

    /* TODO consider period->GetSegmentBase()  */
    mm_status_t status = MM_ERROR_UNKNOWN;

    int64_t mpdDurationMs = (TimeResolver::GetDurationInSec(mpd->GetMediaPresentationDuration()) * 1000);

    std::vector<int64_t> periodStartMs;
    std::vector<int64_t> periodDurationMs;

    periodStartMs.resize(mpd->GetPeriods().size(), -1);
    periodDurationMs.resize(mpd->GetPeriods().size(), -1);

    for (int i = 0; i < (int)mpd->GetPeriods().size(); i++) {
        int64_t duration = TimeResolver::GetDurationInSec(mpd->GetPeriods().at(i)->GetDuration()) * 1000;
        int64_t start = TimeResolver::GetDurationInSec(mpd->GetPeriods().at(i)->GetStart()) * 1000;

        DEBUG("%" PRId64" %" PRId64 "", start, duration);
        if (start)
            periodStartMs[i] = start;
        else if (i == 0)
            periodStartMs[i] = 0; // FIXME mpd may not start from 0

        if (duration)
            periodDurationMs[i] = duration;
        else {
            if (mpd->GetPeriods().size() == 1) {
                DEBUG("single period mpd");
                periodStartMs[0] = 0;
                periodDurationMs[0] = mpdDurationMs;
                if (!mpdDurationMs)
                    status = MM_ERROR_SOURCE;
                else
                    status = MM_ERROR_SUCCESS;
                break;
            } else if (i == (int)(mpd->GetPeriods().size() - 1)) {

                if (i != 0 && periodDurationMs[i - 1] < 0)
                    periodDurationMs[i - 1] = periodStartMs[i] - periodStartMs[i - 1];

                periodStartMs[i] += periodStartMs[i - 1] + periodDurationMs[i - 1];;
                periodDurationMs[i] = mpdDurationMs - periodStartMs[i];
                DEBUG("last period %d:%d", periodStartMs[i], periodDurationMs[i]);
                status = MM_ERROR_SUCCESS;
                if (!mpdDurationMs)
                    status = MM_ERROR_SOURCE;
                break;
            }

            if (!start) {
                ERROR("period %d @start and @duration are null", i);
                status = MM_ERROR_SOURCE;
                break;
            }

            periodStartMs[i] = start;
        }

        DEBUG("%d %" PRId64" %" PRId64 "", i, periodStartMs[i], periodDurationMs[i]);
        if (i != 0 && periodDurationMs[i - 1] < 0)
            periodDurationMs[i - 1] = periodStartMs[i] - periodStartMs[i - 1];

        if (i != 0 && (periodStartMs[i - 1] < 0 || periodDurationMs[i - 1] < 0)) {
            ERROR("cannot derive period %d timeline", i);
            status = MM_ERROR_SOURCE;
            break;
        } else if (periodStartMs[i] != INVALID_TIMESTAMPT && i != 0)
            periodStartMs[i] = periodStartMs[i - 1] + periodDurationMs[i - 1];

        DEBUG("%d %" PRId64" %" PRId64 "", period, periodStartMs[period], periodDurationMs[period]);
        if (i >= period &&
            periodStartMs[period] != INVALID_TIMESTAMPT &&
            periodDurationMs[period] != INVALID_TIMESTAMPT) {
            status = MM_ERROR_SUCCESS;
            break;
        }
    }

    if (status == MM_ERROR_SUCCESS) {
        msec = (int)periodStartMs[period];
        durationMs = (int)periodDurationMs[period];
    }

    EXIT_AND_RETURN1(status);
}

/* static */
mm_status_t DashUtils::getPeroidFromTimeMs(dash::mpd::IMPD *mpd, int &period, int &peroidStartMs, int msec) {
    ENTER1();
    CHECK_MPD(mpd);

    int64_t durationMs = (TimeResolver::GetDurationInSec(mpd->GetMediaPresentationDuration()) * 1000);

    if (durationMs != 0 && msec > durationMs) {
        ERROR("seek out of range");
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);
    }

    if (mpd->GetPeriods().empty()) {
        ERROR("mpd has no period");
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);
    }

    mm_status_t status = MM_ERROR_INVALID_PARAM;
    for (int i = 0; i < (int)mpd->GetPeriods().size(); i++) {
        int startMs, durationMs;
        if ((status = DashUtils::getPeroidTimeRange(mpd, i, startMs, durationMs)) != MM_ERROR_SUCCESS) {
            ERROR("fail to get period time range");
            break;
        }

        DEBUG("period %d, start %dms duration %dms", i, startMs, durationMs);
        if(msec >= startMs && msec < startMs + durationMs) {
            status = MM_ERROR_SUCCESS;
            period = i;
            peroidStartMs = startMs;
            break;
        }
    }

    EXIT_AND_RETURN1(status);
}

/* static */
mm_status_t DashUtils::selectRepresentationFromPeroid(IPeriod* period,
                                                      IAdaptationSet* curAdaptationSet,
                                                      IRepresentation* curRepresentation,
                                                      IAdaptationSet* &adaptationSet,
                                                      IRepresentation* &representation) {
    ENTER();

    if (!period || !curAdaptationSet || !curRepresentation)
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);

    std::vector<dash::mpd::IAdaptationSet *>  adaptationSets;

    if (AdaptationSetHelper::IsVideoAdaptationSet(curAdaptationSet))
        adaptationSets = AdaptationSetHelper::GetVideoAdaptationSets(period);
    else if (AdaptationSetHelper::IsAudioAdaptationSet(curAdaptationSet))
        adaptationSets = AdaptationSetHelper::GetAudioAdaptationSets(period);
    else
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);


    // FIXME select close adaptation set and representation
    adaptationSet = adaptationSets.at(0);
    representation = adaptationSet->GetRepresentation().at(0);

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

/* static */
mm_status_t DashUtils::getSegmentWithTimeMs(dash::mpd::IMPD *mpd,
                                            IPeriod *period,
                                            IAdaptationSet* adaptationSet,
                                            IRepresentation* representation,
                                            int peroidStartMs,
                                            int msec,
                                            int &index) {
    ENTER();
    CHECK_MPD(mpd);

    if (!period || !adaptationSet || !representation)
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);

    int pos = peroidStartMs;
    int periodIndex = -1;

    if (msec < pos)
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);

    for (int i = 0; i < (int)mpd->GetPeriods().size(); i++) {
        if (period == mpd->GetPeriods().at(i))
            periodIndex = i;
    }

    if (periodIndex < 0)
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);

    mm_status_t status;
    index = 0;

    do {
        int durationMs;
        status = getSegmentDuration(mpd,
                               periodIndex,
                               adaptationSet,
                               representation,
                               index,
                               durationMs);

        if (status != MM_ERROR_SUCCESS)
            break;

        if (pos + durationMs > msec)
            break;

        index++;
        pos += durationMs;

    } while(1);


    EXIT_AND_RETURN(status);
}

/* static */
size_t DashUtils::getSegmentNumber(dash::mpd::IMPD *mpd,
                                   int periodIndex,
                                   IAdaptationSet* adaptationSet,
                                   IRepresentation* representation) {
    ENTER1();
    CHECK_MPD(mpd);

    IPeriod* period = mpd->GetPeriods().at(periodIndex);

    if (!period || !adaptationSet || !representation)
        EXIT_AND_RETURN1(-1);

    ISegmentBase* base = NULL;
    RepresentationType type = getRepresentationType(period, adaptationSet, representation, &base);

    size_t size = -1;
    if (type == kSegmentUndefined)
        EXIT_AND_RETURN1(-1);
    else if (type == kSingleSegment) { /* period duration */
        EXIT_AND_RETURN(1);
    } else if (type == kSegmentList){
        ISegmentList *segmentList = dynamic_cast<ISegmentList*>(base);
        if (!segmentList) {
            ERROR("Segment List is NULL");
            EXIT_AND_RETURN1(-1);
        }
        size = segmentList->GetSegmentURLs().size();
    } else {
        ISegmentTemplate *segmentTemplate = dynamic_cast<ISegmentTemplate*>(base);
        if (!segmentTemplate) {
            ERROR("Segment Template is NULL");
            EXIT_AND_RETURN1(-1);
        }

        if (segmentTemplate->GetSegmentTimeline()) {
            size_t numOfTimelines = 0, repeatCount = 0;
            numOfTimelines = segmentTemplate->GetSegmentTimeline()->GetTimelines().size();
            for (size_t i = 0; i < numOfTimelines; i++) {
                repeatCount = segmentTemplate->GetSegmentTimeline()->GetTimelines().at(i)->GetRepeatCount();
                if (repeatCount > 0)
                    size += repeatCount;
                else
                    size++;
            }
        } else {
            int msec, periodDuration;
            mm_status_t status = getPeroidTimeRange(mpd, periodIndex, msec, periodDuration);
            if (status != MM_ERROR_SUCCESS)
                EXIT_AND_RETURN1(-1);

            int durationMs;
            status = getSegmentDuration(mpd,
                               periodIndex,
                               adaptationSet,
                               representation,
                               0,
                               durationMs);

            if (status != MM_ERROR_SUCCESS)
                EXIT_AND_RETURN1(-1);

            size = ceil(1.0f * periodDuration / durationMs);
        }
    }

    EXIT_AND_RETURN1(size);
}

/* static */
mm_status_t DashUtils::getAlternativeBandWidth(IPeriod* period,
                                        dash::mpd::IAdaptationSet* adaptationSet,
                                        std::vector<int> &bandWidth) {
    ENTER();


    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

/* static */
size_t DashUtils::getPeroidIndex(dash::mpd::IMPD* mpd, dash::mpd::IPeriod* period) {
    ENTER();
    CHECK_MPD(mpd);

    if (!period)
        EXIT_AND_RETURN1(-1);

    for (int i = 0; i < (int)mpd->GetPeriods().size(); i++) {
        if (period == mpd->GetPeriods().at(i))
            return (size_t)i;
    }

    EXIT_AND_RETURN1(-1);
}

} // end of namespace YUNOS_DASH
} // end of namespace YUNOS_MM
