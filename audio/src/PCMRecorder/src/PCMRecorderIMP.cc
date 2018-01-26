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
#include "PCMRecorderIMP.h"
#include <util/Utils.h>

namespace YUNOS_MM {

MM_LOG_DEFINE_MODULE_NAME("PCMRecorderIMP");


#define USE_PA_STREAM_INTERPOLATE_TIMING
#define USE_PA_STREAM_ADJUST_LATENCY

#ifdef __AUDIO_PULSE__
static const char * CONTEXT_NAME = "PCMRecorder";
static const char * stream_name = "PCMRecorder";

static const struct format_entry {
    snd_format_t spformat;
    pa_sample_format_t pa;
} format_map[] = {
    {SND_FORMAT_PCM_16_BIT, PA_SAMPLE_S16LE},
    {SND_FORMAT_PCM_32_BIT, PA_SAMPLE_S32LE},
    {SND_FORMAT_PCM_8_BIT, PA_SAMPLE_U8},
    {SND_FORMAT_INVALID, PA_SAMPLE_INVALID}
};
#endif

#define FUNC_ENTER() do { MMLOGV("+\n"); }while(0)
#define FUNC_LEAVE() do { MMLOGV("-\n"); }while(0)

#define setState(_param, _deststate) do {\
    MMLOGI("change state from %d to %s\n", _param, #_deststate);\
    (_param) = (_deststate);\
}while(0)

PCMRecorderIMP::PCMRecorderIMP() :
                                    mCondition(mLock),
                                    mState(STATE_IDLE),
                                    mInputSouce(ADEV_SOURCE_DEFAULT),
                                    mFormat(SND_FORMAT_PCM_16_BIT),
                                    mSampleRate(0),
                                    mChannelCount(0),
                                    mChannelsMap(NULL),
#ifdef __AUDIO_CRAS__
                                    mCapture(NULL),
                                    mMinBufSize(0),
                                    mIsPaused(true),
#elif defined(__AUDIO_PULSE__)
                                    mPAContext(NULL),
                                    mPALoop(NULL),
                                    mPAStream(NULL),
                                    mPAReadData(NULL),
                                    mPAReadableSize(0),
                                    mPAReadIndex(0),
#endif
                                    mCorkResult(0)

{
    FUNC_ENTER();
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
   ENSURE_AUDIO_DEF_CONNECTION_INIT();
#endif
    FUNC_LEAVE();
}

PCMRecorderIMP::~PCMRecorderIMP()
{
    FUNC_ENTER();
    if ( mState != STATE_IDLE ) {
        reset_l();
    }
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
   ENSURE_AUDIO_DEF_CONNECTION_CLEAN();
#endif
    FUNC_LEAVE();
}

#ifdef __AUDIO_PULSE__
#define prepare_failed(_retcode, _info, _pa_err_no) do {\
        MMLOGE("%s, retcode: %d, pa: %s\n", _info, _retcode, pa_strerror(_pa_err_no));\
        reset_l();\
        return _retcode;\
}while(0)

pa_sample_format PCMRecorderIMP::convertFormatToPulse(snd_format_t format)
{
    for (int i = 0; format_map[i].spformat != SND_FORMAT_INVALID; ++i) {
        if (format_map[i].spformat == format)
            return format_map[i].pa;
    }
    return PA_SAMPLE_INVALID;
}

