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

#include <string.h>

#include <multimedia/mm_debug.h>
#include <multimedia/mmlistener.h>
#include "PCMPlayerIMP.h"

#ifndef MM_LOG_OUTPUT_V
//#define MM_LOG_OUTPUT_V
#endif

namespace YUNOS_MM {

MM_LOG_DEFINE_MODULE_NAME("PCMPlayerIMP");

#define FUNC_ENTER() do { MMLOGI("+\n"); }while(0)
#define FUNC_LEAVE() do { MMLOGV("-\n"); }while(0)
#ifdef __AUDIO_PULSE__
#include <unistd.h>
static const char * CONTEXT_NAME = "PCMPlayer";
static const char * STREAM_NAME = "PCMPlayer";
#define MAX_VOLUME 10
#endif

#define setState(_param, _deststate) do {\
    MMLOGI("change state from %d to %s\n", _param, #_deststate);\
    (_param) = (_deststate);\
}while(0)

PCMPlayerIMP::PCMPlayerIMP() :mState(STATE_IDLE),
#ifdef __AUDIO_CRAS__
                                  mRender(NULL),
                                  mBytesPerSample(2),
#elif defined(__AUDIO_PULSE__)
                                  s(NULL),
                                  play_thread(0),
                                  stop_play_thread(false),
#endif
                                  mFile(NULL)
{
    FUNC_ENTER();
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_INIT();
#endif
    FUNC_LEAVE();
}

PCMPlayerIMP::~PCMPlayerIMP()
{
    FUNC_ENTER();
    MMAutoLock locker(mLock);
    if (mState != STATE_IDLE) {
        stop_l();
    }
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_CLEAN();
#endif
    FUNC_LEAVE();
}

#ifdef __AUDIO_CRAS__
void PCMPlayerIMP::audioCallback(YunOSAudioNS::AudioRender::evt_t event, void * user,void * info) {
    PCMPlayerIMP *me = static_cast<PCMPlayerIMP *>(user);
    if (!me) {
        MMLOGE("audio cb invalid param");
        return;
    }

    if (YunOSAudioNS::AudioRender::EVT_MORE_DATA == event) {
        me->onMoreData(event, info);
    }
}

void PCMPlayerIMP::onMoreData(YunOSAudioNS::AudioRender::evt_t event, void *info) {
    MMASSERT(YunOSAudioNS::AudioRender::EVT_MORE_DATA == event);

    MMAutoLock locker(mLock);
    YunOSAudioNS::AudioRender::Buffer *buffer = static_cast<YunOSAudioNS::AudioRender::Buffer *>(info);
    int writeableSize = buffer->mFrameCount * mBytesPerSample;
    buffer->mFrameCount = 0;
    if (mState != STATE_PLAYING) {
        MMLOGV("invalid state\n");
        return;
    }

    ssize_t rBytes = fread(buffer->mBuffer, 1, writeableSize, mFile);
    if (rBytes == 0) {
        MMLOGI("read done\n");
        if (mListener) {
            MMLOGV("send play complete to listener: \n");
            mListener->onMessage(Listener::PLAY_COMPLETE, 1, 0, 0);
        }
        return;
    }

    buffer->mFrameCount = rBytes / mBytesPerSample;
}

mm_status_t PCMPlayerIMP::createCrasClient(snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount)
{
    FUNC_ENTER();
    uint32_t outFrameCnt = YunOSAudioNS::AudioRender::getDefaultPeriodSize(
        sampleRate,
        adev_get_out_mask_from_channel_count(channelCount),
        format,
        AS_TYPE_MUSIC);
    mRender = YunOSAudioNS::AudioRender::create(AS_TYPE_MUSIC,
        sampleRate,
        format,
        adev_get_out_mask_from_channel_count(channelCount),
        outFrameCnt,
        ADEV_OUTPUT_FLAG_NONE,
        audioCallback,
        this);
    if (!mRender) {
        ERROR("audio render create fail");
        return MM_ERROR_NO_MEM;
    }
    mBytesPerSample = adev_bytes_per_sample(format) * channelCount;
    mRender->start();

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

void PCMPlayerIMP::destroyCrasClient()
{
    FUNC_ENTER();
    if (!mRender) {
        MMLOGI("already destroyed");
        return;
    }

    mRender->stop();
    MM_RELEASE(mRender);
    MMLOGI("-\n");
}
#endif

#ifdef __AUDIO_PULSE__
int PCMPlayerIMP::createSample(snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount,
                            const char * connectionId)
{
    MMLOGD("device: %s\n", connectionId);
    pa_sample_spec ss;
    ss.format = (pa_sample_format_t)format;
    ss.rate = sampleRate;
    ss.channels = channelCount;
    pa_buffer_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.maxlength = (uint32_t) -1;;
    attr.tlength = (uint32_t) pa_usec_to_bytes(250*1000, &ss);
    attr.prebuf = (uint32_t) -1;
    attr.minreq =  (uint32_t) -1;
    attr.fragsize = (uint32_t) -1;
    int error = 0;

    MMLOGD("new simple\n");
    std::string connectionIdStr = connectionId;
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
       ENSURE_AUDIO_DEF_CONNECTION_ENSURE(connectionIdStr, MMAMHelper::playChnnelMain());
#endif
    if (!(s = pa_simple_new("127.0.0.1", connectionIdStr.c_str(), PA_STREAM_PLAYBACK, NULL, STREAM_NAME, &ss, NULL, &attr, &error))) {
        MMLOGE("pa_simple_new error");
        return -1;
    }
    /*if(pa_simple_set_volume(s, MAX_VOLUME, &error)!=0){
        MMLOGE("pa_simple_set_volume error");
        pa_simple_free(s);
        s = NULL;
        return -1;
    }*/

    MMLOGD("new simple success\n");
    return 0;
}

void * PCMPlayerIMP::play_thread_func(void *param)
{
#define BUFSIZE 1024
    if (!param) {
        return NULL;
    }
    PCMPlayerIMP *me = static_cast<PCMPlayerIMP*>(param);

    int error = 0;
    while (!me->stop_play_thread) {
        if (me->mState != STATE_PLAYING) {
            usleep(100000);
            continue;
        }
        char *buf = (char *)malloc(BUFSIZE);
        ssize_t rBytes = 0;
        rBytes = fread(buf, 1, BUFSIZE, me->mFile);
        if ( rBytes == 0 ) {
            free(buf);
            buf = NULL;
            MMLOGE("read done\n");
            break;
        }
        if (!me->s || pa_simple_write(me->s, buf, (size_t)rBytes, &error) < 0 ) {
            free(buf);
            buf = NULL;
            MMLOGE("pa_simple_write error");
            goto finish;
        }

        free(buf);
        buf = NULL;
    }

    if (!me->s || pa_simple_drain(me->s, &error) < 0) {
        MMLOGE("pa_simple_drain error");
        goto finish;
    }

finish:
    if (me->mListener) {
        MMLOGV("send play complete to listener: \n");
        me->mListener->onMessage(Listener::PLAY_COMPLETE, 1, 0, 0);
    }

    return NULL;
}
#endif

mm_status_t PCMPlayerIMP::play(const char *filename,
                            snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount,
                            const char * connectionId)
{
    FUNC_ENTER();
    if (!filename) {
        MMLOGE("invalid fname\n");
        return MM_ERROR_INVALID_PARAM;
    }
    MMLOGI("fname: %s, format: %d, sampleRate: %u, channelCount: %d\n", filename, format, sampleRate, channelCount);
    MMAutoLock locker(mLock);
    if (STATE_IDLE != mState) {
        MMLOGI("playing or play over, stop first\n");
        stop_l();
    }

    mFile = fopen(filename, "r");
    if (!mFile) {
        MMLOGE("Failed to open file: %s\n", filename);
        return MM_ERROR_INVALID_URI;
    }
#ifdef __AUDIO_CRAS__
    mm_status_t ret = createCrasClient(format, sampleRate, channelCount);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to create cras client\n");
        fclose(mFile);
        mFile = NULL;
        return ret;
    }
#elif defined(__AUDIO_PULSE__)
    if (createSample(format, sampleRate, channelCount, connectionId)) {
        fclose(mFile);
        mFile = NULL;
    }
    stop_play_thread = false;
    if(pthread_create(&play_thread, NULL, play_thread_func, this)!=0){
        fclose(mFile);
        mFile = NULL;
        MMLOGE("Failed to create thread: play thread.");
        return MM_ERROR_OP_FAILED;
    }
#endif
    setState(mState, STATE_PLAYING);
    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

void PCMPlayerIMP::pause()
{
    FUNC_ENTER();
    MMAutoLock locker(mLock);
    if (mState != STATE_PLAYING) {
        MMLOGE("invalid state: %d\n", mState);
        return;
    }

    setState(mState, STATE_PAUSED);

    FUNC_LEAVE();
}

void PCMPlayerIMP::resume()
{
    FUNC_ENTER();
    MMAutoLock locker(mLock);
    if (mState != STATE_PAUSED) {
        MMLOGE("invalid state: %d\n", mState);
        return;
    }

    setState(mState, STATE_PLAYING);

    FUNC_LEAVE();
}

void PCMPlayerIMP::stop()
{
    FUNC_ENTER();
    MMAutoLock locker(mLock);
    if (mState != STATE_PLAYING &&
        mState != STATE_PAUSED) {
        MMLOGE("invalid state: %d\n", mState);
        return;
    }

    stop_l();

    FUNC_LEAVE();
}

void PCMPlayerIMP::stop_l()
{
    FUNC_ENTER();

#ifdef __AUDIO_CRAS__
    if (!mRender) {
        MMLOGV("already stopped\n");
        return;
    }
    destroyCrasClient();
#elif defined(__AUDIO_PULSE__)
    stop_play_thread = true;
    pthread_join(play_thread, NULL);
    play_thread = 0;

    if (s) {
        pa_simple_free(s);
        s = NULL;
    }
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
   ENSURE_AUDIO_DEF_CONNECTION_CLEAN();
#endif
#endif

    if(mFile){
        fclose(mFile);
        mFile = NULL;
    }

    setState(mState, STATE_IDLE);
    FUNC_LEAVE();
}

}
