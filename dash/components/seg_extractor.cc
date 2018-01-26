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
#include <math.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#include "seg_extractor.h"

#include <multimedia/av_ffmpeg_helper.h>
#include <unistd.h>
#include <multimedia/av_buffer_helper.h>
#include <multimedia/media_attr_str.h>
#include <multimedia/mm_types.h>
#include <multimedia/mm_debug.h>
#include "third_helper.h"

#include <multimedia/mm_debug.h>

using namespace YUNOS_MM::YUNOS_DASH;
using namespace dash;
using namespace dash::mpd;

namespace YUNOS_MM {

#define DEBUG_AVIO

#define MSG_PREPARE (MMMsgThread::msg_type)1
#define MSG_READ_SAMPLE (MMMsgThread::msg_type)2

#define INVALID_TIME_STAMP (-0xfffffff)

#define SET_STATE(_state) do {\
    INFO("state changed from %d to %s\n", mState, #_state);\
    mState = _state;\
}while(0)

#define CHECK_STATE(_expected_state) do {\
    if ( mState != _expected_state ) {\
        ERROR("invalid state(%d). expect %s\n", mState, #_expected_state);\
        return MM_ERROR_INVALID_STATE;\
    }\
}while(0)

#define RET_IF_ALREADY_STATE(_state, _ret) do {\
    if ( mState == (_state) ) {\
        ERROR("already in state(%s). ret %s\n", #_state, #_ret);\
        return (_ret);\
    }\
}while(0)

#define NOTIFY(_event, _param1, _param2, _obj) do {\
    INFO("notify: %s, param1: %d, param2: %d\n", #_event, (_param1), (_param2));\
    notify(_event, _param1, _param2, _obj);\
}while(0)

#define NOTIFY_ERROR(_reason) do {\
    VERBOSE("notify: error: %d, %s\n", _reason, #_reason);\
    NOTIFY(kEventError, _reason, 0, nilParam);\
}while(0)

#define PREPARE_FAILED(_reason) do {\
        resetInternal();\
        NOTIFY(kEventPrepareResult, _reason, 0, nilParam);\
        return;\
}while(0)

#define EXIT_TMHANDLER(_exit) do { \
    INFO("set timeouthandler exit state to: %d\n", _exit);\
    mInterruptHandler->exit(_exit);\
} while (0)

#define WAIT_PREPARE_OVER() do {\
    EXIT_TMHANDLER(true);\
    while ( 1 ) {\
        MMAutoLock locker(mLock);\
        if ( MM_LIKELY(mState != kStatePreparing)  ) {\
            break;\
        }\
        INFO("preparing, waitting...\n");\
        usleep(30000);\
    }\
}while(0)

#define SETPARAM_BEGIN()\
    for ( MediaMeta::iterator i = meta->begin(); i != meta->end(); ++i ) {\
        const MediaMeta::MetaItem & item = *i;

#define CHECK_TYPE(_val_type)\
        if ( item.mType != MediaMeta::_val_type ) {\
            WARNING("invalid type for %s, expect: %s, real: %d\n", item.mName, #_val_type, item.mType);\
            continue;\
        }

#define SETPARAM_I32(_key_name, _val)\
    if ( !strcmp(item.mName, _key_name) ) {\
        CHECK_TYPE(MT_Int32);\
        _val = item.mValue.ii;\
        INFO("setparam key: %s, val: %d\n", item.mName, item.mValue.ii);\
        mMetaData->setInt32(_key_name, item.mValue.ii);\
        continue;\
    }

#define SETPARAM_I64(_key_name, _val)\
    if ( !strcmp(item.mName, _key_name) ) {\
        CHECK_TYPE(MT_Int64);\
        _val = item.mValue.ld;\
        INFO("setparam key: %s, val: %" PRId64 "\n", item.mName, item.mValue.ld);\
        mMetaData->setInt32(_key_name, item.mValue.ld);\
        continue;\
    }

#define SETPARAM_PTR(_key_name, _val)\
    if ( !strcmp(item.mName, _key_name) ) {\
        CHECK_TYPE(MT_Pointer);\
        _val = item.mValue.ptr;\
        INFO("setparam key: %s, val: %p\n", item.mName, item.mValue.ptr);\
        continue;\
    }

#define SETPARAM_END() }

#define FUNC_ENTER() INFO("+\n")
//#define FUNC_LEAVE() VERBOSE("-\n")
#define FUNC_LEAVE() INFO("-\n")

static const char * MMMSGTHREAD_NAME = "SegExtractor";

static const int PREPARE_TIMEOUT_DEFUALT = 10000000;//us
static const int READ_TIMEOUT_DEFAULT = 10000000;
static const int SEEK_TIMEOUT_DEFAULT = 10000000;

static const int REPORTED_PERCENT_NONE = -1;
static const int WANT_TRACK_NONE = -1;
static const int WANT_TRACK_INDEX_DEFAULT = 0;

static const int64_t SEEK_NONE = -1;

static const int64_t BUFFERING_TIME_DEFAULT = 2 * 1000 * 1000;
static const char * BUFFERING_TIME_CFG_KEY = "mm.segextractor.buffer";
static const char * BUFFERING_TIME_CFG_ENV = "MM_AVDEMUXER_BUFFER";

static const int BUFFER_HIGH_FACTOR = 2;

static const int64_t MAX_BUFFER_SEEK = 5 * 1000 * 1000;

static const AVRational gTimeBase = {1,1000000};

static const size_t AVIO_BUFFER_SIZE = 128*1024;

#ifdef DUMP_INPUT
static const char* sourceDataDump = "/data/input.raw";
static DataDump encodedDataDump(sourceDataDump);
#endif

// fixed point to double
#define CONV_FP(x) ((double) (x)) / (1 << 16)

static double av_display_rotation_get(const int32_t matrix[9])
{
    double rotation, scale[2];

    scale[0] = hypot(CONV_FP(matrix[0]), CONV_FP(matrix[3]));
    scale[1] = hypot(CONV_FP(matrix[1]), CONV_FP(matrix[4]));

    if (scale[0] == 0.0 || scale[1] == 0.0)
        return NAN;

    rotation = atan2(CONV_FP(matrix[1]) / scale[1],
                     CONV_FP(matrix[0]) / scale[0]) * 180 / M_PI;

    return rotation;
}

DEFINE_LOGTAG(SegExtractor)
DEFINE_LOGTAG(SegExtractor::InterruptHandler)
DEFINE_LOGTAG(SegExtractor::SegmentReader)

SegExtractor::InterruptHandler::InterruptHandler(SegExtractor* extractor)
                          : mIsTimeout(false),
                          mExit(false),
                          mActive(false)
{
    FUNC_ENTER();
    callback = handleTimeout;
    opaque = this;
    FUNC_LEAVE();
}

SegExtractor::InterruptHandler::~InterruptHandler()
{
    FUNC_ENTER();
    FUNC_LEAVE();
}

void SegExtractor::InterruptHandler::start(int64_t timeout)
{
    VERBOSE("+\n");
    mActive = true;
    mExit = false;
    mIsTimeout = false;
    mTimer.start(timeout);
    FUNC_LEAVE();
}

void SegExtractor::InterruptHandler::end()
{
    VERBOSE("+\n");
    mIsTimeout = mTimer.isExpired();
    mActive = false;
    FUNC_LEAVE();
}

void SegExtractor::InterruptHandler::exit(bool isExit)
{
    mExit = isExit;
}

bool SegExtractor::InterruptHandler::isExiting() const
{
    return mExit;
}

bool SegExtractor::InterruptHandler::isTimeout() const
{
    return mIsTimeout;
}

/*static */int SegExtractor::InterruptHandler::handleTimeout(void* obj)
{
    InterruptHandler* handler = static_cast<InterruptHandler*>(obj);
    if (!handler) {
        ERROR("InterruptHandler is null\n");
        return 1;
    }

    if (!handler->mActive) {
        ERROR("InterruptHandler not started\n");
        return 0;
    }

    if ( handler->isExiting() ) {
        INFO("user exit\n");
        return 1;
    }

    if ( handler->mTimer.isExpired() ) {
        ERROR("timeout\n");
        return 1;
    }

//    VERBOSE("not timeout\n");
    return  0;
}

SegExtractor::StreamInfo::StreamInfo() : mMediaType(kMediaTypeUnknown),
                        mSelectedStream(WANT_TRACK_NONE),
                        mCurrentIndex(0),
                        mHasAttachedPic(false),
                        mNotCheckBuffering(false),
                        mPeerInstalled(false),
                        mMediaTimeCalibrated(true),
                        mPacketCount(0),
                        mLastDts(0),
                        mLastPts(0),
                        mLargestPts(0),
                        mTimeOffsetUs(0),
                        mTargetTimeUs(SEEK_NONE)
{
    FUNC_ENTER();
    memset(&mTimeBase, 0, sizeof(AVRational));
    FUNC_LEAVE();
}

SegExtractor::StreamInfo::~StreamInfo()
{
    FUNC_ENTER();
    reset();
    FUNC_LEAVE();
}

bool SegExtractor::StreamInfo::init()
{
    MMAutoLock locker(mLock);
    FUNC_ENTER();
    mMetaData = MediaMeta::create();
    return mMetaData;
}

void SegExtractor::StreamInfo::reset()
{
    // MMAutoLock locker(mLock); // reset() is called by ~StreamInfo()
    FUNC_ENTER();
    mMediaType = kMediaTypeUnknown;
    mAllStreams.clear();
    mBufferList[0].clear();
    mBufferList[1].clear();
    mHasAttachedPic = false;
    mCurrentIndex = 0;
    mNotCheckBuffering = false;
    mPeerInstalled = false;
    memset(&mTimeBase, 0, sizeof(AVRational));
    mPacketCount = 0;
    mLastDts = 0;
    mLastPts = 0;
    mLargestPts = 0;
    mMetaData->clear();
    FUNC_LEAVE();
}

int64_t SegExtractor::StreamInfo::bufferedTime() const
{
    MMAutoLock locker(mLock);
    if ( mBufferList[mCurrentIndex].size() <= 1 ) {
        VERBOSE("size: %d\n", mBufferList[mCurrentIndex].size());
        return 0;
    }

    int64_t ft = mBufferList[mCurrentIndex].front()->dts();
    int64_t et = mBufferList[mCurrentIndex].back()->dts();
    VERBOSE("size: %u, ft: %" PRId64 ", et: %" PRId64 "\n", mBufferList[mCurrentIndex].size(), ft, et);
    int64_t dur = et - ft;
    if ( dur < 0 ) {
        int64_t all = 0;
        int64_t former;
        if ( mBufferList[mCurrentIndex].size() == 2 ) {
            return ft - et;
        }
        former = ft;
        BufferList::const_iterator i = mBufferList[mCurrentIndex].begin();
        ++i;
        for ( ; i != mBufferList[mCurrentIndex].end(); ++i ) {
            int64_t cur = (*i)->dts();
            if ( cur < former ) {
                former = cur;
                break;
            }

            all += (cur - former);
            former = cur;
        }

        if ( i != mBufferList[mCurrentIndex].end() ) {
            ++i;
            for ( ; i != mBufferList[mCurrentIndex].end(); ++i ) {
                int64_t cur = (*i)->dts();
                all += (cur - former);
                former = cur;
            }
        }

        return all;
    }

    return dur;
}

int64_t SegExtractor::StreamInfo::bufferedFirstTs(bool isPendingList) const
{
    MMAutoLock locker(mLock);
    return bufferedFirstTs_l(isPendingList);
}
int64_t SegExtractor::StreamInfo::bufferedFirstTs_l(bool isPendingList) const
{
    uint32_t index = mCurrentIndex;

    if (isPendingList)
        index = !mCurrentIndex;
    ASSERT(index<2);

    if ( mBufferList[index].size() == 0 ) {
        INFO("buffer list is empty");
        return INVALID_TIME_STAMP;
    }

    return mBufferList[index].front()->dts();
}
int64_t SegExtractor::StreamInfo::bufferedLastTs(bool isPendingList) const
{
    MMAutoLock locker(mLock);
    return bufferedLastTs_l(isPendingList);
}
int64_t SegExtractor::StreamInfo::bufferedLastTs_l(bool isPendingList) const
{
    uint32_t index = mCurrentIndex;

    if (isPendingList)
        index = !mCurrentIndex;
    ASSERT(index<2);

    if ( mBufferList[index].size() == 0 ) {
        INFO("buffer list is empty");
        return INVALID_TIME_STAMP;
    }

    return mBufferList[index].back()->dts();
}

bool SegExtractor::StreamInfo::removeUntilTs(int64_t ts, int64_t * realTs)
{
    MMAutoLock locker(mLock);
    VERBOSE("media: %d, before remove, size: %u, req ts: %" PRId64 "\n", mMediaType, mBufferList[mCurrentIndex].size(), ts);
    MMASSERT(!mBufferList[mCurrentIndex].empty() );
    if ( MM_UNLIKELY(mBufferList[mCurrentIndex].empty()) ) {
        ERROR("no data to remove\n");
        return false;
    }

    if ( ts > bufferedLastTs_l() ) {
        mBufferList[mCurrentIndex].clear();
        VERBOSE("media: %d, req larger than last\n");
        return false;
    }

    while ( !mBufferList[mCurrentIndex].empty() ) {
        if ( ts >= mBufferList[mCurrentIndex].front()->dts() ) {
            *realTs = mBufferList[mCurrentIndex].front()->dts();
            break;
        }
        mBufferList[mCurrentIndex].pop_front();
    }

    if ( mMediaType == kMediaTypeVideo ) {
        VERBOSE("removing to key frame\n");
        while ( !mBufferList[mCurrentIndex].empty() ) {
            MediaBufferSP buf = mBufferList[mCurrentIndex].front();
            if ( !buf->isFlagSet(MediaBuffer::MBFT_KeyFrame) ) {
                mBufferList[mCurrentIndex].pop_front();
                continue;
            }

            *realTs = buf->dts();
            break;
        }

        if ( mBufferList[mCurrentIndex].empty() ) {
            VERBOSE("media: %d, after remove, no data\n");
            return false;
        }
    } else {
        if ( mBufferList[mCurrentIndex].empty() ) {
            VERBOSE("media: %d, after remove, no data\n");
            return false;
        }
    }
    MMLOGD("media: %d, after remove, size: %u, req ts: %" PRId64 ", realts: %" PRId64 "\n", mMediaType, mBufferList[mCurrentIndex].size(), ts, *realTs);
    return true;
}

mm_status_t SegExtractor::StreamInfo::read(MediaBufferSP & buffer)
{
    MMAutoLock locker(mLock);
    if ( mBufferList[mCurrentIndex].size() == 0 ) {
        VERBOSE("no more\n");
        return MM_ERROR_EOS;
    }

    VERBOSE("write_buffer_size_%d: (current, %d), (pending, %d)\n",
        mMediaType, mBufferList[mCurrentIndex].size(), mBufferList[!mCurrentIndex].size());

    buffer = mBufferList[mCurrentIndex].front();
    mBufferList[mCurrentIndex].pop_front();

    // set mTargetTimeUs on mBufferList.front(), in case checkBufferSeek() is true
    if ((mTargetTimeUs > 0) && (buffer->pts() < mTargetTimeUs)) {
        buffer->getMediaMeta()->setInt64(MEDIA_ATTR_TARGET_TIME, mTargetTimeUs);
        MMLOGD("set TargetTimeUs %0.3f\n", mTargetTimeUs/1000000.0f);
        mTargetTimeUs = SEEK_NONE;
    }

    return MM_ERROR_SUCCESS;
}

/* dash doesn't use track index to select reperesentation */
bool SegExtractor::StreamInfo::write(MediaBufferSP buffer/*, int streamIndex*/)
{
    MMAutoLock locker(mLock);
    VERBOSE("write_buffer_size_%d: (current, %d), (pending, %d)\n",
        mMediaType, mBufferList[mCurrentIndex].size(), mBufferList[!mCurrentIndex].size());

    uint32_t index = -1;
    index = mCurrentIndex;

    mBufferList[index].push_back(buffer);
    return true;
}

SegExtractor::SegmentReader::SegmentReader(SegExtractor * component, StreamInfo * si)
                    : mComponent(component),
                    mStreamInfo(si)
{
    FUNC_ENTER();
#ifdef DUMP_OUTPUT
#define OPENF(_f, _path) do {\
    _f = fopen(_path, "w");\
    if ( !_f ) {\
        MMLOGW("failed to open file for dump\n");\
    }\
}while(0)

    OPENF(mOutputDumpFpAudio, "/data/avdoa.bin");
    OPENF(mOutputDumpFpVideo, "/data/avdov.bin");
#endif
    FUNC_LEAVE();
}

SegExtractor::SegmentReader::~SegmentReader()
{
    FUNC_ENTER();
#ifdef DUMP_OUTPUT
#define RELEASEF(_f) do {\
    if ( _f ) {\
        fclose(_f);\
        _f = NULL;\
    }\
}while(0)
    RELEASEF(mOutputDumpFpAudio);
    RELEASEF(mOutputDumpFpVideo);
#endif
    FUNC_LEAVE();
}

