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

#ifndef __dash_utils_h
#define __dash_utils_h

#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mmparam.h>
#include <vector>

namespace dash {
    namespace mpd {
        class IMPD;
        class IPeriod;
        class IAdaptationSet;
        class IRepresentation;
        class ISegmentBase;
    };
};

namespace YUNOS_MM {

namespace YUNOS_DASH {

class DashUtils {
public:

    enum RepresentationType {
        kSingleSegment,
        kSegmentList,
        kSegmentTemplate,
        kSegmentUndefined
    };

    static RepresentationType getRepresentationType(dash::mpd::IPeriod* period,
                                                    dash::mpd::IAdaptationSet* adaptationSet,
                                                    dash::mpd::IRepresentation* representation,
                                                    dash::mpd::ISegmentBase **base = NULL);
    static mm_status_t getSegmentDuration(dash::mpd::IMPD *mpd,
                               int periodIndex,
                               dash::mpd::IAdaptationSet* adaptationSet,
                               dash::mpd::IRepresentation* representation,
                               int index,
                               int &durationMs);

    static bool getRepresentationInfo(dash::mpd::IMPD *mpd, MMParamSP param);

    static mm_status_t getPeroidTimeRange(dash::mpd::IMPD *mpd, int peroid, int &msec, int &durationMs);

    static mm_status_t getPeroidFromTimeMs(dash::mpd::IMPD *mpd, int &peroid, int &peroidStartMs, int msec);

    /*
     * when switch to another peroid, try to select a similar representation
     * with respect to codec/bandwidth etc.
     */
    static mm_status_t selectRepresentationFromPeroid(dash::mpd::IPeriod* period,
                                                      dash::mpd::IAdaptationSet* curAdaptationSet,
                                                      dash::mpd::IRepresentation* curRepresentation,
                                                      dash::mpd::IAdaptationSet* &adaptationSet,
                                                      dash::mpd::IRepresentation* &representation);

    static mm_status_t getSegmentWithTimeMs(dash::mpd::IMPD *mpd,
                                            dash::mpd::IPeriod* period,
                                            dash::mpd::IAdaptationSet* adaptationSet,
                                            dash::mpd::IRepresentation* representation,
                                            int peroidStartMs,
                                            int msec,
                                            int &index);

    static size_t getSegmentNumber(dash::mpd::IMPD* mpd,
                                   int periodIndex,
                                   dash::mpd::IAdaptationSet* adaptationSet,
                                   dash::mpd::IRepresentation* representation);

    static mm_status_t getAlternativeBandWidth(dash::mpd::IPeriod* peroid,
                                        dash::mpd::IAdaptationSet* adaptationSet,
                                        std::vector<int> &bandWidth);

    static size_t getPeroidIndex(dash::mpd::IMPD* mpd, dash::mpd::IPeriod* period);

    DECLARE_LOGTAG()
};

} // end of namespace YUNOS_DASH

} // end of namespace YUNOS_MM
#endif //__dash_utils_h
