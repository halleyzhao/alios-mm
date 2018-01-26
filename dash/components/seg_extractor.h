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

#ifndef __seg_extractor_H
#define __seg_extracotr_H

extern "C" {
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#include <inttypes.h>
#include <libavformat/avformat.h>
}

#include <vector>

#include <multimedia/component.h>
#include <multimedia/mmmsgthread.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/elapsedtimer.h>
#include <multimedia/codec.h>
#include "multimedia/mm_audio.h"
#include <queue>

#include <MediaObject.h>

#include <SegmentBuffer.h>
#include <DashStream.h>

#include <libdash.h>
#include <IMPD.h>

#define DUMP_INPUT

namespace YUNOS_MM {

class SegExtractor : public DashSourceComponent, public MMMsgThread {
public:
    SegExtractor(const char* mimeType);
    virtual ~SegExtractor();

public:
    virtual mm_status_t init();
    virtual void uninit();

    virtual const std::list<std::string> & supportedProtocols() const;
    virtual bool isSeekable();
    virtual mm_status_t getDuration(int64_t & durationMs);
    virtual bool hasMedia(MediaType mediaType);
    virtual MediaMetaSP getMetaData();

    virtual MMParamSP getMediaRepresentationInfo();
    virtual mm_status_t selectMediaRepresentation(int peroid, MediaType mediaType, int adaptationSet, int index);
    virtual int getSelectedMediaRepresentation(int peroid, MediaType mediaType, int &adaptationSet);

    virtual MMParamSP getTrackInfo();
    virtual mm_status_t selectTrack(MediaType mediaType, int index);
    virtual int getSelectedTrack(MediaType mediaType);

public:
    virtual const char * name() const { return "SegExtractor"; }
    COMPONENT_VERSION;

    virtual ReaderSP getReader(MediaType mediaType);
    virtual mm_status_t addSink(Component * component, MediaType mediaType) { return MM_ERROR_IVALID_OPERATION; }

    virtual mm_status_t setParameter(const MediaMetaSP & meta);
    virtual mm_status_t getParameter(MediaMetaSP & meta) const;
    virtual mm_status_t prepare();
    virtual mm_status_t start();
    virtual mm_status_t stop();
    virtual mm_status_t pause();
    virtual mm_status_t resume();
    virtual mm_status_t seek(int msec, int seekSequence);
    virtual mm_status_t reset();
    virtual mm_status_t flush();
    virtual mm_status_t drain() { return MM_ERROR_UNSUPPORTED; }

    virtual mm_status_t setUri(const char * uri,
                            const std::map<std::string, std::string> * headers = NULL);
    virtual mm_status_t setUri(int fd, int64_t offset, int64_t length);

    virtual mm_status_t setMPD(dash::mpd::IMPD *mpd);
    virtual dash::mpd::IMPD *getMPD();

private:
    enum State {
        kStateNone,
        kStateError,
        kStateIdle,
        kStatePreparing,
        kStatePrepared,
        kStateStarting,
        kStateStarted
    };

    class InterruptHandler : public AVIOInterruptCB {
    public:
        InterruptHandler(SegExtractor* extractor);
        ~InterruptHandler();

        void start(int64_t timeout);
        void end();
        void exit(bool isExit);
        bool isExiting() const;
        bool isTimeout() const;

        static int handleTimeout(void* obj);

    private:
        bool mIsTimeout;
        ElapsedTimer mTimer;
        bool mExit;
        bool mActive;

        MM_DISALLOW_COPY(InterruptHandler)
        DECLARE_LOGTAG()
    };

    typedef std::list<MediaBufferSP> BufferList;

    struct StreamInfo {
        StreamInfo();
        ~StreamInfo();

        bool init();
        void reset();

        int64_t bufferedTime() const;
        int64_t bufferedFirstTs(bool isPendingList = false) const;
        int64_t bufferedLastTs(bool isPendingList = false) const;
        int64_t bufferedFirstTs_l(bool isPendingList = false) const;
        int64_t bufferedLastTs_l(bool isPendingList = false) const;
        bool removeUntilTs(int64_t ts, int64_t * realTs);
        //bool shortCircuitSelectTrack(int trackIndex);

        mm_status_t read(MediaBufferSP & buffer);

        /* dash doesn't use track index to select reperesentation */
        bool write(MediaBufferSP buffer);

        mutable Lock mLock;
        MediaType mMediaType;
        int mSelectedStream;
        int mSelectedStreamPending;
        std::vector<int> mAllStreams;
        BufferList mBufferList[2];
        uint32_t mCurrentIndex; // index for current mBufferList
        bool mBeginWritePendingList;
        bool mHasAttachedPic;
        bool mNotCheckBuffering;
        AVRational mTimeBase;
        bool mPeerInstalled;
        bool mMediaTimeCalibrated;

        int mPacketCount;
        int64_t mLastDts;
        int64_t mLastPts;
        int64_t mLargestPts;
        int64_t mTimeOffsetUs;