mm_status_t SegExtractor::SegmentReader::read(MediaBufferSP & buffer)
{
    VERBOSE("+\n");
#ifdef DUMP_OUTPUT
    mm_status_t ret = mComponent->read(buffer, mStreamInfo);
    if ( ret == MM_ERROR_SUCCESS ) {
        FILE * fp;
        if ( mStreamInfo->mMediaType == kMediaTypeAudio ) {
            fp = mOutputDumpFpAudio;
        } else if ( mStreamInfo->mMediaType == kMediaTypeVideo ) {
            fp = mOutputDumpFpVideo;
        } else {
            fp = NULL;
        }
        if ( fp ) {
            if ( buffer->isFlagSet(MediaBuffer::MBFT_CodecData) ) {
                uint8_t * csdBuf;
                int64_t csdBufSize = buffer->size();
                buffer->getBufferInfo((uintptr_t*)&csdBuf, 0, 0, 1);
                if ( csdBuf && csdBufSize > 0 ) {
                    fwrite(csdBuf, 1, csdBufSize, fp);
                    fflush(fp);
                }
            } else if ( buffer->isFlagSet(MediaBuffer::MBFT_AVPacket) ) {
                uintptr_t buf;
                int64_t size = buffer->size();
                buffer->getBufferInfo((uintptr_t*)&buf, 0, 0, 1);
                if ( buf && size > 0 ) {
                    fwrite((unsigned char *)buf, 1, size, fp);
                    fflush(fp);
                }
            } else {
                MMLOGW("invalid outgoing data\n");
            }
        } else {
            ERROR("no fp to dump\n");
        }
    }
    return ret;
#else
    return mComponent->read(buffer, mStreamInfo);
#endif
}

MediaMetaSP SegExtractor::SegmentReader::getMetaData()
{
    return mStreamInfo->mMetaData;
}

SegExtractor::SegExtractor(const char* mimeType) :
                MMMsgThread(MMMSGTHREAD_NAME),
                mState(kStateNone),
                mAVFormatContext(NULL),
                mAVInputFormat(NULL),
                mAVIOContext(NULL),
                mInitSegment(NULL),
                mNextPeriodInitSegment(NULL),
                mSegmentBuffer(NULL),
                mReadPos(0),
                mDashCreateByUs(false),
                mInitSegmentConsumed(false),
                mDashStream(NULL),
                mPrepareTimeout(PREPARE_TIMEOUT_DEFUALT),
                mSeekTimeout(SEEK_TIMEOUT_DEFAULT),
                mReadTimeout(READ_TIMEOUT_DEFAULT),
                mScaledPlayRate(SCALED_PLAY_RATE),
                mScaledThresholdRate(SCALED_PLAY_RATE * 2),
                mScaledPlayRateCur(SCALED_PLAY_RATE),
                mInterruptHandler(NULL),
                mEOS(false),
                mEOF(false),
                mReadingSample(false),
                mReportedBufferingPercent(REPORTED_PERCENT_NONE),
                mSeekUs(SEEK_NONE),
                mSeekDone(false),
                mDashSelectVideo(false),
                mDashSelectAudio(false),
                mSelectedMediaType(kMediaTypeUnknown),
                mCheckVideoKeyFrame(false),
                mPlaybackStartUs(INVALID_TIME_STAMP)
#ifdef DUMP_INPUT
                , mInputFile(NULL)
#endif
{
    FUNC_ENTER();
    class AVInitializer {
    public:
        AVInitializer() {
            INFO("use log level converter\n");
            av_log_set_callback(AVLogger::av_log_callback);
            av_log_set_level(AV_LOG_ERROR);
            avcodec_register_all();
            av_register_all();
            avformat_network_init();
        }
        ~AVInitializer() {
            av_log_set_callback(NULL);
            avformat_network_deinit();
        }
    };
    static AVInitializer sAVInit;

    mBufferingTime = getConfigBufferingTime();
    if (mBufferingTime <= 0) {
        mBufferingTime = BUFFERING_TIME_DEFAULT;
    } else {
        mBufferingTime *= (1000 * 1000);
    }
    mBufferingTimeHigh = mBufferingTime * BUFFER_HIGH_FACTOR;
    INFO("BufferingTime: %" PRId64 ", BufferingTimeHigh: %" PRId64 "\n", mBufferingTime, mBufferingTimeHigh);

    mMime = mimeType;
    FUNC_LEAVE();
}

SegExtractor::~SegExtractor()
{
    FUNC_ENTER();

#ifdef DUMP_INPUT
    encodedDataDump.dump(NULL, 0);
#endif

    FUNC_LEAVE();
}

int SegExtractor::getConfigBufferingTime()
{
    std::string result = mm_get_env_str(BUFFERING_TIME_CFG_KEY, BUFFERING_TIME_CFG_ENV);
    const char * bufferingTime = result.c_str();
    if (!bufferingTime || bufferingTime[0] == '\0') {
        INFO("buffering time not configured\n");
        return -1;
    }

    INFO("buffering time configured: %s\n", bufferingTime);
    return atoi(bufferingTime);
}

mm_status_t SegExtractor::init()
{
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    RET_IF_ALREADY_STATE(kStateIdle, MM_ERROR_SUCCESS);
    CHECK_STATE(kStateNone);

    if ( !(mMetaData = MediaMeta::create()) ) {
        ERROR("no mem\n");
        SET_STATE(kStateError);
        return MM_ERROR_NO_MEM;
    }

    for ( int i = 0; i < kMediaTypeCount; ++i ) {
        if ( !mStreamInfoArray[i].init() ) {
            ERROR("failed to prepare si %d\n", i);
            SET_STATE(kStateError);
            return MM_ERROR_NO_MEM;
        }
    }

    mInterruptHandler = new InterruptHandler(this);
    if ( !mInterruptHandler ) {
        ERROR("failed to alloc InterruptHandler\n");
        SET_STATE(kStateError);
        return MM_ERROR_NO_MEM;
    }

    int ret = run();
    if ( ret ) {
        ERROR("failed to run, ret: %d\n", ret);
        MM_RELEASE(mInterruptHandler);
        SET_STATE(kStateError);
        return MM_ERROR_NO_MEM;
    }

    SET_STATE(kStateIdle);
    VERBOSE("success\n");
    return MM_ERROR_SUCCESS;
}

void SegExtractor::uninit()
{
    FUNC_ENTER();
    reset();
    exit();
    VERBOSE("thread exited\n");
    VERBOSE("freeing InterruptHandler\n");
    MM_RELEASE(mInterruptHandler);
    FUNC_LEAVE();
}

const std::list<std::string> & SegExtractor::supportedProtocols() const
{
    return getSupportedProtocols();
}

/*static */const std::list<std::string> & SegExtractor::getSupportedProtocols()
{
    static std::list<std::string> protocols;
    if (!protocols.empty()) {
        ERROR("no supported protocol\n");
        return protocols;
    }

    av_register_all();
    void* opq = NULL;
    const char* protocol;
    while ((protocol = avio_enum_protocols(&opq, 0))) {
        VERBOSE("supported protocol: %s\n", protocol);
        std::string str(protocol);
        protocols.push_back(str);
    }
    return protocols;
}

bool SegExtractor::isSeekable()
{
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    return isSeekableInternal();
}

mm_status_t SegExtractor::getDuration(int64_t & durationMs)
{
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    return duration(durationMs);
}

mm_status_t SegExtractor::duration(int64_t & durationMs)
{
    if ( !mDashStream ) {
        ERROR("not loaded\n");
        return MM_ERROR_IVALID_OPERATION;
    }

    mm_status_t status;
    if ((status = mDashStream->getDuration(durationMs)) != MM_ERROR_SUCCESS) {
        INFO("\n");
        return status;
    }

    INFO("dur: %" PRId64 " ms\n", durationMs);
    //INFO("dur: %" PRId64 " ms\n", ( mAVFormatContext->duration + 500 )/1000);
    //durationMs = ( mAVFormatContext->duration + 500 ) / 1000LL;

    return MM_ERROR_SUCCESS;
}

bool SegExtractor::isSeekableInternal()
{
    if ( !mAVFormatContext ) {
        ERROR("not inited\n");
        return false;
    }

    if ( mAVFormatContext->duration == (int64_t)AV_NOPTS_VALUE ) {
        MMLOGD("duration not determined, false\n");
        return false;
    }

    if ( mAVFormatContext->pb && mAVFormatContext->pb->seekable ) {
        MMLOGD("pb exists, ret: %d\n", mAVFormatContext->pb->seekable);
        return mAVFormatContext->pb->seekable;
    }

    MMLOGD("read_seek: %p, read_seek2: %p\n", mAVFormatContext->iformat->read_seek, mAVFormatContext->iformat->read_seek2);
    //return mAVFormatContext->iformat->read_seek || mAVFormatContext->iformat->read_seek2;
    //MMLOGD("dash seek is not implemented\n");
    return true;
}

bool SegExtractor::hasMedia(MediaType mediaType)
{
    MMAutoLock lock(mLock);
    return hasMediaInternal(mediaType);
}

bool SegExtractor::hasMediaInternal(MediaType mediaType)
{
    if ( mediaType >= kMediaTypeCount || mediaType <= kMediaTypeUnknown ) {
        ERROR("not supported mediatype: %d\n", mediaType);
        return false;
    }

    if (mediaType == kMediaTypeVideo)
        return mDashSelectVideo;
    if (mediaType == kMediaTypeAudio)
        return mDashSelectAudio;

    return false;
}

MediaMetaSP SegExtractor::getMetaData()
{
    return mMetaData;
}

