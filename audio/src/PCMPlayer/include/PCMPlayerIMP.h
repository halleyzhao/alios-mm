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

#ifndef __PCMPlayerIMP_H
#define __PCMPlayerIMP_H

#include <pthread.h>
#include <stdio.h>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/PCMPlayer.h>
#include "multimedia/mm_audio_compat.h"
#ifdef __AUDIO_PULSE__
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
#include <multimedia/mm_amhelper.h>
#endif
#endif

namespace YUNOS_MM {

class PCMPlayerIMP : public PCMPlayer {
public:
    PCMPlayerIMP();
    ~PCMPlayerIMP();

public:

    virtual mm_status_t play(const char *filename,
                            snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount,
                            const char * connectionId = NULL);
    virtual void pause();
    virtual void resume();
    virtual void stop();

private:
    enum state_t {
        STATE_IDLE,
        STATE_PLAYING,
        STATE_PAUSED
    };

	void stop_l();

#ifdef __AUDIO_CRAS__
    mm_status_t createCrasClient(snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount);
    void destroyCrasClient();
    static void audioCallback(YunOSAudioNS::AudioRender::evt_t event, void *user, void *info);
    void onMoreData(YunOSAudioNS::AudioRender::evt_t event, void *info);
#elif defined(__AUDIO_PULSE__)
    int createSample(snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount,
                            const char * connectionId);
    static void *play_thread_func(void *param);
#endif

private:
    state_t mState;
#ifdef __AUDIO_CRAS__
    YunOSAudioNS::AudioRender *mRender;
    int32_t mBytesPerSample;
#elif defined(__AUDIO_PULSE__)
    pa_simple *s;
    pthread_t play_thread;
    bool stop_play_thread;
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_DECLARE()
#endif
#endif
    FILE *mFile;
    Lock mLock;

    MM_DISALLOW_COPY(PCMPlayerIMP);
};

}

#endif // __PCMPlayerIMP_H
