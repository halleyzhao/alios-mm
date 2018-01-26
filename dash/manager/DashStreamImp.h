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

#include "DashStream.h"

#include <DASHManager.h>
#include <IMPD.h>

#ifndef __dash_stream_imp_h
#define __dash_stream_imp_h

namespace YUNOS_MM {

namespace YUNOS_DASH {

class DashStreamImp : public DashStream,
                      public libdash::framework::input::IDASHManagerObserver {

public:

    DashStreamImp(char *mpd, StreamType type);
    DashStreamImp(dash::mpd::IMPD *mpd, StreamType type);
    virtual ~DashStreamImp();

    virtual SegmentBuffer* createSegmentBuffer();
    virtual mm_status_t start();
    virtual void stop();
    virtual uint32_t getPosition();
    virtual void setPosition(uint32_t index);
    virtual void clear();
    virtual void clearTail();
    virtual mm_status_t seek(int64_t msec);
    virtual mm_status_t setRepresentation(int period, int adaptationSet, int representation, bool flush = false);
    virtual bool setStreamType(StreamType type);
    bool getRepresentationInfo(MMParamSP param);
    virtual mm_status_t getDuration(int64_t &durationMs);
    virtual bool hasMediaType(StreamType type, int period = 0);
    virtual dash::mpd::IMPD *getMPD() { return mMpd; }

    virtual mm_status_t getCurrentSegmentStart(int &period, int64_t &startUs, int64_t &durationUs);

    /* IDASHManagerObserver */
    virtual void OnSegmentBufferStateChanged   (uint32_t fillstateInPercent) {}
    virtual void OnEndOfPeriod();

private:
    void stop_l();

    StreamType mType;
    dash::mpd::IMPD *mMpd;
    bool mOwnMpd;
    bool mRepresentationSelected;
    dash::mpd::IPeriod          *mPeriod;
    dash::mpd::IAdaptationSet   *mAdaptationSet;
    dash::mpd::IRepresentation  *mRepresentation;
    int mStartSegIdx;

    dash::IDASHManager *mMpdFacotry;

    MMSharedPtr<libdash::framework::input::DASHManager> mManager;
    MMSharedPtr<SegmentBuffer> mSegmentBuffer;
    Lock mLock;

private:
    void setRepresentationSegment(int idx);

    MM_DISALLOW_COPY(DashStreamImp)

    DECLARE_LOGTAG()
};

} // end of namespace YUNOS_DASH

} // end of namespace YUNOS_MM
#endif //__dash_stream_imp_h
