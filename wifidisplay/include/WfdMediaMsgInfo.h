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

#ifndef __wfd_media_msg_info
#define __wfd_media_msg_info

namespace YUNOS_MM {

// definition for param1 when msg is MSG_INFO_EXT

enum {
    // param2: rtp port
    WFD_MSG_INFO_RTP_CREATE,
    // param2: na
    WFD_MSG_INFO_RTP_STALL,
    // param2: na
    WFD_MSG_INFO_RTP_DESTROY,
};

} // end of namespace YUNOS_MM
#endif //__wfd_media_msg_info
