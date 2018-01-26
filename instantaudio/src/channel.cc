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

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>
#include "channel.h"


namespace YUNOS_MM {

#define C_LOGE(_format, _arg...) MMLOGE("[%d]" _format, logid(), ##_arg)
#define C_LOGW(_format, _arg...) MMLOGW("[%d]" _format, logid(), ##_arg)
#define C_LOGI(_format, _arg...) MMLOGI("[%d]" _format, logid(), ##_arg)
#define C_LOGD(_format, _arg...) MMLOGD("[%d]" _format, logid(), ##_arg)
#define C_LOGV(_format, _arg...) MMLOGV("[%d]" _format, logid(), ##_arg)

DEFINE_LOGTAG(Channel)
DEFINE_LOGTAG(Channel::WriteThread)


#define INTERPOLATE
#define USE_PA_STREAM_INTERPOLATE_TIMING
#define USE_PA_STREAM_ADJUST_LATENCY

#define FUNC_ENTER() do { C_LOGV("+\n"); }while(0)
#define FUNC_LEAVE() do { C_LOGV("-\n"); }while(0)

#define setState(_param, _deststate) do {\
    C_LOGI("change state from %d to %s\n", _param, #_deststate);\
    (_param) = (_deststate);\
}while(0)

#ifdef __AUDIO_PULSE__
#define RELEASE_MAINLOOP() do {\
    if (mPALoop) {\
        pa_threaded_mainloop_free(mPALoop);\
        mPALoop = NULL;\
    }\
}while(0)

#define RELEASE_CONTEXT() do {\
    if (mPAContext) {\
        pa_context_unref(mPAContext);\
        mPAContext = NULL;\
    }\
}while(0)

/*static*/ pa_sample_format_t Channel::format_convert(snd_format_t format)
{
    pa_sample_format_t format_to;
    switch(format) {
        case SND_FORMAT_PCM_8_BIT:
            format_to = PA_SAMPLE_U8;
            break;
        case SND_FORMAT_PCM_16_BIT:
            format_to = PA_SAMPLE_S16LE;
            break;
        case SND_FORMAT_PCM_32_BIT:
            format_to = PA_SAMPLE_S32LE;
            break;
        default:
            format_to = PA_SAMPLE_S16LE;
            break;
    }
    return format_to;
}
#endif
static const char * MMTHREAD_NAME = "Channel::WriteThread";


Channel::Channel(int id, play_competed_cb playCompletedCB, void * cbUserData):
        mId(id),
        mSample(NULL),
        mLoop(0),
        mCurrentLoop(0),
        mWriteIndex(0),
        mRate(1),
        mPriority(0),
        mPlayCompletedCB(playCompletedCB),
        mPlayCompletedCbUserParam(cbUserData),
#ifdef __AUDIO_CRAS__
        mRender(NULL),
        mBytesPerSample(2),
        mAudioStreamType(AS_TYPE_SYSTEM),
#elif defined(__AUDIO_PULSE__)
        mPAContext(NULL),
        mPALoop(NULL),
        mPAStream(NULL),
        mLatency(0),
        mCorkResult(0),
#endif
        mWriteThread(NULL)
{
    mVolume.left = 0.0f;
    mVolume.right = 0.0f;
    memset(mStreamName, 0, STREAM_NAME_SIZE);
    snprintf(mStreamName, STREAM_NAME_SIZE - 1, "SoundC_%d", id);
    C_LOGV("name: %s\n", mStreamName);

#ifdef __AUDIO_PULSE__
    mm_status_t ret = 0;

    ret = createContext();
    if ( ret != MM_ERROR_SUCCESS ) {
        C_LOGE("failed to create context\n");
        setState(mState, STATE_ERROR);
        return;
    }
#endif

#ifdef __AUDIO_PULSE__
    mWriteThread = new WriteThread(this);
    if ( !mWriteThread ) {
        C_LOGE("no mem\n");
        setState(mState, STATE_ERROR);
        destroyContext();
        return;
    }
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_INIT();
#endif
#endif
    setState(mState, STATE_PREPARED);
}