MMParamSP SegExtractor::getTrackInfo()
{
    MMAutoLock lock(mLock);
    if ( mState <  kStatePrepared || mState > kStateStarted ) {
        ERROR("invalid state(%d)\n", mState);
        return nilParam;
    }

    int streamCount = 0;
    for ( int i = 0; i < kMediaTypeCount; ++i ) {
        if ( mStreamInfoArray[i].mMediaType != kMediaTypeUnknown ) {
            ++streamCount;
        }
    }

    if ( streamCount == 0 ) {
        INFO("no stream found\n");
        return nilParam;
    }

    MMParamSP param(new MMParam());
    if ( !param ) {
        ERROR("no mem\n");
        return nilParam;
    }

    // stream count
    param->writeInt32(streamCount);
    VERBOSE("stream count: %d\n", streamCount);

    // every stream
    const StreamInfo * si = mStreamInfoArray;
    for ( int i = 0; i < kMediaTypeCount; ++i, ++si ) {
        if ( si->mMediaType == kMediaTypeUnknown ) {
            VERBOSE("%d: type: %d not supported, ignore\n", i, si->mMediaType);
            continue;
        }

        // track type
        VERBOSE("track type: %d\n", i);
        param->writeInt32(i);

        // track count
        VERBOSE("track count: %d\n", si->mAllStreams.size());
        param->writeInt32(si->mAllStreams.size());
        std::vector<int>::const_iterator it;
        int idx;
        for ( it = si->mAllStreams.begin(), idx = 0; it != si->mAllStreams.end(); ++it, ++idx ) {
            VERBOSE("track id: %d\n", idx);
            param->writeInt32(idx); // id
            AVStream * stream = mAVFormatContext->streams[*it];
            // codec
            CowCodecID codecId;
            const char * codecName;
            int32_t width = 0, height = 0;
            AVCodecContext *ctx = stream->codec;
            if (ctx && ctx->codec_id) {
                VERBOSE("has ctx: %p\n", ctx);
                width = ctx->width;
                height = ctx->height;
                codecId = AVCodecId2CodecId(ctx->codec_id);
                codecName = avcodec_descriptor_get(ctx->codec_id)->name;
                VERBOSE("codecId: %d, codecName: %s\n", codecId, codecName);
            } else {
                INFO("unknown codec\n");
                codecId = kCodecIDNONE;
                codecName = "";
            }
            param->writeInt32(codecId);
            param->writeCString(codecName);
            param->writeCString(codecId2Mime((CowCodecID)codecId));
            VERBOSE("getting title\n");
            // title
            AVDictionaryEntry * tagTitle = av_dict_get(stream->metadata, "title", NULL, 0);
#define WRITE_TAG(_tag) do {\
        if (_tag)\
            param->writeCString(_tag->value);\
        else\
            param->writeCString("");\
    }while(0)
            WRITE_TAG(tagTitle);

            // lang
            VERBOSE("getting lang\n");
            AVDictionaryEntry * tagLang = av_dict_get(stream->metadata, "language", NULL, 0);
            if (!tagLang)
                tagLang = av_dict_get(stream->metadata, "lang", NULL, 0);
            WRITE_TAG(tagLang);

            MMLOGD("id: %d(%d), title: %s, lang: %s, codecId: %d, codecName: %s\n",
                idx, *it, tagTitle ? tagTitle->value : "", tagLang ? tagLang->value : "", codecId, codecName);
            if (i == kMediaTypeVideo) {
                param->writeInt32(width);
                param->writeInt32(height);
                MMLOGD("resolution: %dx%d\n", width, height);
            }
        }
    }

    return param;
}

MMParamSP SegExtractor::getMediaRepresentationInfo()
{
    MMAutoLock lock(mLock);
    if ( mState <  kStatePrepared || mState > kStateStarted ) {
        ERROR("invalid state(%d)\n", mState);
        return nilParam;
    }

    if (!mDashStream) {
        ERROR("no dash stream");
        return nilParam;
    }

    MMParamSP param(new MMParam());
    if ( !param ) {
        ERROR("no mem\n");
        return nilParam;
    }

    if (!mDashStream->getRepresentationInfo(param)) {
        ERROR("fail to get media representation info");
        return nilParam;
    }

    return param;
}

mm_status_t SegExtractor::selectTrack(MediaType mediaType, int index)
{
    INFO("mediaType: %d, index: %d\n", mediaType, index);
    if ( mediaType >= kMediaTypeCount || mediaType <= kMediaTypeUnknown ) {
        ERROR("not supported mediatype: %d\n", mediaType);
        return MM_ERROR_INVALID_PARAM;
    }

    // MMAutoLock lock(mLock);
    // CHECK_STATE(kStatePrepared);
    mm_status_t status = selectTrackInternal(mediaType, index);
    if (status != MM_ERROR_SUCCESS)
        return status;

    if (mediaType == kMediaTypeVideo) {
        WARNING("assume we select audio track with same index: %d", index);
        int audio_track_index = index;

        selectTrackInternal(kMediaTypeAudio, audio_track_index);
    }

    return status;
}

