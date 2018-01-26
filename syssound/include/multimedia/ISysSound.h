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

#ifndef __ISysSound_H
#define __ISysSound_H

namespace YUNOS_MM {

class ISysSound {
public:
    enum SoundType {
        // Keyboard and direction pad click sound
        ST_KEY_CLICK = 0,
        // Focus has moved up
        ST_FOCUS_NAVIGATION_UP,
        // Focus has moved down
        ST_FOCUS_NAVIGATION_DOWN,
        // Focus has moved left
        ST_FOCUS_NAVIGATION_LEFT,
        // Focus has moved right
        ST_FOCUS_NAVIGATION_RIGHT,
        // IME standard keypress sound
        ST_KEYPRESS_STANDARD,
        // IME spacebar keypress sound
        ST_KEYPRESS_SPACEBAR,
        // IME delete keypress sound
        ST_KEYPRESS_DELETE,
        // IME return_keypress sound
        ST_KEYPRESS_RETURN,
        // Invalid keypress sound
        ST_KEYPRESS_INVALID,
        // Screen lock sound
        ST_SCREEN_LOCK,
        // Screen unlock sound
        ST_SCREEN_UNLOCK,
        ST_COUNT
    };

public:
    ISysSound(){}
    virtual ~ISysSound(){}

public:
    virtual mm_status_t play(SoundType soundType, float leftVolume, float rightVolume) = 0;

public:
    static const char * serviceName() { return "syssound.service.media.yunos.com"; }
    static const char * pathName() { return "/com/yunos/media/service/syssound"; }
    static const char * _interface() { return "syssound.service.media.yunos.com.interface"; }
};

}

#endif