mm_status_t PCMRecorderIMP::createContext()
{
    FUNC_ENTER();

    int ret;
    pa_mainloop_api *api;

    /* Set up a new main loop */
    MMLOGV("newing pa thread main loop\n");
    mPALoop = pa_threaded_mainloop_new();
    if (mPALoop == NULL){
        prepare_failed(MM_ERROR_NO_MEM, "failed to get pa api", pa_context_errno(mPAContext));
    }

    api = pa_threaded_mainloop_get_api(mPALoop);
    pa_proplist *proplist = pa_proplist_new();
    if ( !proplist ) {
        prepare_failed(MM_ERROR_NO_MEM, "failed to new proplist", pa_context_errno(mPAContext));
    }
    ret = pa_proplist_sets(proplist, "log-backtrace", "10");
    if ( ret < 0 ) {
        pa_proplist_free(proplist);
        prepare_failed(MM_ERROR_NO_MEM, "failed to set proplist", ret);
    }

    mPAContext = pa_context_new_with_proplist(api, CONTEXT_NAME, proplist);
    pa_proplist_free(proplist);
    if (mPAContext == NULL) {
        prepare_failed(MM_ERROR_OP_FAILED, "failed to get pa api", pa_context_errno(mPAContext));
    }

    pa_context_set_state_callback(mPAContext, context_state_cb, this);

    /* Connect the context */
    MMLOGV("connecting pa context\n");
    ret = pa_context_connect(mPAContext, "127.0.0.1", PA_CONTEXT_NOFLAGS, NULL);
    if ( ret < 0) {
        prepare_failed(MM_ERROR_OP_FAILED, "failed to connect to context", ret);
    }

    MMLOGV("starting pa mainloop\n");
    ret = pa_threaded_mainloop_start(mPALoop);
    if(ret != 0){
        prepare_failed(MM_ERROR_OP_FAILED, "failed to start mainloop", ret);
    }

    mm_status_t result;
    MMLOGV("waitting context ready\n");
    pa_threaded_mainloop_lock(mPALoop);
    while ( 1 ) {
        pa_context_state_t state = pa_context_get_state (mPAContext);
        MMLOGD("now state: %d\n", state);

        if ( state == PA_CONTEXT_READY ) {
            MMLOGI("ready\n");
            result = MM_ERROR_SUCCESS;
            break;
        } else if ( state == PA_CONTEXT_TERMINATED || state == PA_CONTEXT_FAILED ) {
            MMLOGE("terminated or failed\n");
            result = MM_ERROR_OP_FAILED;
            break;
        }

        MMLOGV("not expected state, wait\n");
        pa_threaded_mainloop_wait (mPALoop);
    }
    pa_threaded_mainloop_unlock(mPALoop);

    return result;
}

/*static*/ void PCMRecorderIMP::context_state_cb(pa_context *c, void *userdata) {
    FUNC_ENTER();
    if (c == NULL) {
        MMLOGE("invalid param\n");
        return;
    }

    PCMRecorderIMP * me = static_cast<PCMRecorderIMP*>(userdata);
    MMASSERT(me);

    MMLOGV("state: %d\n", pa_context_get_state(c));

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
    FUNC_LEAVE();
}

