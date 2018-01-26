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

#ifndef __channel_H
#define __channel_H

#include <semaphore.h>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include "multimedia/mmthread.h"
#include <multimedia/instantaudio.h>
#include "sample.h"
#include "multimedia/mm_audio_compat.h"
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
#include <multimedia/mm_amhelper.h>
#endif

namespace YUNOS_MM {

// status: MM_ERROR_XXX
// this callback will be invoked after the playback is completed or an error occured.
typedef void (*play_competed_cb)(int channelID, int status, void * userData);

class Channel {
public:
    Channel(int id, play_competed_cb playCompletedCB, void * cbUserData);
    ~Channel();

public:
    int id() const { return mId; }
    int sampleID() const { return mSample->id(); }
    // returns: MM_ERROR_XXX
    mm_status_t play(const Sample * sample, const InstantAudio::VolumeInfo * volume, int loop, float rate, int streamType);
    mm_status_t play(const Sample * sample, const InstantAudio::VolumeInfo * volume, int loop, float rate, const char * connectionId);
    mm_status_t pause();
    mm_status_t resume();
    mm_status_t stop();
    mm_status_t setVolume(const InstantAudio::VolumeInfo * volume);
    mm_status_t setLoop(int loop);
    mm_status_t setRate(float rate);
    mm_status_t setPriority(int priority);
    int priority() const { return mPriority; }

private:
    Channel();

private:
    enum state_t {
        STATE_PREPARED,
        STATE_STARTED,
        STATE_PAUSED,
        STATE_ERROR,
    };

    mm_status_t doStop();
    int write();

#ifdef __AUDIO_CRAS__
    mm_status_t createCrasClient();
    mm_status_t destroyCrasClient();
    static void audioCallback(YunOSAudioNS::AudioRender::evt_t event, void *user, void *info);
    void onMoreData(YunOSAudioNS::AudioRender::evt_t event, void *info);
    void onStreamError(YunOSAudioNS::AudioRender::evt_t event, void *info);
#elif defined(__AUDIO_PULSE__)
    static pa_sample_format_t format_convert(snd_format_t format);
    mm_status_t createContext();
    void destroyContext();
    mm_status_t createStream(const Sample *sample, const char * connectionId);
    void destroyStream();
    static void context_state_cb(pa_context *c, void *userdata);
    static void stream_state_cb(pa_stream *s, void * userdata);
    static void stream_write_cb(pa_stream *p, size_t nbytes, void *userdata);
    static void stream_latency_cb(pa_stream *p, void *userdata);
    static void stream_cork_cb(pa_stream*s, int success, void *userdata);
    static void success_context_cb(pa_context *c, int success, void *userdata);
    static void drain_cb(pa_stream *s, int success, void *userdata);
    mm_status_t cork(int b);
    void drain();
#endif
    private:
        int logid() { return id(); }

private:
    class WriteThread : public MMThread {
    public:
        WriteThread(Channel * watcher);
        ~WriteThread();
    public:
        bool start();
        void stop();
        void write();

    protected:
        virtual void main();

    private:
        int logid() { return mWather->id(); }

    private:
        Channel * mWather;
        sem_t mSem;
        bool mContinue;

        DECLARE_LOGTAG()
    };

    friend class WriteThread;


    int mId;
    Lock mLock;
    state_t mState;
    Sample *mSample;
    InstantAudio::VolumeInfo mVolume;
    int mLoop;
    int mCurrentLoop;
    size_t mWriteIndex;
    float mRate;
    int mPriority;
    play_competed_cb mPlayCompletedCB;
    void *mPlayCompletedCbUserParam;

#define STREAM_NAME_SIZE 32
    char mStreamName[STREAM_NAME_SIZE];
#ifdef __AUDIO_CRAS__
    YunOSAudioNS::AudioRender *mRender;
    int32_t mBytesPerSample;
    int32_t mAudioStreamType;
#elif defined(__AUDIO_PULSE__)
    pa_context *mPAContext;
    pa_threaded_mainloop *mPALoop;
    pa_stream *mPAStream;
    pa_usec_t  mLatency;
    int mCorkResult;
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_DECLARE()
#endif
#endif
    WriteThread * mWriteThread;

    MM_DISALLOW_COPY(Channel)
    DECLARE_LOGTAG()
};

}

#endif // __soundchannel_H