Channel::~Channel(){
    FUNC_ENTER();

    C_LOGV("cur state: %d\n", mState);
    switch ( mState ) {
        case STATE_STARTED:
        case STATE_PAUSED:
            C_LOGV("stop first\n");
            doStop();
            MMASSERT(mState == STATE_PREPARED);
            break;
        case STATE_PREPARED:
        case STATE_ERROR:
        default:
            break;
    }

#ifdef __AUDIO_PULSE__
    MM_RELEASE(mWriteThread);
    destroyContext();
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_CLEAN();
#endif
#endif
    FUNC_LEAVE();
}

#ifdef __AUDIO_PULSE__
mm_status_t Channel::createContext()
{
    FUNC_ENTER();
    int ret;
    pa_mainloop_api *api;

    /* Set up a new main loop */
    C_LOGV("newing pa thread main loop\n");
    mPALoop = pa_threaded_mainloop_new();
    if (mPALoop == NULL){
        C_LOGE("failed to new main loop\n");
        return MM_ERROR_NO_MEM;
    }

    api = pa_threaded_mainloop_get_api(mPALoop);
    pa_proplist *proplist = pa_proplist_new();
    if ( !proplist ) {
        C_LOGE("failed to new proplist\n");
        return MM_ERROR_NO_MEM;
    }
    ret = pa_proplist_sets(proplist, "set-log-level", "4");
    ret = pa_proplist_sets(proplist, "log-backtrace", "10");
    if ( ret < 0 ) {
        pa_proplist_free(proplist);
        RELEASE_MAINLOOP();
        return MM_ERROR_NO_MEM;
    }

    mPAContext = pa_context_new_with_proplist(api, mStreamName, proplist);
    pa_proplist_free(proplist);
    if (mPAContext == NULL) {
        C_LOGE("failed to new context\n");
        RELEASE_MAINLOOP();
        return MM_ERROR_NO_MEM;
    }

    pa_context_set_state_callback(mPAContext, context_state_cb, this);

    /* Connect the context */
    C_LOGV("connecting pa context\n");
    ret = pa_context_connect(mPAContext, "127.0.0.1", PA_CONTEXT_NOFLAGS, NULL);
    if ( ret < 0) {
        C_LOGE("failed to connect context: %s\n", pa_strerror(pa_context_errno(mPAContext)));
        RELEASE_CONTEXT();
        RELEASE_MAINLOOP();
        return MM_ERROR_OP_FAILED;
    }

    C_LOGD("starting pa mainloop\n");
    ret = pa_threaded_mainloop_start(mPALoop);
    if(ret != 0){
        C_LOGE("failed to start mainloop: %s\n", pa_strerror(pa_context_errno(mPAContext)));
        pa_context_disconnect(mPAContext);
        RELEASE_CONTEXT();
        RELEASE_MAINLOOP();
        return MM_ERROR_OP_FAILED;
    }

    mm_status_t result;
    C_LOGD("waitting context ready\n");
    pa_threaded_mainloop_lock(mPALoop);
    while ( 1 ) {
        pa_context_state_t state = pa_context_get_state (mPAContext);
        C_LOGD("now state: %d\n", state);

        if ( state == PA_CONTEXT_READY ) {
            C_LOGI("ready\n");
            result = MM_ERROR_SUCCESS;
            break;
        } else if ( state == PA_CONTEXT_TERMINATED || state == PA_CONTEXT_FAILED ) {
            C_LOGE("terminated or failed\n");
            result = MM_ERROR_OP_FAILED;
            break;
        }

        C_LOGD("not expected state, wait\n");
        pa_threaded_mainloop_wait (mPALoop);
    }
    pa_threaded_mainloop_unlock(mPALoop);

    if ( result != MM_ERROR_SUCCESS ) {
        pa_context_disconnect(mPAContext);
        RELEASE_CONTEXT();
        pa_threaded_mainloop_stop(mPALoop);
        RELEASE_MAINLOOP();
    }
    FUNC_LEAVE();
    return result;
}