mm_status_t PCMRecorderIMP::createStream(const char * audioConnectionId)
{
    FUNC_ENTER();
    MMLOGD("device: %s\n", audioConnectionId);
    int ret;
    uint32_t flags = PA_STREAM_AUTO_TIMING_UPDATE |
                                              PA_STREAM_START_CORKED;
#ifdef USE_PA_STREAM_INTERPOLATE_TIMING
    flags |= PA_STREAM_INTERPOLATE_TIMING;
#endif
#ifdef USE_PA_STREAM_ADJUST_LATENCY
    flags |= PA_STREAM_ADJUST_LATENCY;
#endif

    pa_sample_spec ss = {
        .format = convertFormatToPulse(mFormat),
        .rate = mSampleRate,
        .channels = mChannelCount
    };

    MMLOGV("newing stream\n");
    pa_proplist *pl = pa_proplist_new();
    std::string connectionIdStr = audioConnectionId;
#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
    ENSURE_AUDIO_DEF_CONNECTION_ENSURE(connectionIdStr, MMAMHelper::recordChnnelMic());
#endif
    pa_proplist_sets(pl, "connection_id", connectionIdStr.c_str());
    mPAStream = pa_stream_new_with_proplist(mPAContext, stream_name, &ss, NULL, pl);
    //mPAStream = pa_stream_new(mPAContext, stream_name, &ss, NULL);
    if (mPAStream == NULL) {
        prepare_failed(MM_ERROR_NO_MEM, "failed to new stream", pa_error_code());
    }

    pa_stream_set_state_callback(mPAStream, stream_state_cb, this);

    pa_buffer_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.maxlength = (uint32_t) -1;;
    attr.tlength = (uint32_t) -1;
    attr.prebuf = (uint32_t)-1;
    attr.minreq = (uint32_t) -1;
    attr.fragsize = (uint32_t) -1;
    ret = pa_stream_connect_record(mPAStream, NULL, &attr, (pa_stream_flags_t)flags);
    if( ret != 0 ){
        prepare_failed(MM_ERROR_OP_FAILED, "failed to connect to record", ret);
    }
    pa_stream_set_read_callback(mPAStream, stream_read_cb, this);

    pa_stream_set_latency_update_callback(mPAStream, stream_latency_cb, NULL);

    MMLOGD("waiting result\n");
    mm_status_t result;
    pa_threaded_mainloop_lock(mPALoop);
    while ( 1 ) {
        pa_stream_state_t state = pa_stream_get_state(mPAStream);

        if ( state == PA_STREAM_READY ) {
            MMLOGI("ready\n");
            result = MM_ERROR_SUCCESS;

            const pa_buffer_attr *actual_attr = pa_stream_get_buffer_attr (mPAStream);
            MMLOGD("maxlength=%d, tlength=%d, prebuf=%d, minreq=%d, fragsize=%d\n",
                                actual_attr->maxlength,
                                actual_attr->tlength,
                                actual_attr->prebuf,
                                actual_attr->minreq,
                                actual_attr->fragsize);
            break;
        }

        if ( state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED ) {
            MMLOGE("failed or terminated\n");
            result = MM_ERROR_OP_FAILED;
            break;
        }

        MMLOGV("not expected state, wait\n");
        pa_threaded_mainloop_wait(mPALoop);
    }
    pa_threaded_mainloop_unlock(mPALoop);

    MMLOGI("over\n");
    return result;
}

/*static */void PCMRecorderIMP::stream_latency_cb(pa_stream *p, void *userdata)
{
    FUNC_ENTER();
#ifndef USE_PA_STREAM_INTERPOLATE_TIMING
    pa_operation *o;

    o = pa_stream_update_timing_info(p, NULL, NULL);
    pa_operation_unref(o);
#endif
}

