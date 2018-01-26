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

#ifndef __segment_buffer_h
#define __segment_buffer_h

#include <multimedia/mm_cpp_utils.h>
#include <string>

#include <MediaObject.h>

namespace libdash
{
    namespace framework
    {
        namespace input
        {
            class DASHManager;
        }
    }
}

using namespace libdash::framework::input;

namespace YUNOS_MM {

namespace YUNOS_DASH {

class SegmentBuffer {
public:

    static SegmentBuffer* create(MMSharedPtr<DASHManager> manager);
    virtual ~SegmentBuffer();

    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual MediaObject* getSegment(uint32_t segment) = 0;
    virtual MediaObject* getNextSegment() = 0;
    virtual MediaObject* getInitSegment(MediaObject* segment) = 0;

protected:
    SegmentBuffer();

private:
    MM_DISALLOW_COPY(SegmentBuffer)
};

} // end of namespace YUNOS_DASH

} // end of namespace YUNOS_MM
#endif //__segment_buffer_h
