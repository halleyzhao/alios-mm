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

#ifndef __mediametaretriever_imp_H
#define __mediametaretriever_imp_H

#include <semaphore.h>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_buffer.h>
#include <multimedia/component.h>
#include <multimedia/mediametaretriever.h>

namespace YUNOS_MM {


class MediaMetaRetrieverImp : public MediaMetaRetriever {
public:
    MediaMetaRetrieverImp();
    virtual ~MediaMetaRetrieverImp();

public:
    virtual bool initCheck();
    virtual mm_status_t setDataSource(
            const char * dataSourceUrl,
            const std::map<std::string, std::string> * headers = NULL);
    virtual mm_status_t setDataSource(int fd, int64_t offset, int64_t length);
    virtual mm_status_t extractMetadata(MediaMetaSP & fileMeta, MediaMetaSP & audioMeta, MediaMetaSP & videoMeta);
    virtual VideoFrameSP extractVideoFrame(int64_t timeUs, int32_t * memCosts = NULL);
    virtual VideoFrameSP extractVideoFrame(int64_t timeUs,
                RGB::Format format,
                uint32_t scaleNum = 1,
                uint32_t scaleDenom = 1);
    virtual CoverSP extractCover();

protected:
    class RSink : public SinkComponent {
    public:
        RSink(MediaType mediaType);
        virtual ~RSink();

        enum EventInfo {
            kEventDataReceived = kEventInfoSubClassStart + 1
        };

    public:
        virtual const char * name() const { return "RSink"; }
        virtual const char * version() const { return "0"; }
        virtual WriterSP getWriter(MediaType mediaType);
        virtual mm_status_t addSource(Component * component, MediaType mediaType);
        virtual mm_status_t getParameter(MediaMetaSP & meta) const;
        virtual mm_status_t reset();
        virtual int64_t getCurrentPosition() {return -1ll;}

    public:
        VideoFrameSP getFrame() const;
        void cleanFrame();

    protected:
        void setMetaData(const MediaMetaSP & meta);
        void bufferReiceived(const MediaBufferSP & buffer);

    private:
        class RWriter : public Writer {
        public:
            RWriter(RSink * watcher);
            ~RWriter();

        public:
            virtual mm_status_t write(const MediaBufferSP & buffer);
            virtual mm_status_t setMetaData(const MediaMetaSP & metaData);

        private:
            RSink * mWatcher;

            MM_DISALLOW_COPY(RWriter)
            DECLARE_LOGTAG()
        };

    private:
        MediaType mMediaType;
        MediaMetaSP mMeta;
        VideoFrameSP mVideoFrame;
        ReaderSP mReader;

        MM_DISALLOW_COPY(RSink)
        DECLARE_LOGTAG()
    };
    typedef std::tr1::shared_ptr<RSink> RSinkSP;


private:
    enum State {
        kStateNone,
        kStateError,
        kStateInited
    };

    struct ComponentListener : public Component::Listener {
    public:
        ComponentListener(MediaMetaRetrieverImp * wathcer);

        virtual void onMessage(int msg, int param1, int param2, const MMParamSP obj, const Component * sender);

    private:
        MediaMetaRetrieverImp * mWatcher;

        MM_DISALLOW_COPY(ComponentListener)
        DECLARE_LOGTAG()
    };
    typedef std::tr1::shared_ptr<ComponentListener> ComponentListenerSP;

    struct ComponentInfo {
        ComponentInfo() : state(kStateNotCreated), mMemCosts(0) {}
        ~ComponentInfo() {}

        enum State {
            kStateError,
            kStateExecuting,
            kStateNotCreated,
            kStateCreated,
            kStatePrepared,
            kStateSeekComplete,
            kStateStarted,
            kStatePaused,
            kStateFlushed,
            kStateStopped,
            kStateDataReceived,
            kStateEOS,
        };

        State state;
        ComponentSP component;
        ComponentListenerSP listener;
        int mMemCosts;
    };

    enum ComponentIndex {
        kIdSouce,
        kIdVideoDec,
        kIdAudioSink,
        kIdVideoSink,
        kComponentCount
    };

private:
    bool createListeners();
    bool createByFactory(int componentIndex, const char * mime);
    bool initSource();
    bool initSinks();
    bool initSink(int index, Component::MediaType type);
    mm_status_t extractMeta();
    mm_status_t prepareSource(std::string & audioMime, std::string & videoMime);
    void reset();
    void onComponentMessage(int msg,
            int param1,
            int param2,
            const MMParamSP obj,
            const Component * sender);
    void resetComponents();
    bool waitComponentsState(ComponentInfo::State state);
    mm_status_t prepareVideoExtract(int64_t timeUs);
    mm_status_t createVideoDec();
    mm_status_t prepareVideoDec();
    mm_status_t seekSource(int64_t timeUs);
    mm_status_t connectComponentsForVideoExtract();
    mm_status_t extractVideoFrame();

    bool hasComponentError() const;

    bool convertNV12(VideoFrameSP & videoFrame);

private:
    mutable Lock mLock;
    Condition mCondition;
    State mState;
    ComponentInfo mComponents[kComponentCount];

    std::string mAudioMime;
    std::string mVideoMime;

    MM_DISALLOW_COPY(MediaMetaRetrieverImp)
    DECLARE_LOGTAG()
};

}

#endif // __mediametaretriever_imp_H