mm_status_t SegExtractor::selectMediaRepresentation(int peroid, MediaType mediaType, int adaptationSet, int index)
{
    INFO("mediaType: %d, index: %d\n", mediaType, index);
    mSelectedMediaType = mediaType;

    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::selectTrackInternal(MediaType mediaType, int index)
{
    VERBOSE("mediaType: %d, index: %d\n", mediaType, index);
    if ( index < 0 ) {
        VERBOSE("<0, set to default: %d\n", WANT_TRACK_INDEX_DEFAULT);
        index = WANT_TRACK_INDEX_DEFAULT;
    }

    MMASSERT(mediaType < kMediaTypeCount && mediaType > kMediaTypeUnknown);
    StreamInfo * si = &mStreamInfoArray[mediaType];
    if ( si->mMediaType == kMediaTypeUnknown ) {
        ERROR("no such media(%d)\n", mediaType);
        return MM_ERROR_INVALID_PARAM;
    }

    if ( (size_t)index >= si->mAllStreams.size() ) {
        ERROR("no such index(%d)\n", index);
        return MM_ERROR_INVALID_PARAM;
    }

    si->mSelectedStream = si->mAllStreams[index];

    int id = si->mAllStreams[index];
    AVStream * stream = mAVFormatContext->streams[id];
    stream->discard = AVDISCARD_DEFAULT;
    memcpy(&si->mTimeBase, &stream->time_base, sizeof(AVRational));
    si->mHasAttachedPic = stream->disposition & AV_DISPOSITION_ATTACHED_PIC ? true : false;
    INFO("mediaType: %d, index: %d, id: %d, info: %p, HasAttachedPic: %d, num: %d, den: %d\n", mediaType, index, id, si, si->mHasAttachedPic, si->mTimeBase.num, si->mTimeBase.den);

    AVCodecContext * codecContext = mAVFormatContext->streams[id]->codec;
    INFO("codecContext: codec_type: %d, codec_id: 0x%x, codec_tag: 0x%x, stream_codec_tag: 0x%x, profile: %d, width: %d, height: %d, extradata: %p, extradata_size: %d, channels: %d, sample_rate: %d, channel_layout: %" PRId64 ", bit_rate: %d, block_align: %d, avg_frame_rate: (%d, %d)\n",
                    codecContext->codec_type,
                    codecContext->codec_id,
                    codecContext->codec_tag,
                    codecContext->stream_codec_tag,
                    codecContext->profile,
                    codecContext->width,
                    codecContext->height,
                    codecContext->extradata,
                    codecContext->extradata_size,
                    codecContext->channels,
                    codecContext->sample_rate,
                    codecContext->channel_layout,
                    codecContext->bit_rate,
                    codecContext->block_align,
                    mAVFormatContext->streams[id]->avg_frame_rate.num,
                    mAVFormatContext->streams[id]->avg_frame_rate.den);

    if ( mediaType == kMediaTypeVideo ) {
        if ( codecContext->extradata && codecContext->extradata_size > 0 ) {
            si->mMetaData->setByteBuffer(MEDIA_ATTR_CODEC_DATA, codecContext->extradata, codecContext->extradata_size);
        }
        AVRational * avr = &mAVFormatContext->streams[id]->avg_frame_rate;
        if ( avr->num > 0 && avr->den > 0 ) {
            MMLOGD("has avg fps: %d\n", avr->num / avr->den);
            si->mMetaData->setInt32(MEDIA_ATTR_AVG_FRAMERATE, avr->num / avr->den);
        }
        si->mMetaData->setInt32(MEDIA_ATTR_CODECID, codecContext->codec_id);
        si->mMetaData->setString(MEDIA_ATTR_MIME, codecId2Mime((CowCodecID)codecContext->codec_id));
        si->mMetaData->setInt32(MEDIA_ATTR_CODECTAG, codecContext->codec_tag);
        si->mMetaData->setInt32(MEDIA_ATTR_STREAMCODECTAG, codecContext->stream_codec_tag);
        si->mMetaData->setInt32(MEDIA_ATTR_CODECPROFILE, codecContext->profile);
        si->mMetaData->setInt32(MEDIA_ATTR_WIDTH, codecContext->width);
        si->mMetaData->setInt32(MEDIA_ATTR_HEIGHT, codecContext->height);

        //set threshold rate according to dimesion
        if (codecContext->width > 1280 && codecContext->height > 720) {
            mScaledThresholdRate = SCALED_PLAY_RATE * 2;
        } else if (codecContext->width > 640 && codecContext->height > 480) {
            mScaledThresholdRate = SCALED_PLAY_RATE * 3;
        } else if (codecContext->width > 0 && codecContext->height > 0){
            mScaledThresholdRate = SCALED_PLAY_RATE * 4;
        }
        MMLOGD("width %d, height %d, mScaledThresholdRate %d\n", codecContext->width, codecContext->height, mScaledThresholdRate);


        if (stream->nb_side_data) {
            for (int i = 0; i < stream->nb_side_data; i++) {
                AVPacketSideData sd = stream->side_data[i];
                if (sd.type == AV_PKT_DATA_DISPLAYMATRIX) {
                    double degree = av_display_rotation_get((int32_t *)sd.data);
                    MMLOGD("degree %0.2f\n", degree);

                    int32_t degreeInt = (int32_t)degree;
                    if (degreeInt < 0)
                        degreeInt += 360;
                    si->mMetaData->setInt32(MEDIA_ATTR_ROTATION, (int32_t)degreeInt);
                    NOTIFY(kEventVideoRotationDegree, int(degreeInt), 0, nilParam);
                }
                else {
                    MMLOGD("ignore type %d\n", sd.type);
                    continue;
                }
            }
        }

    } else if ( mediaType == kMediaTypeAudio ) {
        mMetaData->setInt32(MEDIA_ATTR_BIT_RATE, codecContext->bit_rate);
        if ( codecContext->extradata && codecContext->extradata_size > 0 ) {
            si->mMetaData->setByteBuffer(MEDIA_ATTR_CODEC_DATA, codecContext->extradata, codecContext->extradata_size);
        }
        si->mMetaData->setInt32(MEDIA_ATTR_CODECID, codecContext->codec_id);
        si->mMetaData->setString(MEDIA_ATTR_MIME, codecId2Mime((CowCodecID)codecContext->codec_id));
        si->mMetaData->setInt32(MEDIA_ATTR_CODECTAG, codecContext->codec_tag);
        si->mMetaData->setInt32(MEDIA_ATTR_STREAMCODECTAG, codecContext->stream_codec_tag);
        si->mMetaData->setInt32(MEDIA_ATTR_CODECPROFILE, codecContext->profile);
        si->mMetaData->setInt32(MEDIA_ATTR_SAMPLE_FORMAT, convertAudioFormat(codecContext->sample_fmt));
        si->mMetaData->setInt32(MEDIA_ATTR_SAMPLE_RATE, codecContext->sample_rate);
        si->mMetaData->setInt32(MEDIA_ATTR_CHANNEL_COUNT, codecContext->channels);
        si->mMetaData->setInt64(MEDIA_ATTR_CHANNEL_LAYOUT, codecContext->channel_layout);
        si->mMetaData->setInt32(MEDIA_ATTR_BIT_RATE, codecContext->bit_rate);
        si->mMetaData->setInt32(MEDIA_ATTR_BLOCK_ALIGN, codecContext->block_align);
        int64_t durationMs;
        if ( duration(durationMs) == MM_ERROR_SUCCESS )
            si->mMetaData->setInt64(MEDIA_ATTR_DURATION, durationMs);

        if (mAVInputFormat == av_find_input_format("aac") ||
           mAVInputFormat == av_find_input_format("mpegts") ||
           mAVInputFormat == av_find_input_format("hls,applehttp")) {
           //Set kKeyIsADTS to ture as default as mpegts2Extractor/aacExtractor did
           si->mMetaData->setInt32(MEDIA_ATTR_IS_ADTS, 1);
        }
    }

    si->mMetaData->setFraction(MEDIA_ATTR_TIMEBASE, 1, 1000000);

    si->mMetaData->setPointer(MEDIA_ATTR_CODEC_CONTEXT, codecContext);
    si->mMetaData->setPointer(MEDIA_ATTR_CODEC_CONTEXT_MUTEX, &mAVLock);
    si->mMetaData->setInt32(MEDIA_ATTR_CODECPROFILE, codecContext->profile);
    si->mMetaData->setInt32(MEDIA_ATTR_CODECTAG, codecContext->codec_tag);
    si->mMetaData->setString(MEDIA_ATTR_MIME, codecId2Mime((CowCodecID)codecContext->codec_id));
    si->mMetaData->setInt32(MEDIA_ATTR_CODECID, codecContext->codec_id);

    NOTIFY(kEventInfo, kEventMetaDataUpdate, 0, nilParam);

    return MM_ERROR_SUCCESS;
}

int SegExtractor::getSelectedTrack(MediaType mediaType)
{
    VERBOSE("mediaType: %d\n", mediaType);
    if ( mediaType >= kMediaTypeCount || mediaType <= kMediaTypeUnknown ) {
        ERROR("not supported mediatype: %d\n", mediaType);
        return -1;
    }

    MMAutoLock lock(mLock);
    // ?? mSelectedStream is packet stream_index, not track index
    return mStreamInfoArray[mediaType].mSelectedStream;
}

int SegExtractor::getSelectedMediaRepresentation(int peroid, MediaType mediaType, int &adaptationSet)
{
    return -1;
}

BEGIN_MSG_LOOP(SegExtractor)
    MSG_ITEM(MSG_PREPARE, onPrepare)
    MSG_ITEM(MSG_READ_SAMPLE, onReadSample)
END_MSG_LOOP()

void SegExtractor::resetDash() {
    if (!mDashCreateByUs)
        return;

    INFO("destroy dash stream");
    if (mDashStream)
        MM_RELEASE(mDashStream);

    mDashCreateByUs = false;
    mInitSegmentConsumed = false;

    mDashSelectVideo = false;
    mDashSelectAudio = false;
}

void SegExtractor::resetInternal()
{
    EXIT_TMHANDLER(true);
    releaseContext();

    mMpdUri = "";

    mEOS = false;
    mEOF = false;
    mScaledThresholdRate = SCALED_PLAY_RATE * 2;
    mScaledPlayRate = SCALED_PLAY_RATE;
    mSelectedMediaType = kMediaTypeUnknown;

    for ( int i = 0; i < kMediaTypeCount; ++i ) {
        mStreamInfoArray[i].reset();
    }
    mStreamIdx2Info.clear();
    mReportedBufferingPercent = REPORTED_PERCENT_NONE;
    mMetaData->clear();
    MMAutoLock lock2(mBufferLock);
    while (!mSeekSequence.empty()) {
        mSeekSequence.pop();
    }
    mSeekUs = SEEK_NONE;

    SET_STATE(kStateIdle);

    if (mSegmentBuffer)
        mSegmentBuffer->stop();

    resetDash();
}

Component::ReaderSP SegExtractor::getReader(MediaType mediaType)
{
    MMAutoLock lock(mLock);
    if ( mediaType >= kMediaTypeCount || mediaType <= kMediaTypeUnknown ) {
        ERROR("not supported mediatype: %d\n", mediaType);
        return Component::ReaderSP((Component::Reader*)NULL);
    }

    StreamInfo * si = &mStreamInfoArray[mediaType];
    si->mPeerInstalled = true;

    return ReaderSP(new SegmentReader(this, si));
}

mm_status_t SegExtractor::read(MediaBufferSP & buffer, StreamInfo * si)
{
    if ( !si ) {
        ERROR("invalid params\n");
        return MM_ERROR_INVALID_PARAM;
    }

    MMAutoLock lock(mBufferLock);

    mm_status_t ret = si->read(buffer);
    VERBOSE("read_%d, ret: %d\n", si->mMediaType, ret);

    if ( MM_UNLIKELY(mEOS) ) {
        INFO("read_%d, eos, ret: %d\n", si->mMediaType, ret);
        return ret;
    }

    if ( MM_UNLIKELY(ret != MM_ERROR_SUCCESS) ) {
        DEBUG("read not success, %d", ret);
        ret =  MM_ERROR_AGAIN;
    }

    int64_t min = INT64_MAX;
    if ( MM_UNLIKELY(!checkBuffer(min)) ) {
        INFO("%d_not check buffer, start read sample\n", si->mMediaType);
        if (!mReadingSample) {
            postMsg(MSG_READ_SAMPLE, 0, 0);
            mReadingSample = true;
        }
        return ret;
    }

    if (min < mBufferingTime / 2) {
        INFO("start read sample");
        if (!mReadingSample) {
            postMsg(MSG_READ_SAMPLE, 0, 0);
            mReadingSample = true;
        }
        return ret;
    }

    return ret;
}

mm_status_t SegExtractor::setParameter(const MediaMetaSP & meta)
{
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    void *ptr = NULL;
    SETPARAM_BEGIN()
        SETPARAM_I32(PARAM_KEY_PREPARE_TIMEOUT, mPrepareTimeout)
        SETPARAM_I32(PARAM_KEY_READ_TIMEOUT, mReadTimeout)
        SETPARAM_I64(PARAM_KEY_BUFFERING_TIME, mBufferingTime)
        SETPARAM_I32(MEDIA_ATTR_PALY_RATE, mScaledPlayRateCur)
        SETPARAM_PTR(PARAM_KEY_SEGMENT_BUFFER, ptr)
    SETPARAM_END()

    mBufferingTimeHigh = mBufferingTime * BUFFER_HIGH_FACTOR;

    if (ptr) {
        INFO("get media segment buffer");
        mSegmentBuffer = static_cast<SegmentBuffer*>(ptr);
        mDashCreateByUs = false;
    }

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::getParameter(MediaMetaSP & meta) const
{
    meta.reset();
    meta = mMetaData;

    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::setUri(const char * uri,
                            const std::map<std::string, std::string> * headers/* = NULL*/)
{
    INFO("mpd: %s\n", uri);
    MMAutoLock lock(mLock);
    CHECK_STATE(kStateIdle);
    mMpdUri = uri;

    if (!mSegmentBuffer) {
        mDashCreateByUs = true;

        INFO("create dash stream, selected media type %d", mSelectedMediaType);
        if (mSelectedMediaType == kMediaTypeVideo) {
            mDashStream = DashStream::createStreamByName((char*)uri, DashStream::kStreamTypeVideo);
            mDashSelectVideo = true;
        } else if (mSelectedMediaType == kMediaTypeAudio) {
            mDashStream = DashStream::createStreamByName((char*)uri, DashStream::kStreamTypeAudio);
            mDashSelectAudio = true;
        }
        else
            mDashStream = DashStream::createStreamByName((char*)uri, DashStream::kStreamTypeUndefined);

        if (!mDashStream) {
            ERROR("fail to create dash stream");
            resetDash();
            return MM_ERROR_INVALID_URI;
        }

        if (mSelectedMediaType != kMediaTypeUnknown)
            if (mDashStream->setRepresentation(0, 0, 0) != MM_ERROR_SUCCESS)
                WARNING("fail to select representation");

        // for mSelectedMediaType not set
        bool ret = false;
        if (mDashStream->hasMediaType(DashStream::kStreamTypeVideo)) {
            ret = mDashStream->setStreamType(DashStream::kStreamTypeVideo);
            if (ret) {
                INFO("set stream type video, ret %d", ret);
                mDashSelectVideo = ret;
                if (mDashStream->setRepresentation(0, 0, 0) != MM_ERROR_SUCCESS)
                    WARNING("fail to select representation");
            }
        }

        if (!ret && mDashStream->hasMediaType(DashStream::kStreamTypeAudio)) {
            ret = mDashStream->setStreamType(DashStream::kStreamTypeAudio);
            if (ret) {
                INFO("set stream type video, ret %d", ret);
                mDashSelectVideo = ret;
                if (mDashStream->setRepresentation(0, 0, 0) != MM_ERROR_SUCCESS)
                    WARNING("fail to select representation");
            }
        }

        mSegmentBuffer = mDashStream->createSegmentBuffer();
        if (!mSegmentBuffer) {
            ERROR("fail to create segment buffer");
            resetDash();
            return MM_ERROR_NO_MEM;
        }

        updateCurrentPeriod();
    } else
        return MM_ERROR_INVALID_STATE;

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::setUri(int fd, int64_t offset, int64_t length)
{
    INFO("");
    return MM_ERROR_UNSUPPORTED;
}

mm_status_t SegExtractor::setMPD(dash::mpd::IMPD *mpd) {
    MMAutoLock lock(mLock);
    CHECK_STATE(kStateIdle);

    if (!mpd) {
        ERROR("mpd object is null");
        return MM_ERROR_INVALID_STATE;
    }

    INFO("mpd: %s\n", mpd->GetMPDPathBaseUrl()->GetUrl().c_str());

    mMpdUri = mpd->GetMPDPathBaseUrl()->GetUrl().c_str();

    if (!mSegmentBuffer) {
        mDashCreateByUs = true;

        INFO("create dash stream, selected media type %d", mSelectedMediaType);

        if (mSelectedMediaType == kMediaTypeVideo) {
            mDashStream = DashStream::createStreamByMpd(mpd, DashStream::kStreamTypeVideo);
            mDashSelectVideo = true;
        } else if (mSelectedMediaType == kMediaTypeAudio) {
            mDashStream = DashStream::createStreamByMpd(mpd, DashStream::kStreamTypeAudio);
            mDashSelectAudio = true;
        }
        else
            mDashStream = DashStream::createStreamByMpd(mpd, DashStream::kStreamTypeUndefined);

        if (!mDashStream) {
            ERROR("fail to create dash stream");
            resetDash();
            return MM_ERROR_INVALID_URI;
        }

        if (mSelectedMediaType != kMediaTypeUnknown)
            if (mDashStream->setRepresentation(0, 0, 0) != MM_ERROR_SUCCESS)
                WARNING("fail to select representation");

        // for mSelectedMediaType not set
        bool ret = false;
        if (mDashStream->hasMediaType(DashStream::kStreamTypeVideo)) {
            ret = mDashStream->setStreamType(DashStream::kStreamTypeVideo);
            if (ret) {
                INFO("set stream type video, ret %d", ret);
                mDashSelectVideo = ret;
                if (mDashStream->setRepresentation(0, 0, 0) != MM_ERROR_SUCCESS)
                    WARNING("fail to select representation");
            }
        }

        if (!ret && mDashStream->hasMediaType(DashStream::kStreamTypeAudio)) {
            ret = mDashStream->setStreamType(DashStream::kStreamTypeAudio);
            if (ret) {
                INFO("set stream type video, ret %d", ret);
                mDashSelectVideo = ret;
                if (mDashStream->setRepresentation(0, 0, 0) != MM_ERROR_SUCCESS)
                    WARNING("fail to select representation");
            }
        }

        mSegmentBuffer = mDashStream->createSegmentBuffer();
        if (!mSegmentBuffer) {
            ERROR("fail to create segment buffer");
            resetDash();
            return MM_ERROR_NO_MEM;
        }
        updateCurrentPeriod();
    } else
        return MM_ERROR_INVALID_STATE;

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

dash::mpd::IMPD* SegExtractor::getMPD() {
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    if (!mDashStream) {
        ERROR("no MPD");
        return NULL;
    }

    return mDashStream->getMPD();
}

mm_status_t SegExtractor::prepare()
{
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    RET_IF_ALREADY_STATE(kStatePreparing, MM_ERROR_ASYNC);
    RET_IF_ALREADY_STATE(kStatePrepared, MM_ERROR_SUCCESS);
    CHECK_STATE(kStateIdle);

    if ( mMpdUri.empty()) {
        ERROR("uri not set\n");
        return MM_ERROR_INVALID_URI;
    }

    SET_STATE(kStatePreparing);
    if ( postMsg(MSG_PREPARE, 0, 0, 0) ) {
        ERROR("failed to post\n");
        SET_STATE(kStateIdle);
        return MM_ERROR_NO_MEM;
    }

    FUNC_LEAVE();
    return MM_ERROR_ASYNC;
}

bool SegExtractor::ensureMediaSegment() {

    FUNC_ENTER();

    if (!mDashStream) {
        ERROR("segment stream is not provided");
        return false;
    }

    if (!mSegmentBuffer) {
        ERROR("segment buffer is not provided");
        return false;
    }

    if (!mSegmentBuffer->start()) {
        ERROR("segment buffer start fail");
        return false;
    }

    {
        MMAutoLock lock(mSegmentLock);

        mSegment = mNextPeriodSegment;
        mInitSegment = mNextPeriodInitSegment;

        if (!mSegment) {
            mSegment.reset(mSegmentBuffer->getNextSegment());
            mInitSegment = mSegmentBuffer->getInitSegment(mSegment.get());
        }
        mInitSegmentConsumed = false;
        mReadPos = 0;


        if (!mInitSegment) {
            mInitSegmentConsumed = true;
            INFO("init segment is not exist, media segment is self-initialized");
        }
    }

    if (!mSegment) {
        ERROR("get segment fail, init seg %p, seg %p", mInitSegment, mSegment.get());
        return false;
    }

    FUNC_LEAVE();
    return true;
}

void SegExtractor::onPrepare(param1_type param1, param2_type param2, uint32_t rspId)
{
    MMASSERT(rspId == 0);
    MMAutoLock lock(mLock);
    MMASSERT(mState == kStatePreparing);

    EXIT_TMHANDLER(false);
    if (!ensureMediaSegment())
        PREPARE_FAILED(MM_ERROR_NOT_INITED);

    mm_status_t ret = createContext();
    if ( ret !=  MM_ERROR_SUCCESS) {
        ERROR("failed to find stream info: %d\n", ret);
        PREPARE_FAILED(MM_ERROR_NOT_SUPPORTED_FILE);
    }

    mAVInputFormat = mAVFormatContext->iformat;
    VERBOSE("name: %s, long_name: %s, flags: %d, mime: %s\n",
        mAVInputFormat->name, mAVInputFormat->long_name, mAVInputFormat->flags, mAVInputFormat->mime_type);

    bool hasAudio = false;
    bool hasVideo = false;
    for (unsigned int i = 0; i < mAVFormatContext->nb_streams; ++i) {
        AVStream * s = mAVFormatContext->streams[i];
        AVMediaType type = s->codec->codec_type;
        VERBOSE("steramid: %d, codec_type: %d, start_time: %" PRId64 ", duration: %" PRId64 ", nb_frames: %" PRId64 "\n",
            i,
            s->codec->codec_type,
            s->start_time == (int64_t)AV_NOPTS_VALUE ? -1 : s->start_time,
            s->duration,
            s->nb_frames);
        StreamInfo * si;
        switch ( type ) {
            case AVMEDIA_TYPE_VIDEO:
                if (s->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                    INFO("ignore the video\n");
                    AVPacket * pkt = &s->attached_pic;
                    mMetaData->setString(MEDIA_ATTR_HAS_COVER, MEDIA_ATTR_YES);
                    mMetaData->setPointer(MEDIA_ATTR_ATTACHEDPIC, pkt->data);
                    mMetaData->setInt32(MEDIA_ATTR_ATTACHEDPIC_SIZE, (int32_t)pkt->size);
                    mMetaData->setInt32(MEDIA_ATTR_ATTACHEDPIC_CODECID, (int32_t)s->codec->codec_id);
                    mMetaData->setString(MEDIA_ATTR_ATTACHEDPIC_MIME, codecId2Mime((CowCodecID)s->codec->codec_id));
                    continue;
                }
                si = &mStreamInfoArray[kMediaTypeVideo];
                si->mMediaType = kMediaTypeVideo;
                hasVideo = true;
                INFO("found video stream(%d)\n", i);
                break;
            case AVMEDIA_TYPE_AUDIO:
                si = &mStreamInfoArray[kMediaTypeAudio];
                si->mMediaType = kMediaTypeAudio;
                hasAudio = true;
                INFO("found audio stream(%d)\n", i);
                break;
            case AVMEDIA_TYPE_SUBTITLE:
                si = &mStreamInfoArray[kMediaTypeSubtitle];
                si->mMediaType = kMediaTypeSubtitle;
                INFO("found subtitle stream(%d)\n", i);
                break;
            default:
                INFO("not supported mediatype: %d\n", type);
                continue;
        }

        si->mAllStreams.push_back(i);
        mStreamIdx2Info.insert(std::pair<int, StreamInfo*>(i, si));
    }

    if ( !hasAudio && !hasVideo ) {
        ERROR("no streams for play\n");
        PREPARE_FAILED(MM_ERROR_NOT_SUPPORTED_FILE);
    }

    AVDictionaryEntry *tag = NULL;
    if (NULL != mAVFormatContext->metadata) {
        while ((tag = av_dict_get(mAVFormatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            INFO("got meta: %s : %s\n", tag->key, tag->value);
            if ( !strcmp(tag->key, "album_artist") ) {
                mMetaData->setString(MEDIA_ATTR_ALBUM_ARTIST, tag->value);
            } else {
                mMetaData->setString(tag->key, tag->value);
            }
        }
    } else {
        MMLOGW("metadata in avformat is NULL\n");
    }

    int64_t durationMs;
    if ( duration(durationMs) == MM_ERROR_SUCCESS )
        mMetaData->setInt64(MEDIA_ATTR_DURATION, durationMs);
    else
        durationMs = -1;

    mReportedBufferingPercent = REPORTED_PERCENT_NONE;
    SET_STATE(kStatePrepared);
    NOTIFY(kEventInfo, kEventInfoSeekable, isSeekableInternal(), nilParam);
    // FIXME, int(durationMs)
    NOTIFY(kEventInfoDuration, int(durationMs), 0, nilParam);
    NOTIFY(kEventPrepareResult, MM_ERROR_SUCCESS, 0, nilParam);
    // disable all track by default, then we can enable the selected track in selectTrackInternal
    for (uint32_t i = 0; i < mAVFormatContext->nb_streams; i++)
        mAVFormatContext->streams[i]->discard = AVDISCARD_ALL;

    for ( int i = 0; i < kMediaTypeCount; ++i ) {
        StreamInfo * si = &mStreamInfoArray[i];
        if ( si->mMediaType != kMediaTypeUnknown ) {
            selectTrackInternal((MediaType)i, WANT_TRACK_INDEX_DEFAULT);
        }
    }

    if ( !strcmp(mAVInputFormat->long_name, "QuickTime / MOV") ) {
        const char * major_brand = NULL;
        if ( hasAudio && !hasVideo ) {
            if ( mMetaData->getString(MEDIA_ATTR_FILE_MAJOR_BAND, major_brand) ) {
                if ( strstr(major_brand, "3gp") ) {
                    mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_AUDIO_3GPP);
                } else if ( strstr(major_brand, "qt") ) {
                    mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_AUDIO_QUICKTIME);
                } else {
                    mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_AUDIO_MP4);
                }
            } else {
                mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_AUDIO_MP4);
            }
        } else {
            if ( mMetaData->getString(MEDIA_ATTR_FILE_MAJOR_BAND, major_brand) ) {
                if ( strstr(major_brand, "3gp") ) {
                    mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_VIDEO_3GPP);
                } else if ( strstr(major_brand, "qt") ) {
                    mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_VIDEO_QUICKTIME);
                } else {
                    mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_VIDEO_MP4);
                }
            } else {
                mMetaData->setString(MEDIA_ATTR_MIME, MEDIA_ATTR_MIME_VIDEO_MP4);
            }
        }
    } else {
        int idx = hasVideo ? kMediaTypeVideo : kMediaTypeAudio;
        const char * mime = NULL;
        // FIXME, redo it after user selectTrack
        if ( mStreamInfoArray[idx].mMetaData->getString(MEDIA_ATTR_MIME, mime) ) {
            mMetaData->setString(MEDIA_ATTR_MIME, mime);
        } else {
            mMetaData->setString(MEDIA_ATTR_MIME, "unknown");
        }
    }

    FUNC_LEAVE();
}

