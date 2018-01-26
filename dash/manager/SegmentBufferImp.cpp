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

#include "SegmentBufferImp.h"

#include <multimedia/mm_debug.h>

using namespace libdash::framework::input;

namespace YUNOS_MM {

namespace YUNOS_DASH {

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

DEFINE_LOGTAG(SegmentBufferImp)

SegmentBufferImp::SegmentBufferImp(MMSharedPtr<DASHManager> manager)
    : mManager(manager) {
    ENTER1();

    if (!mManager)
        WARNING("input::DASHManager is null");

    EXIT1();
}

SegmentBufferImp::~SegmentBufferImp() {
    ENTER1();

    EXIT1();
}

bool SegmentBufferImp::start() {
    ENTER1();
    if (mManager)
        return mManager->Start();

    EXIT_AND_RETURN1(false);
}

void SegmentBufferImp::stop() {
    ENTER1();
    if (mManager)
        return mManager->Stop();

    EXIT1();
}

MediaObject* SegmentBufferImp::getSegment(uint32_t segment) {
    ENTER();

    ERROR("method not supported");

    return NULL;
}

MediaObject* SegmentBufferImp::getNextSegment() {
    ENTER();

    if (mManager) {
        return mManager->getNextSegment();
    }

    ERROR("dash manager is not provided");

    return NULL;
}

MediaObject* SegmentBufferImp::getInitSegment(MediaObject* segment) {
    ENTER();
    if (mManager)
        return mManager->getInitSegment(segment);

    ERROR("dash manager is not provided");

    return NULL;
}

} // end of namespace YUNOS_DASH

} // end of namespace YUNOS_MM
