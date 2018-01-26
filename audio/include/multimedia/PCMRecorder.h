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

#ifndef __PCMRecorder_H
#define __PCMRecorder_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mmlistener.h>
#include "multimedia/mm_audio.h"

namespace YUNOS_MM {

class PCMRecorder {
public:
    class Listener : public MMListener {
    public:
        Listener(){}
        virtual ~Listener(){}

        enum message_type {
            // Request to read more data from PCM buffer.
            // params:
            //    param1: bytes to read
            EVENT_MORE_DATA               = 0,
            // an error occured
            // params:
            //     param1: erro code, see error_type
            MSG_ERROR             = 100,
        };

        MM_DISALLOW_COPY(Listener)
    };

public:
    static PCMRecorder * create();
    static void destroy(PCMRecorder * recorder);

    void setListener(Listener * listener);

    /*
     * format: The sample format
     * sampleRate: The sample rate. (e.g. 44100)
     * channels: Audio channels. 
     * channelsMap: channel position map
     */
    virtual mm_status_t prepare(adev_source_t inputSource,
                            snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount,
                            adev_channel_mask_t * channelsMap,
                            const char * audioConnectionId = NULL) = 0;

    virtual mm_status_t start() = 0;
    virtual mm_status_t pause() = 0;
    virtual size_t read(void* buffer, size_t size) = 0;

    virtual mm_status_t reset() = 0;

protected:
    PCMRecorder();
    virtual ~PCMRecorder();

protected:
    Listener * mListener;

MM_DISALLOW_COPY(PCMRecorder);
};

}

#endif // __PCMRecorder_H