void Channel::destroyContext()
{
    do {
        if ( mPAContext ) {
            C_LOGI("disconnecting context\n");
            if ( !mPALoop ) {
                C_LOGW("no main loop\n");
                pa_context_disconnect(mPAContext);
                break;
            }

            pa_threaded_mainloop_lock(mPALoop);
            pa_context_disconnect(mPAContext);

            while ( 1 ) {
                pa_context_state_t state = pa_context_get_state(mPAContext);
                C_LOGI("now context state: %d\n", state);

                if ( state == PA_CONTEXT_TERMINATED
                    || state == PA_CONTEXT_FAILED ) {
                    C_LOGE("context terminated or failed\n");
                    break;
                }

                C_LOGI("not expected state, wait\n");
                pa_threaded_mainloop_wait (mPALoop);
            }
            pa_threaded_mainloop_unlock(mPALoop);
        }
    }while (0);

    if ( mPALoop ) {
        pa_threaded_mainloop_stop(mPALoop);
    }

    RELEASE_CONTEXT();
    RELEASE_MAINLOOP();
}

mm_status_t Channel::cork(int b)
{
    FUNC_ENTER();
    mCorkResult = 0;

    pa_threaded_mainloop_lock (mPALoop);
    pa_operation * op = pa_stream_cork(mPAStream, b, stream_cork_cb, this);
    if ( !op ) {
        C_LOGE("failed to cork: pa: %s\n", pa_strerror(pa_context_errno(mPAContext)));
        pa_threaded_mainloop_unlock (mPALoop);
        return MM_ERROR_OP_FAILED;
    }

    C_LOGD("waitting result\n");
    while ( pa_operation_get_state (op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait (mPALoop);
    }
    C_LOGI("result: %d\n", mCorkResult);
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock (mPALoop);
    FUNC_LEAVE();
    return mCorkResult > 0 ? MM_ERROR_SUCCESS : MM_ERROR_OP_FAILED;
}

void Channel::stream_cork_cb(pa_stream*s, int success, void *userdata){
    MMLOGV("+\n");
    Channel *me = static_cast<Channel*>(userdata);
    MMASSERT(me);
    MMLOGV("+id: %d\n", me->id());
    me->mCorkResult = success ? 1 : -1;
    pa_threaded_mainloop_signal (me->mPALoop, 0);
    MMLOGV("-\n");
}

void Channel::stream_write_cb(pa_stream *p, size_t nbytes, void *userdata){
//    MMLOGV("+\n");
    Channel *me = static_cast<Channel*>(userdata);
    MMASSERT(me);
//    MMLOGV("+id: %d\n", me->id());
    me->mWriteThread->write();
//    MMLOGV("-\n");
}

void Channel::stream_latency_cb(pa_stream *p, void *userdata) {
//    MMLOGV("+\n");
#ifndef INTERPOLATE
    pa_operation *o;
    o = pa_stream_update_timing_info(p, NULL, NULL);
    pa_operation_unref(o);
#endif
}

void Channel::stream_state_cb(pa_stream *s, void *userdata){
    MMLOGV("+\n");
    if (s == NULL) {
        MMLOGE("invalid param\n");
        return;
    }

    Channel *me = static_cast<Channel*>(userdata);
    MMASSERT(me);

    MMLOGD("id: %d, state: %d\n", me->id(), pa_stream_get_state(s));
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            MMLOGV("id: %d, signal pa loop\n", me->id());
            pa_threaded_mainloop_signal(me->mPALoop, 0);
            break;
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

mm_status_t Channel::createStream(const Sample *sample, const char * connectionId)
{
    FUNC_ENTER();
    int ret;
    uint32_t flags = PA_STREAM_AUTO_TIMING_UPDATE;
#ifdef USE_PA_STREAM_INTERPOLATE_TIMING
    flags |= PA_STREAM_INTERPOLATE_TIMING;
#endif
#ifdef USE_PA_STREAM_ADJUST_LATENCY
    flags |= PA_STREAM_ADJUST_LATENCY;
#endif

    audio_sample_spec * audioSample = (audio_sample_spec *)sample->sampleSpec();
    if(!audioSample) {
        C_LOGE("sampleSpec invalid\n");
        return MM_ERROR_OP_FAILED;
    }

    pa_sample_spec ss;
    ss.channels = audioSample->channels;
    ss.format = format_convert(audioSample->format);
    ss.rate = audioSample->rate;

    uint32_t sampleRate =uint32_t( ss.rate * mRate +0.5);
    ss.rate = sampleRate;
    C_LOGD("newing 1 stream ,sampleRate = %u\n",sampleRate);
    pa_threaded_mainloop_lock(mPALoop);
    pa_proplist *pl = pa_proplist_new();
    std::string connectionIdStr(connectionId);
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_ENSURE(connectionIdStr, MMAMHelper::playChnnelMain());
#endif
    pa_proplist_sets(pl, "connection_id", connectionIdStr.c_str());
    mPAStream = pa_stream_new_with_proplist(mPAContext, mStreamName, &ss, NULL, pl);
    //mPAStream = pa_stream_new(mPAContext, mStreamName, &ss, NULL);
    if (mPAStream == NULL) {
        C_LOGE("failed to new stream, %s\n", pa_strerror(pa_error_code()));
        pa_threaded_mainloop_unlock(mPALoop);
        return MM_ERROR_NO_MEM;
    }

    pa_stream_set_state_callback(mPAStream, stream_state_cb, this);

    pa_buffer_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.maxlength = (uint32_t) -1;;
    attr.tlength = (uint32_t) -1;
    attr.prebuf = (uint32_t)-1;
    attr.minreq = (uint32_t) -1;
    attr.fragsize = (uint32_t) -1;
    ret = pa_stream_connect_playback(mPAStream, NULL, &attr, (pa_stream_flags_t)flags, NULL, NULL);
    if ( ret != 0 ){
        C_LOGE("failed to connect stream: %s\n", pa_strerror(pa_error_code()));
        pa_stream_unref(mPAStream);
        mPAStream = NULL;
        return MM_ERROR_OP_FAILED;
    }
    pa_stream_set_write_callback(mPAStream, stream_write_cb, this);

    pa_stream_set_latency_update_callback(mPAStream, stream_latency_cb, NULL);

    C_LOGD("waiting result\n");
    mm_status_t result;
    while ( 1 ) {
        pa_stream_state_t state = pa_stream_get_state(mPAStream);
            C_LOGI("get state = %d\n", state);

        if ( state == PA_STREAM_READY ) {
            C_LOGI("ready\n");
            result = MM_ERROR_SUCCESS;

            const pa_buffer_attr *actual_attr = pa_stream_get_buffer_attr (mPAStream);
            C_LOGD("maxlength=%d, tlength=%d, prebuf=%d, minreq=%d, fragsize=%d\n",
                                actual_attr->maxlength,
                                actual_attr->tlength,
                                actual_attr->prebuf,
                                actual_attr->minreq,
                                actual_attr->fragsize);
            break;
        }

        if ( state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED ) {
            C_LOGE("failed or terminated\n");
            result = MM_ERROR_OP_FAILED;
            break;
        }

        C_LOGD("not expected state, wait, stream = %p ,mPALoop =%p\n",mPAStream ,mPALoop);
        pa_threaded_mainloop_wait(mPALoop);
    }

    if ( result != MM_ERROR_SUCCESS ){
        C_LOGE("failed to create stream: %s\n", pa_strerror(pa_error_code()));
        pa_stream_disconnect(mPAStream);
        pa_stream_unref(mPAStream);
        mPAStream = NULL;
        return MM_ERROR_OP_FAILED;
    }

    pa_threaded_mainloop_unlock(mPALoop);

    C_LOGI("over, result: %d\n", result);
    FUNC_LEAVE();
    return result;
}

void Channel::destroyStream()
{
    MMLOGV("destoying stream\n");
    if ( !mPAStream ) {
        C_LOGV("no stream to release\n");
        return;
    }

    pa_threaded_mainloop_lock(mPALoop);
    pa_stream_disconnect(mPAStream);
    pa_stream_unref(mPAStream);
    mPAStream = NULL;
    pa_threaded_mainloop_unlock(mPALoop);
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_CLEAN();
#endif
}

void Channel::context_state_cb(pa_context *c, void *userdata) {
    MMLOGV("+\n");
    if (c == NULL) {
        MMLOGE("invalid param\n");
        return;
    }

    Channel * me = static_cast<Channel*>(userdata);
    MMASSERT(me);

    MMLOGD("id: %d, state: %d\n", me->id(), pa_context_get_state(c));

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal (me->mPALoop, 0);
            break;
        default:
            break;
    }
    MMLOGV("\n");
}

void Channel::success_context_cb(pa_context *c, int success, void *userdata) {
    Channel *me = static_cast<Channel*>(userdata);
    MMASSERT(me);
    pa_threaded_mainloop_signal(me->mPALoop, 0);
}

/*static */void Channel::drain_cb(pa_stream *s, int success, void *userdata)
{
//    Channel * me = static_cast<Channel*>(userdata);
//    MMASSERT(me != NULL);
//    MMLOGV("id: %d, success: %d\n", me->id(), success);
//    pa_threaded_mainloop_signal(me->mPALoop, 0);
//    MMLOGV("id: %d, success: %d, signal over\n", me->id(), success);
}

void Channel::drain()
{
    C_LOGV("calling drain\n");
    pa_operation *o = pa_stream_drain(mPAStream, drain_cb, this);
    if ( o ) {
        /*C_LOGV("drain call over, wait\n");
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            C_LOGV("pa_operation_get_state: %d\n", pa_operation_get_state(o));
            pa_threaded_mainloop_wait(mPALoop);
        }
        C_LOGV("drain wait over\n");*/
        pa_operation_unref(o);
    } else {
        C_LOGV("calling drain failed: %s\n", pa_strerror(pa_error_code()));
    }
}

#endif//pulse

mm_status_t Channel::setVolume(const InstantAudio::VolumeInfo *volume) {
    FUNC_ENTER();
#ifdef __AUDIO_CRAS__
    mRender->setVolume(volume->left >= volume->right ? volume->left :volume->right);
#elif defined(__AUDIO_PULSE__)
    pa_operation *o = NULL;
    uint32_t idx;
    pa_cvolume cv;
    pa_threaded_mainloop_lock(mPALoop);

    idx = pa_stream_get_index (mPAStream);
    pa_volume_t setv = (pa_volume_t)((float)65535 * (volume->left >= volume->right ? volume->left :volume->right));//set the max volume of right and left
    pa_cvolume_set(&cv, 2, setv);
    MMLOGV("volume: %u, PA_VOLUME_NORM: %u, left: %f, right: %f\n", setv, PA_VOLUME_NORM, volume->left, volume->right);
    o = pa_context_set_sink_input_volume (mPAContext, idx, &cv, success_context_cb, this);

    if ( o ) {
        C_LOGV("set volume call over, wait\n");
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(mPALoop);
        }
        C_LOGV("wait over\n");
        pa_operation_unref(o);
    } else {
        C_LOGE("set volume failed\n");
    }

    o = pa_context_set_sink_input_mute (mPAContext, idx, 0, success_context_cb, this);

    //
    if ( o ) {
        C_LOGV("set no mute over, wait\n");
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(mPALoop);
        }
        C_LOGV("wait over\n");
        pa_operation_unref(o);
    } else {
        C_LOGE("set no mute failed\n");
    }

    pa_threaded_mainloop_unlock(mPALoop);
