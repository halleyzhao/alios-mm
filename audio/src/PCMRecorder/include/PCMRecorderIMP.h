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

#ifndef __PCMRecorderIMP_H
#define __PCMRecorderIMP_H

#include <pthread.h>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/PCMRecorder.h>
#ifdef __AUDIO_CRAS__
#include <queue>
#include <multimedia/media_buffer.h>
#endif
#ifdef __AUDIO_PULSE__
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
#include <multimedia/mm_amhelper.h>
#endif
#endif
#include "multimedia/mm_audio_compat.h"

namespace YUNOS_MM {

class PCMRecorderIMP : public PCMRecorder {
public:
    PCMRecorderIMP();
    ~PCMRecorderIMP();

public:
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
                            const char * audioConnectionId = NULL);

    virtual mm_status_t start();
    virtual mm_status_t pause();
    virtual size_t read(void* buffer, size_t size);

    virtual mm_status_t reset();

private:
    enum state_t {
        STATE_IDLE,
        STATE_PREPARED,
        STATE_STARTED,
        STATE_PAUSED,
        STATE_ERROR,
    };

    mm_status_t pause_l();
    void reset_l();
    mm_status_t cork(int b);
#ifdef __AUDIO_CRAS__
    mm_status_t createCrasClient();
    mm_status_t destroyCrasClient();
    static void audioCallback(YunOSAudioNS::AudioCapture::evt_t evt, void* user, void *info);
    static bool releaseInputBuffer(MediaBuffer* mediaBuffer);
    void clearSourceBuffers();
#elif defined(__AUDIO_PULSE__)
    pa_sample_format convertFormatToPulse(snd_format_t format);
    mm_status_t createContext();
    mm_status_t createStream(const char * audioConnectionId);
    static void context_state_cb(pa_context *c, void *userdata);
    static void stream_state_cb(pa_stream *s, void * userdata);
    static void stream_read_cb(pa_stream *p, size_t nbytes, void *userdata);
    static void stream_latency_cb(pa_stream *p, void *userdata);
    static void stream_cork_cb(pa_stream*s, int success, void *userdata);
#endif

private:

    Condition mCondition;
    Lock mLock;
    state_t mState;
    adev_source_t mInputSouce;
    snd_format_t mFormat;
    uint32_t mSampleRate;
    uint8_t mChannelCount;
    adev_channel_mask_t * mChannelsMap;

#ifdef __AUDIO_CRAS__
    YunOSAudioNS::AudioCapture *mCapture;
    uint32_t mMinBufSize;
    bool mIsPaused;
    std::queue<MediaBufferSP> mAvailableSourceBuffers;
#elif defined(__AUDIO_PULSE__)
    pa_context * mPAContext;
    pa_threaded_mainloop *mPALoop;
    pa_stream * mPAStream;
    const uint8_t * mPAReadData;
    size_t mPAReadableSize;
    size_t mPAReadIndex;
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_DECLARE()
#endif
#endif
    int mCorkResult;

    MM_DISALLOW_COPY(PCMRecorderIMP);
};

}

#endif // __PCMRecorderIMP_H