/*static */void PCMRecorderIMP::stream_state_cb(pa_stream *s, void * userdata) {
    FUNC_ENTER();
    if (s == NULL) {
        MMLOGE("invalid param\n");
        return;
    }

    PCMRecorderIMP * me = static_cast<PCMRecorderIMP*>(userdata);
    MMASSERT(me);

    MMLOGV("state: %d\n", pa_stream_get_state(s));
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(me->mPALoop, 0);
            break;
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

mm_status_t PCMRecorderIMP::cork(int b)
{
    MMLOGI("+");
    mCorkResult = 0;

    pa_threaded_mainloop_lock (mPALoop);
    pa_operation * op = pa_stream_cork(mPAStream, b, stream_cork_cb, this);
    if ( !op ) {
        MMLOGE("failed to cork: pa: %s\n", pa_strerror(pa_context_errno(mPAContext)));
        pa_threaded_mainloop_unlock (mPALoop);
        return MM_ERROR_OP_FAILED;
    }

    MMLOGV("waitting result\n");
    while ( pa_operation_get_state (op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait (mPALoop);
    }
    MMLOGI("result: %d\n", mCorkResult);
    pa_threaded_mainloop_unlock (mPALoop);
    pa_operation_unref(op);

    return mCorkResult > 0 ? MM_ERROR_SUCCESS : MM_ERROR_OP_FAILED;
}

/*static*/ void PCMRecorderIMP::stream_cork_cb(pa_stream*s, int success, void *userdata)
{
    FUNC_ENTER();
    PCMRecorderIMP * me = static_cast<PCMRecorderIMP*>(userdata);
    MMASSERT(me);
    me->mCorkResult = success ? 1 : -1;
    pa_threaded_mainloop_signal (me->mPALoop, 0);
    FUNC_LEAVE();
}

/*static*/ void PCMRecorderIMP::stream_read_cb(pa_stream *p, size_t nbytes, void *userdata)
{
    FUNC_ENTER();
    if (p == NULL) {
        MMLOGE("invalid param\n");
        return;
    }

    PCMRecorderIMP * me = static_cast<PCMRecorderIMP*>(userdata);
    MMASSERT(me);

    if ( nbytes == 0 ) {
        MMLOGV("nbytes: 0\n");
        return;
    }

    if (me->mListener) {
        MMLOGV("send EVENT_MORE_DATA to listener: %u\n", nbytes);
        me->mListener->onMessage(Listener::EVENT_MORE_DATA, nbytes, 0, 0);
    }
}
#endif

#ifdef __AUDIO_CRAS__
void PCMRecorderIMP::clearSourceBuffers()
{
    while(!mAvailableSourceBuffers.empty()) {
        mAvailableSourceBuffers.pop();
    }
}

/*static*/ bool PCMRecorderIMP::releaseInputBuffer(MediaBuffer* mediaBuffer)
{
    uint8_t *buffer = NULL;
    if (!(mediaBuffer->getBufferInfo((uintptr_t *)&buffer, NULL, NULL, 1))) {
        WARNING("error in release mediabuffer");
        return false;
    }
    MM_RELEASE_ARRAY(buffer);
    return true;
}

void PCMRecorderIMP::audioCallback(YunOSAudioNS::AudioCapture::evt_t evt, void* user, void *info)
{
    printf("callback received\n");
    PCMRecorderIMP *me = static_cast<PCMRecorderIMP *>(user);
    if (evt == YunOSAudioNS::AudioCapture::EVT_MORE_DATA) {
        MMAutoLock locker(me->mLock);
        if(me->mIsPaused) return;
        YunOSAudioNS::AudioCapture::Buffer *bufferReaded = static_cast<YunOSAudioNS::AudioCapture::Buffer *>(info);
        MediaBufferSP mediaBuf = MediaBuffer::createMediaBuffer(MediaBuffer::MBT_RawAudio);
        uint8_t *buffer = NULL;
        size_t readSize = 0;
        readSize = bufferReaded->size;
        buffer = new uint8_t[readSize];
        memcpy(buffer, bufferReaded->mData, readSize);
        mediaBuf->setBufferInfo((uintptr_t *)&buffer, NULL, (int32_t *)&readSize, 1);
        mediaBuf->setSize((int64_t)readSize);
        mediaBuf->addReleaseBufferFunc(releaseInputBuffer);
        me->mAvailableSourceBuffers.push(mediaBuf);

        if (me->mListener) {
            MMLOGV("send EVENT_MORE_DATA to listener: %u\n", readSize);
            me->mListener->onMessage(Listener::EVENT_MORE_DATA, readSize, 0, 0);
        }
        me->mCondition.signal();
    }
}

mm_status_t PCMRecorderIMP::createCrasClient()
{
    mMinBufSize = YunOSAudioNS::AudioCapture::getDefaultPeriodSize(
        mSampleRate,
        adev_get_in_mask_from_channel_count(mChannelCount),
        (snd_format_t)mFormat
        );
    mCapture = YunOSAudioNS::AudioCapture::create(
        ADEV_SOURCE_MIC,
        mSampleRate,
        (snd_format_t)mFormat,
        adev_get_in_mask_from_channel_count(mChannelCount),
        mMinBufSize,
        audioCallback,
        this
        );

    if (!mCapture) {
        ERROR("audio capture create fail");
        return MM_ERROR_OP_FAILED;
    }
    return MM_ERROR_SUCCESS;
}

mm_status_t PCMRecorderIMP::destroyCrasClient()
{
    if(mCapture)
        delete mCapture;
    return MM_ERROR_SUCCESS;
}
#endif

mm_status_t PCMRecorderIMP::prepare(adev_source_t inputSource,
                            snd_format_t format,
                            uint32_t sampleRate,
                            uint8_t channelCount,
                            adev_channel_mask_t * channelsMap,
                            const char * audioConnectionId/* = NULL*/)
{
    FUNC_ENTER();
    MMLOGD("inputSource: %d, format: %d, sampleRate:%u, channelCount: %d, channelsMap: %p",
        inputSource, format, sampleRate, channelCount, channelsMap);

    MMAutoLock locker(mLock);
    mInputSouce = inputSource;
    mFormat = format;
    mSampleRate = sampleRate;
    mChannelCount = channelCount;
    if ( channelsMap ) {
        MMLOGV("channelsMap provided\n");
        MMASSERT(mChannelsMap==NULL);
        mChannelsMap = new adev_channel_mask_t[channelCount];
        if ( !mChannelsMap ) {
            MMLOGE("no mem\n");
            setState(mState, STATE_ERROR);
            return MM_ERROR_NO_MEM;
        }
        memcpy(mChannelsMap, channelsMap, channelCount);
    } else {
        MMLOGV("channelsMap not provided\n");
        mChannelsMap = NULL;
    }
    mm_status_t ret = 0;

#ifdef __AUDIO_CRAS__
    ret = createCrasClient();
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to create cras client\n");
        setState(mState, STATE_ERROR);
        return ret;
    }
#elif defined(__AUDIO_PULSE__)
    ret = createContext();
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to create context\n");
        setState(mState, STATE_ERROR);
        return ret;
    }

    ret = createStream(audioConnectionId);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to create stream\n");
        setState(mState, STATE_ERROR);
        return ret;
    }