#endif
    C_LOGV("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::play(const Sample *sample, const InstantAudio::VolumeInfo *volume, int loop, float rate, int streamType){
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    if ( mState != STATE_PREPARED ) {
        C_LOGE("invalid state: %d\n", mState);
        return MM_ERROR_INVALID_STATE;
    }
#ifdef __AUDIO_PULSE__
    MMASSERT(mPAStream == NULL);
#endif
    mCurrentLoop = 0;
    mWriteIndex = 0;
    mm_status_t ret = 0;

    mSample = (Sample *)sample;
    memcpy(&mVolume, volume, sizeof(InstantAudio::VolumeInfo));
    mLoop = loop;
    mRate = rate;
#ifdef __AUDIO_CRAS__
    mAudioStreamType = streamType;
    ret = createCrasClient();
    if ( ret != MM_ERROR_SUCCESS ) {
        C_LOGE("failed to create context\n");
        setState(mState, STATE_ERROR);
        return ret;
    }

    ret = mRender->start();
    if (0 != ret) {
        C_LOGE("Failed to start AudioRender: %d.\n", ret);
        destroyCrasClient();
        return MM_ERROR_OP_FAILED;
    }
#elif defined(__AUDIO_PULSE__)
    ret = createStream(sample, NULL);
    if ( ret != MM_ERROR_SUCCESS ) {
        C_LOGE("failed to create stream\n");
        setState(mState, STATE_ERROR);
        return ret;
    }
#endif

    C_LOGI("setVolume \n");
    setVolume(volume);

#ifdef __AUDIO_PULSE__
    if ( !mWriteThread->start() ) {
        C_LOGE("failed to start write thread\n");
        destroyStream();
        setState(mState, STATE_ERROR);
        return false;
    }
#endif

    setState(mState, STATE_STARTED);
#ifdef __AUDIO_PULSE__
    mWriteThread->write();
#endif
    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::play(const Sample * sample, const InstantAudio::VolumeInfo * volume, int loop, float rate, const char * connectionId)
{
#ifdef __AUDIO_PULSE__
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    if ( mState != STATE_PREPARED ) {
        C_LOGE("invalid state: %d\n", mState);
        return MM_ERROR_INVALID_STATE;
    }
    mCurrentLoop = 0;
    mWriteIndex = 0;
    mm_status_t ret = 0;

    mSample = (Sample *)sample;
    memcpy(&mVolume, volume, sizeof(InstantAudio::VolumeInfo));
    mLoop = loop;
    mRate = rate;

    ret = createStream(sample, connectionId);
    if ( ret != MM_ERROR_SUCCESS ) {
        C_LOGE("failed to create stream\n");
        setState(mState, STATE_ERROR);
        return ret;
    }

    C_LOGI("setVolume \n");
    setVolume(volume);

    if ( !mWriteThread->start() ) {
        C_LOGE("failed to start write thread\n");
        destroyStream();
        setState(mState, STATE_ERROR);
        return false;
    }

    setState(mState, STATE_STARTED);
    mWriteThread->write();
    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
#else
    return MM_ERROR_UNSUPPORTED;
#endif
}

mm_status_t Channel::pause(){
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    MMLOGV("got lock\n");
    if ( mState != STATE_STARTED ) {
        C_LOGE("invalid state: %d\n", mState);
        return MM_ERROR_INVALID_STATE;
    }
#ifdef __AUDIO_CRAS__
    mRender->pause();
#elif defined(__AUDIO_PULSE__)
    mm_status_t result = cork(1);
    if ( result != MM_ERROR_SUCCESS ) {
        setState(mState, STATE_ERROR);
        C_LOGE("failed to corck\n");
        return MM_ERROR_OP_FAILED;
    }
#endif

    setState(mState, STATE_PAUSED);
    C_LOGV("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::resume(){
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    if ( mState != STATE_PAUSED ) {
        C_LOGE("invalid state: %d\n", mState);
        return MM_ERROR_INVALID_STATE;
    }
#ifdef __AUDIO_CRAS__
    int ret = mRender->start();
    if (0 != ret) {
        C_LOGE("Failed to start AudioRender: %d.\n", ret);
        destroyCrasClient();
        return MM_ERROR_OP_FAILED;
    }

#elif defined(__AUDIO_PULSE__)
    mm_status_t result = cork(0);
    if ( result != MM_ERROR_SUCCESS ) {
        setState(mState, STATE_ERROR);
        C_LOGE("failed to corck\n");
        return MM_ERROR_OP_FAILED;
    }
#endif

    setState(mState, STATE_STARTED);
    C_LOGV("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::stop(){
    FUNC_ENTER();
    return doStop();
}

mm_status_t Channel::doStop()
{
    C_LOGV("+\n");
    MMAutoLock lock(mLock);
    if ( mState != STATE_STARTED && mState != STATE_PAUSED ) {
        C_LOGD("state: %d\n", mState);
        return MM_ERROR_SUCCESS;
    }
    C_LOGV("stopping write thread\n");
#ifdef __AUDIO_PULSE__
    mWriteThread->stop();
#endif
#ifdef __AUDIO_CRAS__
    mRender->stop();
    destroyCrasClient();
#elif defined(__AUDIO_PULSE__)
    C_LOGV("destroying stream\n");
    destroyStream();
#endif
    mCurrentLoop = 0;
    mWriteIndex = 0;

    setState(mState, STATE_PREPARED);
    C_LOGV("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::setRate(float rate){
    mRate  = rate;
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::setPriority(int priority){
    mPriority = priority;
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::setLoop(int loop){
    mLoop = loop;
    return MM_ERROR_SUCCESS;
}

int Channel::write()
{
    FUNC_ENTER();

    MMASSERT(mWriteIndex <= mSample->size());

#ifdef __AUDIO_PULSE__
    pa_threaded_mainloop_lock(mPALoop);
    size_t pa_available_size = pa_stream_writable_size(mPAStream);
    if ( pa_available_size == 0 ) {
        C_LOGV("no space\n");
        return 0;
    }
    C_LOGD("pa_stream_writable_size: %ld\n", pa_available_size);

    size_t totalWritten = 0;
    while ( totalWritten < pa_available_size ) {
        size_t bufLeft = mSample->size() - mWriteIndex;
        size_t remain = pa_available_size - totalWritten;
        size_t toWrite = remain >= bufLeft ? bufLeft : remain;
        C_LOGV("writting to pa, sample size: %u, mWriteIndex: %u, bufLeft: %u, pa_available_size: %u, totalWritten: %u, remain: %u, toWrite: %u\n",
            mSample->size(),
            mWriteIndex,
            bufLeft,
            pa_available_size,
            totalWritten,
            remain,
            toWrite);
        if(pa_stream_write(mPAStream, mSample->data() + mWriteIndex , toWrite, NULL, 0, (pa_seek_mode_t)0) != 0) {
            C_LOGE("pa_stream_write error\n");
            drain();
            pa_threaded_mainloop_unlock(mPALoop);
            mPlayCompletedCB(id(), MM_ERROR_OP_FAILED, mPlayCompletedCbUserParam);
            C_LOGV("-\n");
            return -1;
        }
        totalWritten += toWrite;
        C_LOGV("%d bytes written, totalWritten: %u\n", toWrite, totalWritten);

        if ( remain >= bufLeft ) {
            mWriteIndex = 0;
            if ( ++mCurrentLoop > mLoop ) {
                C_LOGI("loop over, notify, cur: %d, total: %d\n", mCurrentLoop, mLoop);
                drain();
                pa_threaded_mainloop_unlock(mPALoop);
                mPlayCompletedCB(id(), MM_ERROR_SUCCESS, mPlayCompletedCbUserParam);
                C_LOGV("-\n");
                return -1;
            }
        } else {
            mWriteIndex += remain;
        }
    }

    pa_threaded_mainloop_unlock(mPALoop);
    C_LOGV("-\n");
    return pa_available_size;
#else
    return -1;
#endif
}

#ifdef __AUDIO_CRAS__
void Channel::audioCallback(YunOSAudioNS::AudioRender::evt_t event, void * user,void * info) {
    Channel *me = static_cast<Channel *>(user);

    if (YunOSAudioNS::AudioRender::EVT_MORE_DATA == event) {
        me->onMoreData(event, info);
    } else if (YunOSAudioNS::AudioRender::EVT_STREAM_ERROR == event) {
        me->onStreamError(event, info);
    } else {
        printf("audio callback event %d not implement.", event);
    }
}

void Channel::onMoreData(YunOSAudioNS::AudioRender::evt_t event, void *info) {
    int32_t remainSize = 0;

    if (YunOSAudioNS::AudioRender::EVT_MORE_DATA != event) {
        printf("%s failed: called in invalid event type %d.\n", __func__, event);
    }

    YunOSAudioNS::AudioRender::Buffer *buffer = static_cast<YunOSAudioNS::AudioRender::Buffer *>(info);
    int mWriteableSize = buffer->mFrameCount * mSample->sampleSpec()->channels * mBytesPerSample;
    buffer->mFrameCount = 0;
#if defined (__MM_YUNOS_CNTRHAL_BUILD__) && !(__PLATFORM_TV__)
#if MM_USE_AUDIO_VERSION>=20
    buffer->mIsEOS = false;
#endif
#endif
    if (mWriteIndex >= mSample->size() || mSample->size() <= 0) {
        return;
    }

    remainSize = mSample->size() - mWriteIndex;
    if (mWriteableSize >= remainSize) {
        memcpy(buffer->mBuffer , mSample->data() + mWriteIndex, remainSize);
        buffer->mFrameCount += remainSize / (mSample->sampleSpec()->channels * mBytesPerSample);
        mWriteableSize -= remainSize;
        mWriteIndex += remainSize;
    } else {
        memcpy(buffer->mBuffer , mSample->data() + mWriteIndex, mWriteableSize);
        buffer->mFrameCount += mWriteableSize / (mSample->sampleSpec()->channels * mBytesPerSample);
        mWriteIndex += mWriteableSize;
    }

    if (mWriteIndex >= mSample->size()) {
        mWriteIndex = 0;
        if ( ++mCurrentLoop > mLoop ) {
#if defined (__MM_YUNOS_CNTRHAL_BUILD__) && !(__PLATFORM_TV__)
#if MM_USE_AUDIO_VERSION>=20
            buffer->mIsEOS = true;
#endif
#endif
            C_LOGI("loop over, notify, cur: %d, total: %d\n", mCurrentLoop, mLoop);
            mPlayCompletedCB(id(), MM_ERROR_SUCCESS, mPlayCompletedCbUserParam);
            C_LOGV("-\n");
            return;
        }
        C_LOGV("-\n");
    }

}

void Channel::onStreamError(YunOSAudioNS::AudioRender::evt_t event, void *info)
{
    C_LOGE("audio stream error, audio stream has been deleted.\n");
    mPlayCompletedCB(id(), MM_ERROR_SUCCESS, mPlayCompletedCbUserParam);
}

mm_status_t Channel::createCrasClient()
{
    uint32_t outFrameCnt = YunOSAudioNS::AudioRender::getDefaultPeriodSize(
        mSample->sampleSpec()->rate,
        adev_get_out_mask_from_channel_count(mSample->sampleSpec()->channels),
        mSample->sampleSpec()->format,
        (as_type_t)mAudioStreamType);
    mRender = YunOSAudioNS::AudioRender::create((as_type_t)mAudioStreamType,
        mSample->sampleSpec()->rate,
        mSample->sampleSpec()->format,
        adev_get_out_mask_from_channel_count(mSample->sampleSpec()->channels),
        outFrameCnt,
        ADEV_OUTPUT_FLAG_NONE,
        audioCallback,
        this);
    mBytesPerSample = adev_bytes_per_sample((snd_format_t)mSample->sampleSpec()->format);

    if (!mRender) {
        ERROR("audio reander create fail");
        return MM_ERROR_OP_FAILED;
    }
    return MM_ERROR_SUCCESS;
}

mm_status_t Channel::destroyCrasClient()
{
    if(mRender)
        delete mRender;
    return MM_ERROR_SUCCESS;
}

#endif

Channel::WriteThread::WriteThread(Channel * watcher) : MMThread(MMTHREAD_NAME), mWather(watcher), mContinue(true)
{
    C_LOGV("+\n");
    sem_init(&mSem, 0, 0);
}

Channel::WriteThread::~WriteThread()
{
    C_LOGV("+\n");
    sem_destroy(&mSem);
}

bool Channel::WriteThread::start()
{
    C_LOGV("+\n");
    mContinue = true;
    MMASSERT(!created());
    return create() == 0;
}

void Channel::WriteThread::stop()
{
    C_LOGV("+\n");
    mContinue = false;
    sem_post(&mSem);
    destroy();
    C_LOGV("-\n");
}

void Channel::WriteThread::write()
{
//    C_LOGV("+\n");
    sem_post(&mSem);
}

void Channel::WriteThread::main()
{
    C_LOGI("+\n");
    while ( mContinue ) {
        C_LOGV("watting sem\n");
        sem_wait(&mSem);
        C_LOGV("watting sem over\n");
        if ( !mContinue ) {
            break;
        }
        while ( mContinue ) {
            C_LOGV("calling write\n");
            int ret = mWather->write();
            if ( ret == 0 )
                break;
            if ( ret == -1 ) {
                C_LOGI("write ret -1, ret\n");
                return;
            }
        }
    }
    C_LOGD("-\n");
}

}