void SegExtractor::onReadSample(param1_type param1, param2_type param2, uint32_t rspId) {
    if (mState != kStateStarted) {
        return;
    }
    if ( mSeekUs != SEEK_NONE ) {
        if (!mSeekDone) {
            mm_status_t ret = mDashStream->seek(mSeekUs/1000);
            if (ret == MM_ERROR_SUCCESS) {
                {
                    mReadPos = 0;

                    {
                        MMAutoLock lock(mSegmentLock);
                        mNextPeriodSegment.reset();
                        mNextPeriodInitSegment = NULL;
                    }
                    reallocContext();
                }

                flushInternal();
                mSeekDone = true;
            } else {
                ERROR("seek fail");
            }
        }
        checkSeek();
    }

    mm_status_t status = readFrame();
    if (status != MM_ERROR_SUCCESS)
        INFO("read sample return status %d", status);

    int64_t timeout = 0;
    if (status == MM_ERROR_IO || status == MM_ERROR_TIMED_OUT)
        timeout = 100000;

    if (mEOS) {
        INFO("eos, onReadSample return");
        return;
    }

    MMAutoLock lock2(mBufferLock);
    int64_t min = INT64_MAX;
    if ( MM_UNLIKELY(!checkBuffer(min)) ) {
        INFO("not check buffer\n");
        postMsg(MSG_READ_SAMPLE, 0, 0, timeout);
        return;
    }

    if (min < mBufferingTimeHigh) {
        postMsg(MSG_READ_SAMPLE, 0, 0, timeout);
        return;
    }

    INFO("pause reading sample");
    mReadingSample = false;
}

mm_status_t SegExtractor::createContext()
{
    FUNC_ENTER();
    MMASSERT(mAVFormatContext == NULL);
    MMASSERT(mAVIOContext == NULL);

    mAVFormatContext = avformat_alloc_context();
    if ( !mAVFormatContext ) {
        ERROR("failed to create avcontext\n");
        return MM_ERROR_INVALID_PARAM;
    }

    unsigned char * ioBuf = (unsigned char*)av_malloc(AVIO_BUFFER_SIZE);
    if ( !ioBuf ) {
        ERROR("no mem\n");
        avformat_free_context(mAVFormatContext);
        mAVFormatContext = NULL;
        return MM_ERROR_NO_MEM;
    }

    mAVIOContext = avio_alloc_context(ioBuf,
                    AVIO_BUFFER_SIZE,
                    0,
                    this,
                    avRead,
                    NULL,
                    //avSeek);
                    NULL);

    if ( !mAVIOContext ) {
        ERROR("no mem\n");
        avformat_free_context(mAVFormatContext);
        mAVFormatContext = NULL;
        av_free(ioBuf);
        ioBuf = NULL;
        return MM_ERROR_NO_MEM;
    }

    mAVFormatContext->pb = mAVIOContext;

    if (!mAVFormatContext) {
        mAVFormatContext = avformat_alloc_context();
        if ( !mAVFormatContext ) {
            ERROR("failed to create avcontext\n");
            return MM_ERROR_INVALID_PARAM;
        }
    }

    mAVFormatContext->interrupt_callback = *mInterruptHandler;

    mInterruptHandler->start(mPrepareTimeout);
    const char *path = NULL;
    DEBUG("url: %s", PRINTABLE_STR(path));
    int ret = avformat_open_input(&mAVFormatContext, path, NULL, NULL);
    if ( ret < 0 ) {
        ERROR("failed to open input: %d\n", ret);
        mInterruptHandler->end();
        SET_STATE(kStateIdle);
        NOTIFY(kEventPrepareResult, MM_ERROR_INVALID_URI, 0, nilParam);
        return ret;
    }

    mAVFormatContext->flags |= AVFMT_FLAG_GENPTS;
    mAVFormatContext->flags |= AVFMT_FLAG_CUSTOM_IO;

    VERBOSE("finding stream info\n");
    ret = avformat_find_stream_info(mAVFormatContext, NULL);
    mInterruptHandler->end();
    if ( ret < 0 ) {
        ERROR("failed to find stream info: %d\n", ret);
        NOTIFY(kEventPrepareResult, ret, 0, nilParam);
        return ret;
    }

    return MM_ERROR_SUCCESS;
}

void SegExtractor::releaseContext()
{
    FUNC_ENTER();
    if ( mAVFormatContext ) {
        mAVFormatContext->interrupt_callback = {.callback = NULL, .opaque = NULL};
        mAVFormatContext->pb = NULL;
        avformat_free_context(mAVFormatContext);
        mAVFormatContext = NULL;
    }

    if ( mAVIOContext ) {
        if ( mAVIOContext->buffer ) {
            av_free(mAVIOContext->buffer);
            mAVIOContext->buffer = NULL;
        }
        av_free(mAVIOContext);
        mAVIOContext = NULL;
    }
}

