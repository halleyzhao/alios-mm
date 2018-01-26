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
#include "DashStreamImp.h"

namespace YUNOS_MM {

namespace YUNOS_DASH {

/* static */
DashStream* DashStream::createStreamByName(char *mpd, StreamType type) {
    return new DashStreamImp(mpd, type);
}

/* static */
DashStream* DashStream::createStreamByMpd(dash::mpd::IMPD *mpd, StreamType type) {
    return new DashStreamImp(mpd, type);
}

DashStream::~DashStream() {

}

DashStream::DashStream() {

}

} // end of namespace YUNOS_DASH

} // end of namespace YUNOS_MM
