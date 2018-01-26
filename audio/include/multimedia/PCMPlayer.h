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

#ifndef __PCMPlayer_H
#define __PCMPlayer_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mmlistener.h>
#include "multimedia/mm_audio.h"

namespace YUNOS_MM {

class PCMPlayer {
public:
    class Listener : public MMListener {
    public:
        Listener(){}
        virtual ~Listener(){}

        enum message_type {
            // Send play complete cb to node layer.
            // params:
            //    param1: 1 to complete
            PLAY_COMPLETE         = 0,
            // an error occured
            // params:
            //     param1: erro code, see error_type
            MSG_ERROR             = 100,
        };

        MM_DISALLOW_COPY(Listener)
    };

public:
    static PCMPlayer * create();
    static void destroy(PCMPlayer * player);
    void setListener(Listener * listener);

    virtual mm_status_t play(const char *filename,
                            snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount,
                            const char * connectionId = NULL) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;

#ifdef YUNOS_PRODUCT_yunhal
    virtual mm_status_t play(const char *filename) { return MM_ERROR_UNSUPPORTED; }
    virtual mm_status_t reset() { return MM_ERROR_UNSUPPORTED; }
#endif

protected:
    PCMPlayer();
    virtual ~PCMPlayer();

protected:
    Listener * mListener;

MM_DISALLOW_COPY(PCMPlayer);
};

}

#endif // __PCMPlayer_H