mm_status_t SegExtractor::start()
{
    FUNC_ENTER();
    MMAutoLock lock(mLock);
    RET_IF_ALREADY_STATE(kStateStarted, MM_ERROR_SUCCESS);
    if ( mState != kStatePrepared/* &&
        mState != STATE_PAUSED*/ ) {
        ERROR("invalid sate(%d)\n", mState);
        return MM_ERROR_INVALID_STATE;
    }

    EXIT_TMHANDLER(false);
    if ( mState == kStatePrepared)
        mCheckVideoKeyFrame = hasMediaInternal(kMediaTypeVideo);
    MMAutoLock lock2(mBufferLock);
    SET_STATE(kStateStarted);

    if ( postMsg(MSG_READ_SAMPLE, 0, 0, 0) ) {
        ERROR("failed to post\n");
        SET_STATE(kStateIdle);
        return MM_ERROR_NO_MEM;
    }

    INFO("success\n");
    mReadingSample = true;

    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::stop()
{
    FUNC_ENTER();
    WAIT_PREPARE_OVER();

    MMAutoLock lock(mLock);
    RET_IF_ALREADY_STATE(kStatePrepared, MM_ERROR_SUCCESS);

    if ( mState != kStateStarted/* &&
        mState != STATE_PAUSED*/) {
        ERROR("invalid sate(%d)\n", mState);
        return MM_ERROR_INVALID_STATE;
    }

    stopInternal();
    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

void SegExtractor::stopInternal()
{
    FUNC_ENTER();

    EXIT_TMHANDLER(true);

    flush();
    MMAutoLock lock2(mBufferLock);
    while (!mSeekSequence.empty()) {
        mSeekSequence.pop();
    }
    mSeekUs = 0;
    SeekSequence sequence = {0, 1};
    mSeekSequence.push(sequence);
    SET_STATE(kStatePrepared);

    mNextPeriodSegment.reset();
    mNextPeriodInitSegment = NULL;
    FUNC_LEAVE();
}

mm_status_t SegExtractor::pause()
{
    FUNC_ENTER();
    VERBOSE("ignore\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::resume()
{
/*    FUNC_ENTER();
    return start();*/
    VERBOSE("ignore\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::seek(int msec, int seekSequence)
{
    FUNC_ENTER();
    VERBOSE("pos: %d\n", msec);
    if ( msec < 0 ) {
        ERROR("invalid pos: %d\n", msec);
        return MM_ERROR_INVALID_PARAM;
    }
    if ( msec == 0 ) {
        // FIXME: remove later
        if (mSelectedMediaType == kMediaTypeVideo)
        INFO("pos: %d\n", msec);
        return MM_ERROR_SUCCESS;
    }

    MMAutoLock lock(mLock);
    if ( mState < kStatePrepared) {
        ERROR("invalid sate(%d)\n", mState);
        return MM_ERROR_INVALID_STATE;
    }

    if ( !isSeekableInternal() ) {
        ERROR("not seekable\n");
        return MM_ERROR_IVALID_OPERATION;
    }

    int64_t usec = msec * 1000LL;
    if ( usec > durationUs() ) {
        if ((usec - durationUs()) > 500) {
            //this case will avoid in cowplayer seek.
            ERROR("invalid pos: %d, starttime: %" PRId64 ", dur: %" PRId64 "\n", msec, startTimeUs(), durationUs());
        }
        //seek this position to show the last frame
        usec = durationUs() - 1000;
    }

    // if we just 'notify' the latest seek, std::queue isn't necessary for mSeekSequence
    MMAutoLock lock2(mBufferLock);
    mSeekUs = usec + startTimeUs();
    mSeekDone = false;
    SeekSequence sequence = {(uint32_t)seekSequence, 0};
    mSeekSequence.push(sequence);
    INFO("req pos: %" PRId64 " -> %" PRId64 "\n", usec, mSeekUs);

    //flushInternal();

    if (!mReadingSample) {
        postMsg(MSG_READ_SAMPLE, 0, 0, 0);
        mReadingSample = true;
    }

    FUNC_LEAVE();
    return MM_ERROR_ASYNC;
}

bool SegExtractor::setTargetTimeUs(int64_t seekTimeUs)
{
    if (hasMediaInternal(kMediaTypeVideo)) {
        mStreamInfoArray[kMediaTypeVideo].mTargetTimeUs = seekTimeUs;
    }
    if (hasMediaInternal(kMediaTypeAudio)) {
        mStreamInfoArray[kMediaTypeAudio].mTargetTimeUs = seekTimeUs;
    }
    return true;
}

bool SegExtractor::checkBufferSeek(int64_t seekUs)
{
    MMASSERT((int64_t)AV_NOPTS_VALUE < 0);
    int i;
    int videoIndex = -1;
    int audioIndex = -1;
    for ( i = 0; i < kMediaTypeCount; ++i ) {
        StreamInfo * si = &mStreamInfoArray[i];
        if ( si->mMediaType == kMediaTypeUnknown ||
#ifndef __MM_YUNOS_HOSTONLY_BUILD__
            //FIXME: one invalid PTS/AV_NOPTS_VALUE make us not able to use av_seek_frame() to seek
            // see the bailongma.mp3
            si->mNotCheckBuffering ||
#endif
            !si->mPeerInstalled ||
            ( (mScaledPlayRate != SCALED_PLAY_RATE) && (i == kMediaTypeAudio))) {
            DEBUG("ignore: mediatype: %d, notcheck: %d, perrinstalled: %d, mScaledPlayRate %d\n",
                si->mMediaType, si->mNotCheckBuffering, si->mPeerInstalled, mScaledPlayRate);
            continue;
        }

        int64_t firstTs = si->bufferedFirstTs();
        if ( firstTs <= INVALID_TIME_STAMP ) {
            DEBUG("buffer seek not enough, mediatype: %d, firstTs: %" PRId64 "\n", si->mMediaType, firstTs);
            return false;
        }

        int64_t lastTs = si->bufferedLastTs();
        if ( seekUs < firstTs || seekUs > lastTs + MAX_BUFFER_SEEK ) {
            DEBUG("buffer seek not enough, mediatype: %d, firstTs: %" PRId64 ", lastTs: %" PRId64 ", reqto: %" PRId64 "\n", si->mMediaType, firstTs, lastTs, seekUs);
            if (seekUs < firstTs && (firstTs - seekUs) < 1000*500)
               INFO("seekUs is a little bit small than firstTs, let it go");
            else
               return false;
        }

        if ( si->mMediaType == kMediaTypeVideo )
            videoIndex = i;
        else if ( si->mMediaType == kMediaTypeAudio )
            audioIndex = i;
    }

    int64_t realSeekTs = 0;
    if ( videoIndex >= 0 && mStreamInfoArray[videoIndex].removeUntilTs(seekUs, &realSeekTs) ) {
        if ( audioIndex >= 0 ) {
            mStreamInfoArray[audioIndex].removeUntilTs(realSeekTs, &realSeekTs);
        }
    } else {
        if ( audioIndex >= 0 ) {
            mStreamInfoArray[audioIndex].removeUntilTs(seekUs, &realSeekTs);
        }
    }

    if ( videoIndex >= 0 ) {
        if ( mStreamInfoArray[videoIndex].bufferedFirstTs() > INVALID_TIME_STAMP ) {
            mCheckVideoKeyFrame = false;
        } else {
            mCheckVideoKeyFrame = true;
        }
    } else {
        mCheckVideoKeyFrame = false;
    }

    return true;
}

void SegExtractor::checkSeek()
{
    int64_t seekUs = SEEK_NONE ;
    std::queue<SeekSequence> seekSequence;
    mm_status_t status = MM_ERROR_SUCCESS;
    MMAutoLock lock2(mBufferLock);
    {
        if ( mSeekUs == SEEK_NONE ) {
            ASSERT(mSeekSequence.empty());
            return;
        }

        if ( !checkBufferSeek(mSeekUs) ) {
            DEBUG("no enough buffers for bufferSeek");
            return;
        }

        seekUs = mSeekUs;
        ASSERT(mSeekSequence.size());
        std::swap(seekSequence, mSeekSequence);
        mSeekUs = SEEK_NONE;
    }
#if 0
    do { // make it easier to break to the end of func
        if ( checkBufferSeek(seekUs) ) {
            DEBUG("checkBufferSeek is ok");
            break;
        }
        flushInternal();

        INFO("do seeking to mSeekUs: %" PRId64 ", seekUs: %" PRId64, mSeekUs, seekUs);
        mInterruptHandler->start(mSeekTimeout);
        int ret = av_seek_frame(mAVFormatContext, -1, seekUs, AVSEEK_FLAG_BACKWARD);
        mInterruptHandler->end();
        if ( ret < 0 ) {
            MMLOGW("seek failed, seek result: %d \n", ret);
            status = mInterruptHandler->isTimeout() ? MM_ERROR_TIMED_OUT : MM_ERROR_UNKNOWN;
            //If seek failed, don't reset mCheckVideoKeyFrame, just continue playing.
            mCheckVideoKeyFrame = false;
        }
    }while (0);
#endif
    while (seekSequence.size()) {
        MMParamSP mmparam;
        SeekSequence sequence = seekSequence.front();
        mmparam.reset(new MMParam);
        mmparam->writeInt32(sequence.index);
        seekSequence.pop();
        if (!sequence.internal)
            NOTIFY(kEventSeekComplete, status, 0, mmparam);
    }

    setTargetTimeUs(seekUs);
}

int64_t SegExtractor::startTimeUs()
{
    /*
    if (!mAVFormatContext || mAVFormatContext->start_time == (int64_t)AV_NOPTS_VALUE)
        return 0;
    return mAVFormatContext->start_time;
    */
    /*
     * av context start time is the first segment start time after init segment
     * .e.g. after seek to 120s, the new created av context's start time is the
     *       start time of the segment containing pts 120s
     */
    // Need to be mpd start time
    return 0;
}

int64_t SegExtractor::durationUs()
{
    //if (!mAVFormatContext || mAVFormatContext->duration == (int64_t)AV_NOPTS_VALUE)
        //return 0;
    //return mAVFormatContext->duration; //time base: AV_TIME_BASE
    int64_t durationMs;
    mm_status_t status = duration(durationMs);
    if (status != MM_ERROR_SUCCESS)
        return 0;
    return durationMs * 1000;
}

mm_status_t SegExtractor::reset()
{
    FUNC_ENTER();
    WAIT_PREPARE_OVER();

    MMAutoLock lock(mLock);
    switch ( mState ) {
        case kStatePreparing:
            MMASSERT(0);
            break;
        case kStateStarted:
//        case STATE_PAUSED:
            stopInternal();
        case kStatePrepared:
            VERBOSE("reseting\n");
            resetInternal();
            break;
        case kStateNone:
        case kStateError:
        case kStateIdle:
        default:
            INFO("state now is %d\n", mState);
            return MM_ERROR_SUCCESS;
    }

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t SegExtractor::flush()
{
    FUNC_ENTER();
    MMAutoLock lock(mBufferLock);
    return flushInternal();
}

mm_status_t SegExtractor::flushInternal()
{
    FUNC_ENTER();
    for ( int i = 0; i < kMediaTypeCount; ++i ) {
        StreamInfo * si = &mStreamInfoArray[i];
        if ( si->mMediaType == kMediaTypeUnknown )
            continue;
        MMLOGD("clear %s buffer[0].size %d, buffer[1].size %d\n",
            si->mMediaType == kMediaTypeVideo ? "Video" : "Audio",
            si->mBufferList[0].size(), si->mBufferList[1].size());
        si->mBufferList[0].clear();
        si->mBufferList[1].clear();

        //reset last pts when seeking
        si->mLastDts = (int64_t)AV_NOPTS_VALUE;
        si->mLastPts = (int64_t)AV_NOPTS_VALUE;
        si->mLargestPts = (int64_t)AV_NOPTS_VALUE;
    }

    mCheckVideoKeyFrame = hasMediaInternal(kMediaTypeVideo);
    INFO("flush buffer may clear the I video frame, so need to check video key frame\n");

    //mReadThread->read();

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

bool SegExtractor::checkPacketWritable(StreamInfo * si, AVPacket * packet) {
    if ( !si->mPeerInstalled ) {
        VERBOSE("mediatype %d, peerInstalled %d\n",
            si->mMediaType, si->mPeerInstalled);
        return false;
    }

    if (mScaledPlayRateCur != mScaledPlayRate) {
        if (mScaledPlayRate > mScaledThresholdRate && mScaledPlayRateCur <= mScaledThresholdRate) {
            mCheckVideoKeyFrame = true;
            INFO("playrate change %d->%d, need to check video key frame\n", mScaledPlayRate, mScaledPlayRateCur);
        }

        mScaledPlayRate = mScaledPlayRateCur;
    }

    if ( (mScaledPlayRate != SCALED_PLAY_RATE) && (si->mMediaType == kMediaTypeAudio)) {
        VERBOSE("mediatype %d, mScaledPlayRate %d\n",
            si->mMediaType, mScaledPlayRate);
        return false;
    }

    if (packet->stream_index != si->mSelectedStream)
        return false;

    if ((si->mMediaType == kMediaTypeVideo) && (packet->flags & AV_PKT_FLAG_KEY))
        VERBOSE("key frame\n");

    if (mScaledPlayRate > mScaledThresholdRate) {
        if (!(packet->flags & AV_PKT_FLAG_KEY)) {
            VERBOSE("playRate: %d, skip B or P frame\n", mScaledPlayRate);
            return false;
        }
    }

    return true;
}

int SegExtractor::avRead(uint8_t *buf, int buf_size)
{
    //MMAutoLock lock(mFileMutex);
    MediaObject *seg = NULL, *initSeg = NULL;
    ssize_t size = 0;

#ifdef DEBUG_AVIO
    INFO("pos is %d, read buf size %d", mReadPos, buf_size);
#endif

    INFO("");
read_retry:
    {
        // TODO use segment lock for mSegment and mInitSegment
        MMAutoLock lock(mSegmentLock);
        if (!mSegment) {
            INFO("init seg %p, seg %p, possibly we are about to play next period",
                 mSegment.get(), mInitSegment);
            return 0;
        }

        seg = mSegment.get();
        initSeg = mInitSegment;

        /* Peek call is prefered as segment can be reuse and peek unlimited times */
        if (!mInitSegmentConsumed) {
            size = initSeg->Peek(buf, buf_size, mReadPos);
            //size = initSeg->Read(buf, buf_size);
            if (!size) {
                mInitSegmentConsumed = true;
                mReadPos = 0;
            }

#ifdef DEBUG_AVIO
            INFO("read initialization segment, finished %d, init seg read pos %d",
                 mInitSegmentConsumed, mReadPos);
#endif
        }

    }

#ifdef DEBUG_AVIO
    INFO("download state is %d, read pos is %d", seg->getDownloadState(), mReadPos);
#endif

    /* Peek() is prefered than Read () as segment can be reuse and peek unlimited times */
    if (!size)
        //size = seg->Peek(buf, buf_size, mReadPos);
        size = seg->Read(buf, buf_size);

#ifdef DEBUG_AVIO
    INFO("read %d bytes", size);
#endif

#ifdef DUMP_INPUT
    encodedDataDump.dump(buf, size);
#endif

    if (size > 0)
        mReadPos += size;
    else if (size == 0) {
        bool representationChanged = false;
        playNextSegment_l(representationChanged);

        if (representationChanged)
            return size; // return 0 to signal AVERROR_EOF by av_read_frame() call
        else
            goto read_retry;
    }

    if (size == -1) {
        ERROR("read return error %s", strerror(errno));
    } else {
        VERBOSE("read return size %d, request size %d\n", size, buf_size);
    }
    return size;
}

/*static */int SegExtractor::avRead(void *opaque, uint8_t *buf, int buf_size)
{
    SegExtractor * me = static_cast<SegExtractor*>(opaque);
    if ( !me ) {
        ERROR("invalid cb\n");
        return -1;
    }

    return me->avRead(buf, buf_size);
}

/*static */int64_t SegExtractor::avSeek(void *opaque, int64_t offset, int whence)
{
    SegExtractor * me = static_cast<SegExtractor*>(opaque);
    if ( !me ) {
        ERROR("invalid cb\n");
        return -1;
    }

    if ( whence == SEEK_SET && offset < 0 ) {
        ERROR("seek from start, but offset < 0\n");
        return -1;
    }

    if ( whence == AVSEEK_SIZE ) {
        //INFO("AVSEEK_SIZE supported, file length %lld\n", me->mLength);
        //return me->mLength;
    }

    if ( whence == AVSEEK_FORCE ) {
        ERROR("%d not supported\n", whence);
        return -1;
    }


    return me->avSeek(offset, whence);
}

int64_t SegExtractor::avSeek(int64_t offset, int whence)
{
    //MMAutoLock lock(mFileMutex);
    return 0;
}

void SegExtractor::playNextSegment_l(bool &changed) {

    INFO("cur segment is fully extracted, about to go to next segment");

    changed = false;
    mReadPos = 0;

    MediaObject* seg = mSegmentBuffer->getNextSegment();
    if (!seg) {
        INFO("reach the end of media repersentation");
        changed = true;
        mEOF = true;
        return;
    }

    mSegment.reset(seg);
    seg = mSegmentBuffer->getInitSegment(mSegment.get());

    if (seg != mInitSegment) {
        INFO("media representation is changed");
        mNextPeriodSegment = mSegment;
        mNextPeriodInitSegment = seg;
        mInitSegment = seg;
        mSegment.reset();
        mInitSegmentConsumed = false;
        changed = true;
        mEOF = false;
    }
}

mm_status_t SegExtractor::readFrame()
{
    VERBOSE("+\n");

    MMAutoLock lock(mBufferLock);

    if ( mEOS ) {
        VERBOSE("no data to read\n");
        return MM_ERROR_NO_MORE;
    }

#define FREE_AVPACKET(_pkt) do {\
    if ( _pkt ) {\
        av_free_packet(_pkt);\
        free(_pkt);\
        (_pkt) = NULL;\
    }\
}while(0)

    AVPacket * packet = NULL;
    do {
        packet = (AVPacket*)malloc(sizeof(AVPacket));
        if ( !packet ) {
            ERROR("no mem\n");
            NOTIFY_ERROR(MM_ERROR_NO_MEM);
            return MM_ERROR_NO_MEM;
        }
        av_init_packet(packet);

        mInterruptHandler->start(READ_TIMEOUT_DEFAULT);
        lock.unlock();
        //int64_t readStartTime = ElapsedTimer::getUs();
        int ret = av_read_frame(mAVFormatContext, packet); //0: ok, <0: error/end
        //int64_t readCosts = ElapsedTimer::getUs() - readStartTime;
        lock.lock();
        mInterruptHandler->end();

        if ( ret < 0 ) {
            free(packet);
            packet = NULL;
            if ( ret == AVERROR_EOF ) {
                INFO("eof of media segment file\n");
                if (mEOF) {
                    INFO("eof of media presentation\n");
                    mEOS = true;
                    NOTIFY(kEventEOS, 0, 0, nilParam);
                    for ( int i = 0; i < kMediaTypeCount; ++i ) {
                        StreamInfo * s = &mStreamInfoArray[i];
                        if ( s->mMediaType == kMediaTypeUnknown )
                            continue;

                        MediaBufferSP buf = MediaBuffer::createMediaBuffer(MediaBuffer::MBT_ByteBuffer);
                        if ( !buf ) {
                            ERROR("failed to createMediaBuffer\n");
                            NOTIFY_ERROR(MM_ERROR_NO_MEM);
                            return MM_ERROR_NO_MEM;
                        }
                        INFO("write eos to media: %d\n", i);
                        buf->setFlag(MediaBuffer::MBFT_EOS);
                        buf->setSize(0);
                        //s->write(buf, s->mSelectedStream);
                        s->write(buf);
                    }

                    return MM_ERROR_NO_MORE;
                } else {
                    mm_status_t status = reallocContext();
                    if (status != MM_ERROR_SUCCESS)
                        return status;
                }
                return MM_ERROR_NO_MORE;
           }

            if ( mInterruptHandler->isTimeout() ) {
                ERROR("read timeout\n");
                NOTIFY_ERROR(MM_ERROR_TIMED_OUT);
                return MM_ERROR_TIMED_OUT;
            }

            ERROR("read_frame failed: %d\n", ret);
            NOTIFY_ERROR(MM_ERROR_IO);
            return MM_ERROR_IO;
        }

        std::map<int, StreamInfo*>::iterator it = mStreamIdx2Info.find(packet->stream_index);
        if ( it == mStreamIdx2Info.end() ) {
            WARNING("stream index %d not found, try next\n", packet->stream_index);
            FREE_AVPACKET(packet);
            usleep(0);
            continue;
        }

        StreamInfo * si = it->second;
        MMASSERT(si != NULL);

        //discard frames which no need to render
        if (!checkPacketWritable(si, packet)) {
            FREE_AVPACKET(packet);
            usleep(0);
            continue;
        }

        int64_t dtsDiff = 0;
        int64_t ptsDiff = 0;
        //Live stream is always not seekable
        if (isSeekableInternal() &&
            (packet->pts == (int64_t)AV_NOPTS_VALUE || packet->dts == (int64_t)AV_NOPTS_VALUE) &&
            (si->mLastPts != (int64_t)AV_NOPTS_VALUE && si->mLastDts != (int64_t)AV_NOPTS_VALUE)) {
                MMLOGW("invalid pts/dts, using the last pts/dts, pts: %" PRId64 ", dts:%" PRId64 ", last pts: %" PRId64 ", dts:%" PRId64 "\n",
                    packet->pts, packet->dts, si->mLastPts, si->mLastDts);

                //last pts is scaled time;
                packet->pts = av_rescale_q(si->mLastPts, gTimeBase, si->mTimeBase);
                packet->dts = av_rescale_q(si->mLastDts, gTimeBase, si->mTimeBase);
        }

        //If packet timestamp is -1, no need to checkHighWater
        int64_t lastDts = si->mLastDts;
        int64_t lastPts = si->mLargestPts;

        if (packet->pts != (int64_t)AV_NOPTS_VALUE){
            av_packet_rescale_ts(packet, si->mTimeBase, gTimeBase);

            if (si->mLastDts != AV_NOPTS_VALUE && si->mLastPts != AV_NOPTS_VALUE) {
                dtsDiff = packet->dts - si->mLastDts;
                ptsDiff = packet->pts - si->mLastPts;
                if ( MM_UNLIKELY(dtsDiff  > 1000000 || ptsDiff > 1000000  || dtsDiff <-1000000 || ptsDiff <-1000000) ) {
                    MMLOGW("stream: %d, diff too large, pkt ( %" PRId64 ",  %" PRId64 "), last ( %" PRId64 ", %" PRId64 ")",
                        si->mMediaType, packet->dts, packet->pts, si->mLastDts, si->mLastPts);
                }
            }

            packet->dts += si->mTimeOffsetUs;
            packet->pts += si->mTimeOffsetUs;

            si->mLastDts = packet->dts;
            si->mLastPts = packet->pts;
            if (packet->pts > si->mLargestPts)
                si->mLargestPts = packet->pts;

            if ( packet->duration > 0 ) {
                //checkHighWater(readCosts, packet->duration);
            }
        }

        if (!(si->mMediaTimeCalibrated))
            calibrateMediaPresentationTime_l(si, packet, lastPts, lastDts);

        si->mPacketCount++;

        MediaBufferSP buf = AVBufferHelper::createMediaBuffer(packet, true);
        if ( !buf ) {
            ERROR("failed to createMediaBuffer\n");
            FREE_AVPACKET(packet);
            NOTIFY_ERROR(MM_ERROR_NO_MEM);
            return MM_ERROR_NO_MEM;
        }

        //check
        if ( MM_UNLIKELY(mCheckVideoKeyFrame) ) {
            if (si->mMediaType == kMediaTypeVideo &&
                buf->isFlagSet(MediaBuffer::MBFT_KeyFrame)) {
                mCheckVideoKeyFrame = false;
                MMLOGD("get first video key frame after seek\n");
            } else {
                // FIXME, move it to checkPacketWritable()
                //Audio Frame, frame before first video key frame;
                //Video Frame, frame before first key frame;
                MMLOGD("skip %s frame before got first key video frame\n",
                    si->mMediaType == kMediaTypeVideo ? "Video" : "Audio");
                return MM_ERROR_SUCCESS; //continue buffering
            }
        }


        MMLOGD("packet: media: %s, seq: %d, pts: %0.3f , dts: %0.3f, data: %p, size: %d, stream_index: %d, flags: %d, duration: %" PRId64 ", pos: %" PRId64 ", convergence_duration: %" PRId64 ", num: %d, den: %d, dts diff: %" PRId64 ", last dts: %0.3f\n",
            si->mMediaType == kMediaTypeVideo ? "Video" : "Audio",
            si->mPacketCount,
            packet->pts/1000000.0f,
            packet->dts/1000000.0f,
            packet->data,
            packet->size,
            packet->stream_index,
            packet->flags,
            packet->duration,
            packet->pos,
            packet->convergence_duration,
            mAVFormatContext->streams[packet->stream_index]->time_base.num,
            mAVFormatContext->streams[packet->stream_index]->time_base.den,
            dtsDiff,
            si->mLastDts/1000000.0f
            );
        if (si->mMediaType == kMediaTypeVideo && packet->data && packet->data[0] != 0) {
            MMLOGW("video buffer size error");
        }

        //si->write(buf, packet->stream_index);
        si->write(buf);

    } while(0);

    return MM_ERROR_SUCCESS;
}

bool SegExtractor::checkBuffer(int64_t & min)
{
    min = INT64_MAX;
    bool check = false;
    for ( int i = 0; i < kMediaTypeCount; ++i ) {
        StreamInfo * si = &mStreamInfoArray[i];
        if ( si->mMediaType == kMediaTypeUnknown ||
            si->mNotCheckBuffering ||
            !si->mPeerInstalled ||
            ( (mScaledPlayRate != SCALED_PLAY_RATE) && (i == kMediaTypeAudio))) {
            VERBOSE("ignore: mediatype: %d, notcheck: %d, perrinstalled: %d, mScaledPlayRate : %d\n",
                si->mMediaType, si->mNotCheckBuffering, si->mPeerInstalled, mScaledPlayRate);
            continue;
        }

        int64_t j = si->bufferedTime();
        if ( min > j ) {
            VERBOSE("min change from %" PRId64 " to %" PRId64 "(mediatype: %d)\n", min, j, si->mMediaType);
            min = j;
        }
        check = true;
    }

    VERBOSE("need check: %d\n", check);
    return check;
}

/*static */snd_format_t SegExtractor::convertAudioFormat(AVSampleFormat avFormat)
{
#undef item
#define item(_av, _audio) \
    case _av:\
        INFO("%s -> %s\n", #_av, #_audio);\
        return _audio

    switch ( avFormat ) {
        item(AV_SAMPLE_FMT_U8, SND_FORMAT_PCM_8_BIT);
        item(AV_SAMPLE_FMT_S16, SND_FORMAT_PCM_16_BIT);
        item(AV_SAMPLE_FMT_S32, SND_FORMAT_PCM_32_BIT);
        item(AV_SAMPLE_FMT_FLT, SND_FORMAT_PCM_32_BIT);
        item(AV_SAMPLE_FMT_DBL, SND_FORMAT_PCM_32_BIT);
        item(AV_SAMPLE_FMT_U8P, SND_FORMAT_PCM_8_BIT);
        item(AV_SAMPLE_FMT_S16P, SND_FORMAT_PCM_16_BIT);
        item(AV_SAMPLE_FMT_S32P, SND_FORMAT_PCM_32_BIT);
        item(AV_SAMPLE_FMT_FLTP, SND_FORMAT_PCM_32_BIT);
        item(AV_SAMPLE_FMT_DBLP, SND_FORMAT_PCM_32_BIT);
        default:
            VERBOSE("%d -> AUDIO_SAMPLE_INVALID\n", avFormat);
            return SND_FORMAT_INVALID;
    }
}

mm_status_t SegExtractor::reallocContext() {
    FUNC_ENTER();
    INFO("assume media representation is changed, need to re-create av context");
    releaseContext();

    if (!ensureMediaSegment()) {
        ERROR("ensure media segment fail");
        return MM_ERROR_IO;
    }
    mm_status_t ret = createContext();
    if ( ret !=  MM_ERROR_SUCCESS) {
        ERROR("fail to create av context: %d\n", ret);
        NOTIFY_ERROR(MM_ERROR_IO);
        return MM_ERROR_IO;
    }

    updateCurrentPeriod();

    for ( int i = 0; i < kMediaTypeCount; ++i ) {
        StreamInfo * si = &mStreamInfoArray[i];
        if ( si->mMediaType == kMediaTypeUnknown )
            continue;

        //si->mLastDts = (int64_t)AV_NOPTS_VALUE;
        //si->mLastPts = (int64_t)AV_NOPTS_VALUE;
        //si->mLargestPts = (int64_t)AV_NOPTS_VALUE;
        si->mMediaTimeCalibrated = false;
        si->mTimeOffsetUs = 0;;
    }

    return MM_ERROR_SUCCESS;
}

void SegExtractor::updateCurrentPeriod() {
    int period;
    int64_t duration;
    mm_status_t status = mDashStream->getCurrentSegmentStart(period, mPlaybackStartUs, duration);
    if (status != MM_ERROR_SUCCESS) {
        WARNING("fail to get curent period start");
        mPlaybackStartUs = INVALID_TIME_STAMP;
    }

    INFO("current period: index %d start %" PRId64 " duration %" PRId64 "",
         period, mPlaybackStartUs, duration);
}

void SegExtractor::calibrateMediaPresentationTime_l(StreamInfo *si,
                                                    AVPacket* packet,
                                                    int64_t lastPts,
                                                    int64_t lastDts) {

    if (!si || !packet) {
        ERROR("si is %p, av packet is %p", si, packet);
        return;
    }

    si->mMediaTimeCalibrated = true;

    INFO("calibrate media time: pts %" PRId64 " dts %" PRId64 ", last pts %" PRId64 " last dts %" PRId64 ", period start %" PRId64 "",
        packet->pts, packet->dts, lastPts, lastDts, mPlaybackStartUs);

    if (mPlaybackStartUs == INVALID_TIME_STAMP) {
        WARNING("bad timestamp");
        return;
    }

    if (packet->pts == (int64_t)AV_NOPTS_VALUE ||
        packet->dts == (int64_t)AV_NOPTS_VALUE) {
        WARNING("bad timestamp");
        return;
    }

    if (labs((long)(packet->pts - mPlaybackStartUs)) < 1000 * 300) {
        INFO("media time is continuous");
        si->mTimeOffsetUs = 0;
        return;
    }

    if (lastPts == (int64_t)AV_NOPTS_VALUE) {
        INFO("lastPts is invalid, it was due to seek");
        si->mTimeOffsetUs = mPlaybackStartUs - packet->pts;
    } else {
        si->mTimeOffsetUs = mPlaybackStartUs - packet->pts;
        //si->mTimeOffsetUs = (lastDts -  packet->pts) + ;
    }

    packet->dts += si->mTimeOffsetUs;
    packet->pts += si->mTimeOffsetUs;

    INFO("time offset is %" PRId64 "", si->mTimeOffsetUs);
    si->mLastDts = packet->dts;
    si->mLastPts = packet->pts;
    if (packet->pts > si->mLargestPts)
        si->mLargestPts = packet->pts;
}

CowCodecID SegExtractor::AVCodecId2CodecId(AVCodecID id)
{
#undef item
#define item(_avid, _cowid) \
        case _avid:\
            INFO("%s -> %s\n", #_avid, #_cowid);\
            return _cowid

        switch ( id ) {
            item(AV_CODEC_ID_NONE, kCodecIDNONE);
            item(AV_CODEC_ID_MPEG1VIDEO, kCodecIDMPEG1VIDEO);
            item(AV_CODEC_ID_MPEG2VIDEO, kCodecIDMPEG2VIDEO);
            item(AV_CODEC_ID_H261, kCodecIDH261);
            item(AV_CODEC_ID_H263, kCodecIDH263);
            item(AV_CODEC_ID_RV10, kCodecIDRV10);
            item(AV_CODEC_ID_RV20, kCodecIDRV20);
            item(AV_CODEC_ID_MPEG4, kCodecIDMPEG4);
            item(AV_CODEC_ID_MSMPEG4V1, kCodecIDMSMPEG4V1);
            item(AV_CODEC_ID_MSMPEG4V2, kCodecIDMSMPEG4V2);
            item(AV_CODEC_ID_MSMPEG4V3, kCodecIDMSMPEG4V3);
            item(AV_CODEC_ID_WMV1, kCodecIDWMV1);
            item(AV_CODEC_ID_WMV2, kCodecIDWMV2);
            item(AV_CODEC_ID_H263P, kCodecIDH263P);
            item(AV_CODEC_ID_H263I, kCodecIDH263I);
            item(AV_CODEC_ID_DVVIDEO, kCodecIDDVVIDEO);
            item(AV_CODEC_ID_H264, kCodecIDH264);
            item(AV_CODEC_ID_THEORA, kCodecIDTHEORA);
            item(AV_CODEC_ID_MSVIDEO1, kCodecIDMSVIDEO1);
            item(AV_CODEC_ID_VC1, kCodecIDVC1);
            item(AV_CODEC_ID_WMV3, kCodecIDWMV3);
            item(AV_CODEC_ID_AVS, kCodecIDAVS);
            item(AV_CODEC_ID_KMVC, kCodecIDKMVC);
            item(AV_CODEC_ID_FLASHSV, kCodecIDFLASHSV);
            item(AV_CODEC_ID_CAVS, kCodecIDCAVS);
            item(AV_CODEC_ID_VP8, kCodecIDVP8);
            item(AV_CODEC_ID_VP9, kCodecIDVP9);
            item(AV_CODEC_ID_HEVC, kCodecIDHEVC);
            item(AV_CODEC_ID_PCM_S16LE, kCodecIDPCM_S16LE);
            item(AV_CODEC_ID_PCM_S16BE, kCodecIDPCM_S16BE);
            item(AV_CODEC_ID_PCM_U16LE, kCodecIDPCM_U16LE);
            item(AV_CODEC_ID_PCM_U16BE, kCodecIDPCM_U16BE);
            item(AV_CODEC_ID_PCM_S8, kCodecIDPCM_S8);
            item(AV_CODEC_ID_PCM_U8, kCodecIDPCM_U8);
            item(AV_CODEC_ID_PCM_MULAW, kCodecIDPCM_MULAW);
            item(AV_CODEC_ID_PCM_ALAW, kCodecIDPCM_ALAW);
            item(AV_CODEC_ID_PCM_S32LE, kCodecIDPCM_S32LE);
            item(AV_CODEC_ID_PCM_S32BE, kCodecIDPCM_S32BE);
            item(AV_CODEC_ID_PCM_U32LE, kCodecIDPCM_U32LE);
            item(AV_CODEC_ID_PCM_U32BE, kCodecIDPCM_U32BE);
            item(AV_CODEC_ID_PCM_S24LE, kCodecIDPCM_S24LE);
            item(AV_CODEC_ID_PCM_S24BE, kCodecIDPCM_S24BE);
            item(AV_CODEC_ID_PCM_U24LE, kCodecIDPCM_U24LE);
            item(AV_CODEC_ID_PCM_U24BE, kCodecIDPCM_U24BE);
            item(AV_CODEC_ID_PCM_S24DAUD, kCodecIDPCM_S24DAUD);
            item(AV_CODEC_ID_PCM_ZORK, kCodecIDPCM_ZORK);
            item(AV_CODEC_ID_PCM_S16LE_PLANAR, kCodecIDPCM_S16LE_PLANAR);
            item(AV_CODEC_ID_PCM_DVD, kCodecIDPCM_DVD);
            item(AV_CODEC_ID_PCM_F32BE, kCodecIDPCM_F32BE);
            item(AV_CODEC_ID_PCM_F32LE, kCodecIDPCM_F32LE);
            item(AV_CODEC_ID_PCM_F64BE, kCodecIDPCM_F64BE);
            item(AV_CODEC_ID_PCM_F64LE, kCodecIDPCM_F64LE);
            item(AV_CODEC_ID_PCM_BLURAY, kCodecIDPCM_BLURAY);
            item(AV_CODEC_ID_PCM_LXF, kCodecIDPCM_LXF);
            item(AV_CODEC_ID_S302M, kCodecIDS302M);
            item(AV_CODEC_ID_PCM_S8_PLANAR, kCodecIDPCM_S8_PLANAR);
            item(AV_CODEC_ID_PCM_S24LE_PLANAR, kCodecIDPCM_S24LE_PLANAR);
            item(AV_CODEC_ID_PCM_S32LE_PLANAR, kCodecIDPCM_S32LE_PLANAR);
            item(AV_CODEC_ID_PCM_S16BE_PLANAR, kCodecIDPCM_S16BE_PLANAR);
            item(AV_CODEC_ID_PCM_S64LE, kCodecIDPCM_S64LE);
            item(AV_CODEC_ID_PCM_S64BE, kCodecIDPCM_S64BE);
            item(AV_CODEC_ID_ADPCM_IMA_QT, kCodecIDADPCM_IMA_QT);
            item(AV_CODEC_ID_ADPCM_IMA_WAV, kCodecIDADPCM_IMA_WAV);
            item(AV_CODEC_ID_ADPCM_IMA_DK3, kCodecIDADPCM_IMA_DK3);
            item(AV_CODEC_ID_ADPCM_IMA_DK4, kCodecIDADPCM_IMA_DK4);
            item(AV_CODEC_ID_ADPCM_IMA_WS, kCodecIDADPCM_IMA_WS);
            item(AV_CODEC_ID_ADPCM_IMA_SMJPEG, kCodecIDADPCM_IMA_SMJPEG);
            item(AV_CODEC_ID_ADPCM_MS, kCodecIDADPCM_MS);
            item(AV_CODEC_ID_ADPCM_4XM, kCodecIDADPCM_4XM);
            item(AV_CODEC_ID_ADPCM_XA, kCodecIDADPCM_XA);
            item(AV_CODEC_ID_ADPCM_ADX, kCodecIDADPCM_ADX);
            item(AV_CODEC_ID_ADPCM_EA, kCodecIDADPCM_EA);
            item(AV_CODEC_ID_ADPCM_G726, kCodecIDADPCM_G726);
            item(AV_CODEC_ID_ADPCM_CT, kCodecIDADPCM_CT);
            item(AV_CODEC_ID_ADPCM_SWF, kCodecIDADPCM_SWF);
            item(AV_CODEC_ID_ADPCM_YAMAHA, kCodecIDADPCM_YAMAHA);
            item(AV_CODEC_ID_ADPCM_SBPRO_4, kCodecIDADPCM_SBPRO_4);
            item(AV_CODEC_ID_ADPCM_SBPRO_3, kCodecIDADPCM_SBPRO_3);
            item(AV_CODEC_ID_ADPCM_SBPRO_2, kCodecIDADPCM_SBPRO_2);
            item(AV_CODEC_ID_ADPCM_THP, kCodecIDADPCM_THP);
            item(AV_CODEC_ID_ADPCM_IMA_AMV, kCodecIDADPCM_IMA_AMV);
            item(AV_CODEC_ID_ADPCM_EA_R1, kCodecIDADPCM_EA_R1);
            item(AV_CODEC_ID_ADPCM_EA_R3, kCodecIDADPCM_EA_R3);
            item(AV_CODEC_ID_ADPCM_EA_R2, kCodecIDADPCM_EA_R2);
            item(AV_CODEC_ID_ADPCM_IMA_EA_SEAD, kCodecIDADPCM_IMA_EA_SEAD);
            item(AV_CODEC_ID_ADPCM_IMA_EA_EACS, kCodecIDADPCM_IMA_EA_EACS);
            item(AV_CODEC_ID_ADPCM_EA_XAS, kCodecIDADPCM_EA_XAS);
            item(AV_CODEC_ID_ADPCM_EA_MAXIS_XA, kCodecIDADPCM_EA_MAXIS_XA);
            item(AV_CODEC_ID_ADPCM_IMA_ISS, kCodecIDADPCM_IMA_ISS);
            item(AV_CODEC_ID_ADPCM_G722, kCodecIDADPCM_G722);
            item(AV_CODEC_ID_ADPCM_IMA_APC, kCodecIDADPCM_IMA_APC);
            item(AV_CODEC_ID_ADPCM_VIMA, kCodecIDADPCM_VIMA);
            item(AV_CODEC_ID_ADPCM_AFC, kCodecIDADPCM_AFC);
            item(AV_CODEC_ID_ADPCM_IMA_OKI, kCodecIDADPCM_IMA_OKI);
            item(AV_CODEC_ID_ADPCM_DTK, kCodecIDADPCM_DTK);
            item(AV_CODEC_ID_ADPCM_IMA_RAD, kCodecIDADPCM_IMA_RAD);
            item(AV_CODEC_ID_ADPCM_G726LE, kCodecIDADPCM_G726LE);
            item(AV_CODEC_ID_ADPCM_THP_LE, kCodecIDADPCM_THP_LE);
            item(AV_CODEC_ID_ADPCM_PSX, kCodecIDADPCM_PSX);
            item(AV_CODEC_ID_ADPCM_AICA, kCodecIDADPCM_AICA);
            item(AV_CODEC_ID_ADPCM_IMA_DAT4, kCodecIDADPCM_IMA_DAT4);
            item(AV_CODEC_ID_ADPCM_MTAF, kCodecIDADPCM_MTAF);
            item(AV_CODEC_ID_AMR_NB, kCodecIDAMR_NB);
            item(AV_CODEC_ID_AMR_WB, kCodecIDAMR_WB);
            item(AV_CODEC_ID_MP2, kCodecIDMP2);
            item(AV_CODEC_ID_MP3, kCodecIDMP3);
            item(AV_CODEC_ID_AAC, kCodecIDAAC);
            item(AV_CODEC_ID_AC3, kCodecIDAC3);
            item(AV_CODEC_ID_DTS, kCodecIDDTS);
            item(AV_CODEC_ID_VORBIS, kCodecIDVORBIS);
            item(AV_CODEC_ID_DVAUDIO, kCodecIDDVAUDIO);
            item(AV_CODEC_ID_WMAV1, kCodecIDWMAV1);
            item(AV_CODEC_ID_WMAV2, kCodecIDWMAV2);
            item(AV_CODEC_ID_VMDAUDIO, kCodecIDVMDAUDIO);
            item(AV_CODEC_ID_FLAC, kCodecIDFLAC);
            item(AV_CODEC_ID_MP3ADU, kCodecIDMP3ADU);
            item(AV_CODEC_ID_MP3ON4, kCodecIDMP3ON4);
            item(AV_CODEC_ID_SHORTEN, kCodecIDSHORTEN);
            item(AV_CODEC_ID_ALAC, kCodecIDALAC);
            item(AV_CODEC_ID_EAC3, kCodecIDEAC3);
            item(AV_CODEC_ID_MP1, kCodecIDMP1);
            item(AV_CODEC_ID_MP4ALS, kCodecIDMP4ALS);
            item(AV_CODEC_ID_AAC_LATM, kCodecIDAAC_LATM);
            item(AV_CODEC_ID_G723_1, kCodecIDG723_1);
            item(AV_CODEC_ID_G729, kCodecIDG729);
            item(AV_CODEC_ID_PAF_AUDIO, kCodecIDPAF_AUDIO);
            item(AV_CODEC_ID_ON2AVC, kCodecIDON2AVC);
            item(AV_CODEC_ID_DVD_SUBTITLE, kCodecIDDVD_SUBTITLE);
            item(AV_CODEC_ID_DVB_SUBTITLE, kCodecIDDVB_SUBTITLE);
            item(AV_CODEC_ID_TEXT, kCodecIDTEXT);
            item(AV_CODEC_ID_XSUB, kCodecIDXSUB);
            item(AV_CODEC_ID_SSA, kCodecIDSSA);
            item(AV_CODEC_ID_MOV_TEXT, kCodecIDMOV_TEXT);
            item(AV_CODEC_ID_MPEG2TS, kCodecIDMPEG2TS);
            default:
                VERBOSE("%d -> AV_CODEC_ID_NONE\n", id);
                return kCodecIDNONE;
        }

}

extern "C" {

using namespace YUNOS_MM;

MM_LOG_DEFINE_MODULE_NAME("SegExtractorCreater");

Component * createComponent(const char* mimeType, bool isEncoder)
{
    FUNC_ENTER();
    SegExtractor * extractor = new SegExtractor(mimeType);
    if ( !extractor ) {
        ERROR("no mem\n");
        return NULL;
    }

    INFO("ret: %p\n", extractor);
    return extractor;
}

void releaseComponent(Component * component)
{
    INFO("%p\n", component);
    if ( component ) {
        SegExtractor * extractor = DYNAMIC_CAST<SegExtractor*>(component);
        MMASSERT(extractor != NULL);
        MM_RELEASE(extractor);
    }
}
}

}