        MediaMetaSP mMetaData;

        int64_t mTargetTimeUs;
    };

    struct SeekSequence {
        uint32_t index;
        bool internal;
    };

    class SegmentReader : public Reader {
    public:
        SegmentReader(SegExtractor * from, StreamInfo * si);
        ~SegmentReader();

    public:
        virtual mm_status_t read(MediaBufferSP & buffer);
        virtual MediaMetaSP getMetaData();

    private:
        SegExtractor * mComponent;
        StreamInfo * mStreamInfo;

        DECLARE_LOGTAG()
    };

protected:
    bool hasMediaInternal(MediaType mediaType);
    mm_status_t selectTrackInternal(MediaType mediaType, int index);
    mm_status_t readFrame();

    bool checkPacketWritable(StreamInfo * si, AVPacket * packet);

    mm_status_t read(MediaBufferSP & buffer, StreamInfo * si);
    void checkSeek();
    bool checkBufferSeek(int64_t seekUs);
    bool setTargetTimeUs(int64_t seekTimeUs);
    mm_status_t flushInternal();
    int64_t startTimeUs();
    int64_t durationUs();
    bool isSeekableInternal();
    void stopInternal();
    void resetInternal();
    mm_status_t duration(int64_t &durationMs);
    bool checkBuffer(int64_t & min);
    mm_status_t extractH264CSD(const uint8_t* data, size_t size, MediaBufferSP & sps, MediaBufferSP & pps);
    mm_status_t extractAACCSD(int32_t profile, int32_t sr, int32_t channel_configuration, MediaBufferSP & pps);
    mm_status_t createContext();
    void releaseContext();

    int avRead(uint8_t *buf, int buf_size);
    static int avRead(void *opaque, uint8_t *buf, int buf_size);
    static int64_t avSeek(void *opaque, int64_t offset, int whence);
    int64_t avSeek(int64_t offset, int whence);

    static const std::list<std::string> & getSupportedProtocols();
    static CowCodecID AVCodecId2CodecId(AVCodecID id);

    static snd_format_t convertAudioFormat(AVSampleFormat avFormat);

    int getConfigBufferingTime();
    bool ensureMediaSegment();
    void resetDash();
    void playNextSegment_l(bool &changed);
    mm_status_t reallocContext();
    void updateCurrentPeriod();

    void calibrateMediaPresentationTime_l(StreamInfo *si,
                                          AVPacket* packet,
                                          int64_t lastPts,
                                          int64_t lastDts);

private:
    Lock mLock;
    Lock mSegmentLock;
    Lock mBufferLock;
    State mState;
    std::string mMpdUri;
    AVFormatContext * mAVFormatContext;
    AVInputFormat * mAVInputFormat;
    AVIOContext * mAVIOContext;

    // MediaObject and its ISegment need to mem-free manually
    MMSharedPtr<libdash::framework::input::MediaObject> mSegment;
    libdash::framework::input::MediaObject              *mInitSegment;
    MMSharedPtr<libdash::framework::input::MediaObject> mNextPeriodSegment;
    libdash::framework::input::MediaObject              *mNextPeriodInitSegment;
    YUNOS_DASH::SegmentBuffer* mSegmentBuffer;

    size_t mReadPos;
    bool mDashCreateByUs;
    bool mInitSegmentConsumed;

    YUNOS_DASH::DashStream *mDashStream;

    int mPrepareTimeout;
    int mSeekTimeout;
    int mReadTimeout;
    int64_t mBufferingTime;
    int64_t mBufferingTimeHigh;
    int32_t mScaledPlayRate;
    int32_t mScaledThresholdRate;
    int32_t mScaledPlayRateCur;

    InterruptHandler * mInterruptHandler;
    bool mEOS;
    bool mEOF;
    bool mReadingSample;

    StreamInfo mStreamInfoArray[kMediaTypeCount];
    std::map<int, StreamInfo*> mStreamIdx2Info;
    int mReportedBufferingPercent;
    MediaMetaSP mMetaData;

    int64_t mSeekUs;
    bool mSeekDone;

    bool mDashSelectVideo;
    bool mDashSelectAudio;

    std::string mMime;
    MediaType mSelectedMediaType;

    bool mCheckVideoKeyFrame;//true means extractor disards all the audio/video buffer before we get first video key frame

    int64_t mPlaybackStartUs;
#ifdef DUMP_INPUT
    FILE * mInputFile;
#endif
    std::queue<SeekSequence> mSeekSequence;

    Lock mAVLock; // send to downlink components for mutex operation between audio and video stream processing

    DECLARE_MSG_LOOP()
    DECLARE_MSG_HANDLER(onPrepare)
    DECLARE_MSG_HANDLER(onReadSample)

    MM_DISALLOW_COPY(SegExtractor)
    DECLARE_LOGTAG()
};

}

#endif // __seg_extractor_H