#endif

    setState(mState, STATE_PREPARED);
    return ret;
}

mm_status_t PCMRecorderIMP::start()
{
    MMLOGI("+");
    MMAutoLock locker(mLock);
    if ( mState != STATE_PREPARED && mState != STATE_PAUSED ) {
        MMLOGE("invalid state: %d\n", mState);
        return MM_ERROR_INVALID_STATE;
    }
    mm_status_t result = MM_ERROR_SUCCESS;
#ifdef __AUDIO_CRAS__
    mIsPaused = false;
    result = mCapture->start();
    if (result == STATUS_PERMISSION_DENIED) {
        mListener->onMessage(Listener::MSG_ERROR, MM_ERROR_PERMISSION_DENIED, 0, 0);
        setState(mState, STATE_ERROR);
        return result;
    }
#elif defined(__AUDIO_PULSE__)
    result = cork(0);
    if ( result != MM_ERROR_SUCCESS ) {
        setState(mState, STATE_ERROR);
        MMLOGE("failed to corck\n");
        return MM_ERROR_OP_FAILED;
    }
#endif

    setState(mState, STATE_STARTED);
    MMLOGI("-");
    return result;
}

mm_status_t PCMRecorderIMP::pause()
{
    MMLOGI("+");
    MMAutoLock locker(mLock);
    return pause_l();
}

mm_status_t PCMRecorderIMP::pause_l()
{
    MMLOGI("+\n");
    if ( mState != STATE_STARTED ) {
        MMLOGE("invalid state: %d\n", mState);
        return MM_ERROR_INVALID_STATE;
    }
    mm_status_t result = MM_ERROR_SUCCESS;
#ifdef __AUDIO_CRAS__
    mIsPaused = true;
    mCondition.signal();
    //mCapture->pause();//FIXME:need support by cras audio.
#elif defined(__AUDIO_PULSE__)
    result = cork(1);
    if ( result != MM_ERROR_SUCCESS ) {
        setState(mState, STATE_ERROR);
        MMLOGE("failed to corck\n");
        return MM_ERROR_OP_FAILED;
    }
#endif
    setState(mState, STATE_PAUSED);
    MMLOGI("-");
    return result;
}

