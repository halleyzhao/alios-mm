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

#ifndef __instantaudioimp_H
#define __instantaudioimp_H

#include <multimedia/mmthread.h>
#include <multimedia/instantaudio.h>
#include "loader.h"
#include "channel.h"

namespace YUNOS_MM {

class InstantAudioImp : public InstantAudio {
public:
    InstantAudioImp(size_t maxChannels);
    ~InstantAudioImp();

public:
    bool init();
    void release();
    virtual mm_status_t load(const char* uri);
    virtual mm_status_t load(int fd, int64_t offset, int64_t length);
    virtual mm_status_t load(const MMIDBufferSP & buffer);
    virtual mm_status_t unload(int sampleID);
    virtual mm_status_t unloadAll();
    virtual int play(int sampleID, const VolumeInfo * volume, int loop, float rate, int priority, int streamType);
    virtual int play(int sampleID, const VolumeInfo * volume, int loop, float rate, int priority, const char * connectionId);
    virtual mm_status_t pause(int channelID);
    virtual mm_status_t pauseAll();
    virtual mm_status_t resume(int channelID);
    virtual mm_status_t resumeAll();
    virtual mm_status_t stop(int channelID);
    virtual mm_status_t stopAll();
    virtual mm_status_t setVolume(int channelID, const VolumeInfo * volume);
    virtual mm_status_t setLoop(int channelID, int loop);
    virtual mm_status_t setRate(int channelID, float rate);
    virtual mm_status_t setPriority(int channelID, int priority);

private:
    InstantAudioImp(){}

protected:
    static void uri_loaded_cb(const char * uri, int status, Sample * sample, void * userData);
    static void fd_loaded_cb(int fd, int status, Sample * sample, void * userData);
    static void buffer_loaded_cb(MMIDBuffer::id_type bufferId, int status, Sample * sample, void * userData);
    static void play_competed_cb(int channelID, int status, void * userData);

    bool allocChannelPool();
    void freeChannelPool();
    void playCompleted(int channelID);
    void loaded(const char * uri, int status, Sample * sample);
    void loaded(int fd, int status, Sample * sample);
    void loadedBuffer(MMIDBuffer::id_type bufferId, int status, Sample * sample);
    bool loaded(int status, Sample * sample);

    Channel * allocChannel(int priority);
    void deAllocChannel(Channel * channel);
    void insertRecycleChannel(int channelID);
    void removeRecycleChannel(int channelID);
    void recycleChannel();
    void recycleChannelExternaly();
    void insertPlaying(Channel * channel);
    Channel * removePlaying(int channelID);
    Channel * findChannelInPlaying(int channelID);
    mm_status_t doPause(Channel * channel);
    mm_status_t doResume(Channel * channel);
    mm_status_t doStop(Channel * channel);
    mm_status_t doStopAll();
    mm_status_t doUnloadAll();

    class Recycler : public MMThread {
    public:
        Recycler(InstantAudioImp * watcher);
        ~Recycler();

    public:
        bool start();
        void stop();
        void recycle();

    protected:
        virtual void main();

    private:
        InstantAudioImp * mWatcher;
        bool mContinue;
        sem_t mSem;
        MM_DISALLOW_COPY(Recycler)
        DECLARE_LOGTAG()
    };

    size_t mMaxChannels;
    Lock mLock;
    Lock mSubLock;
    Loader * mLoader;
    std::map<int, Sample *> mSamples;
    typedef std::vector<Channel *> channel_pool_t;
    channel_pool_t mChannelPool;
    typedef std::list<Channel*> channel_list_t;
    channel_list_t mIdleChannels;
    channel_list_t mPlayingChannels;
    typedef std::list<int> channelid_list_t;
    channelid_list_t mRecyclingChannels;

    Recycler * mRecycler;

    MM_DISALLOW_COPY(InstantAudioImp)
    DECLARE_LOGTAG()
};

}

#endif

