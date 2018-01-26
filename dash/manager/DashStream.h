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

#ifndef __dash_stream_h
#define __dash_stream_h

#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mmparam.h>
#include <string>

namespace dash {
    namespace mpd {
        class IMPD;
    };
};

namespace YUNOS_MM {

namespace YUNOS_DASH {

class SegmentBuffer;
class DashStream;

typedef MMSharedPtr<DashStream> DashStreamSP;

class DashStream {
public:

    enum StreamType {
        kStreamTypeVideo,
        kStreamTypeAudio,
        kStreamTypeSubtitle,
        kStreamTypeMultiplex,
        kStreamTypeUndefined,
    };

    static DashStream* createStreamByName(char *mpd, StreamType type);
    static DashStream* createStreamByMpd(dash::mpd::IMPD *mpd, StreamType type);

    virtual ~DashStream();

    virtual SegmentBuffer* createSegmentBuffer() = 0;

    virtual mm_status_t start() = 0;
    virtual void stop() = 0;

    /*
     * get segmnet index
     */
    virtual uint32_t getPosition() = 0;

    /*
     * set segmnet index
     */
    virtual void setPosition(uint32_t index) = 0;

    /*
     * select stream type if kStreamTypeUndefined is used in constructor
     */
    virtual bool setStreamType(StreamType type) = 0;

    virtual void clear() = 0;
    virtual void clearTail() = 0;
    virtual mm_status_t seek(int64_t usec) = 0;

    virtual mm_status_t setRepresentation(int period, int adaptationSet, int representation, bool flush = false) = 0;

    virtual bool getRepresentationInfo(MMParamSP param) = 0;
    virtual mm_status_t getDuration(int64_t &durationMs) = 0;
    virtual bool hasMediaType(StreamType type, int period = 0) = 0;
    virtual dash::mpd::IMPD *getMPD() = 0;

    virtual mm_status_t getCurrentSegmentStart(int &period, int64_t &startUs, int64_t &durationUs) = 0;

friend class SegmentBuffer;

protected:
    DashStream();

private:
    MM_DISALLOW_COPY(DashStream)
};

} // end of namespace YUNOS_DASH

} // end of namespace YUNOS_MM
#endif //__dash_stream_h