size_t PCMRecorderIMP::read(void* buffer, size_t size)
{
    MMASSERT(buffer != NULL);
    MMAutoLock locker(mLock);
    if ( mState != STATE_STARTED ) {
        MMLOGE("invalid state: %d\n", mState);
        return 0;
    }

    MMLOGV("req size: %u\n", size);
#ifdef __AUDIO_CRAS__
    if (mAvailableSourceBuffers.empty()) {
            mCondition.wait();
    }
    if (mIsPaused) {
        printf("paused\n");
        return 0;
    }
    MediaBufferSP mediaBuffer;
    uint8_t *sourceBuf = NULL;
    int32_t offset = 0;
    int32_t bufferSize = 0;
    mediaBuffer = mAvailableSourceBuffers.front();
    mediaBuffer->getBufferInfo((uintptr_t*)&sourceBuf, &offset, &bufferSize, 1);
    memcpy(buffer, sourceBuf + offset, bufferSize);
    mAvailableSourceBuffers.pop();
    return bufferSize;
#elif defined(__AUDIO_PULSE__)
    pa_threaded_mainloop_lock(mPALoop);
    if ( !mPAReadData ) {
        MMASSERT(mPAReadableSize == 0);
        MMASSERT(mPAReadIndex == 0);
        MMLOGV("readable: %u\n", mPAReadableSize);
        while ( 1 ) {
            int ret = pa_stream_peek(mPAStream, (const void**)&mPAReadData, &mPAReadableSize);
            if (ret != 0) {
                mPAReadData = NULL;
                mPAReadableSize = 0;
                MMLOGE("read error: %s\n", pa_strerror(ret));
                pa_threaded_mainloop_unlock(mPALoop);
                return 0;
            }
            if ( mPAReadableSize == 0 ) {
                mPAReadData = NULL;
                MMLOGE("no data\n");
                pa_threaded_mainloop_unlock(mPALoop);
                return 0;
            }
            if ( !mPAReadData ) {
                MMLOGW("there is a hole.\n");
                pa_stream_drop(mPAStream);
                mPAReadableSize = 0;
                continue;
            }
            MMLOGV("got new readable size: %u", mPAReadableSize);
            break;
        }
    }

    size_t can_read = mPAReadableSize - mPAReadIndex;
    if ( size < can_read ) {
        memcpy(buffer, mPAReadData + mPAReadIndex, size);
        mPAReadIndex += size;
        MMLOGV("%d readed, drop next time\n", size);
        pa_threaded_mainloop_unlock(mPALoop);
        return size;
    }

    memcpy(buffer, mPAReadData + mPAReadIndex, can_read);
    int ret = pa_stream_drop(mPAStream);
    MMLOGV("droping\n");
    if (ret != 0) {
        MMLOGE("drop error: %s\n", pa_strerror(ret));
    }

    mPAReadData = NULL;
    mPAReadIndex = 0;
    mPAReadableSize = 0;

    MMLOGV("%d readed\n", can_read);
    pa_threaded_mainloop_unlock(mPALoop);
    return can_read;
#endif

}

mm_status_t PCMRecorderIMP::reset()
{
    MMLOGI("mState: %d\n", mState);
    {
        MMAutoLock locker(mLock);
        if (mState == STATE_IDLE) {
            MMLOGV("already idle\n");
            return MM_ERROR_SUCCESS;
        }
        if ( mState == STATE_STARTED ) {
            pause_l();
        }
    }
    reset_l();
    MMLOGI("-");
    return MM_ERROR_SUCCESS;
}

void PCMRecorderIMP::reset_l()
{
    MMLOGI("+");
    if ( mChannelsMap ) {
        MM_RELEASE_ARRAY(mChannelsMap);
    }
#ifdef __AUDIO_CRAS__

    mIsPaused = true;
    mCondition.signal();
    mCapture->stop();
    destroyCrasClient();
    clearSourceBuffers();
#elif defined(__AUDIO_PULSE__)
    if (mPALoop) {
        pa_threaded_mainloop_lock(mPALoop);
    }
    if (mPAStream) {
        pa_stream_disconnect(mPAStream);
        pa_stream_unref(mPAStream);
        mPAStream = NULL;
    }

    if (mPAContext) {
        pa_context_disconnect(mPAContext);
        pa_context_unref(mPAContext);
        mPAContext = NULL;
    }
    if (mPALoop) {
        pa_threaded_mainloop_unlock(mPALoop);
    }

    if (mPALoop) {
        pa_threaded_mainloop_stop(mPALoop);
        pa_threaded_mainloop_free(mPALoop);
        mPALoop = NULL;
    }
#endif

#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
   ENSURE_AUDIO_DEF_CONNECTION_CLEAN();
#endif

    setState(mState, STATE_IDLE);
    MMLOGI("-");
}

}
