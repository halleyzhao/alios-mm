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

#include "instantaudioimp.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(InstantAudioImp)



#define CHANNEL_OP_CHECK(_channel) do {\
    if ( !(_channel) ) {\
        MMLOGE("invalid params\n");\
        return MM_ERROR_INVALID_PARAM;\
    }\
    MMLOGD("id: %d\n", (_channel)->id());\
}while(0)


InstantAudioImp::InstantAudioImp(size_t maxChannels)
                    : mMaxChannels(maxChannels),
                    mLoader(NULL),
                    mRecycler(NULL)
{
    MMLOGI("+\n");
}

InstantAudioImp::~InstantAudioImp()
{
    MMLOGI("+\n");
    MMASSERT(mLoader == NULL);
    MMASSERT(mRecycler == NULL);
    MMASSERT(mPlayingChannels.empty());
    MMASSERT(mSamples.empty());

    MMLOGI("-\n");
}

bool InstantAudioImp::init()
{
    MMLOGI("+\n");
    mRecycler = new Recycler(this);
    if (!mRecycler) {
        MMLOGE("no mem\n");
        return false;
    }
    if (!mRecycler->start()) {
        MMLOGE("failed to start recycler\n");
        MM_RELEASE(mRecycler);
        return false;
    }
    mLoader = new Loader(uri_loaded_cb, fd_loaded_cb, buffer_loaded_cb, this);
    if ( !mLoader ) {
        MMLOGE("no mem\n");
        mRecycler->stop();
        MM_RELEASE(mRecycler);
        return false;
    }

    if ( !allocChannelPool() ) {
        MMLOGE("no mem\n");
        mLoader->exit();
        MM_RELEASE(mLoader);
        mRecycler->stop();
        MM_RELEASE(mRecycler);
        return false;
    }

    for ( size_t i = 0; i < mMaxChannels; ++i )
        mIdleChannels.push_back(mChannelPool[i]);

    if ( mLoader->run() ) {
        MMLOGE("failed to create loader\n");
        freeChannelPool();
        mLoader->exit();
        MM_RELEASE(mLoader);
        mRecycler->stop();
        MM_RELEASE(mRecycler);
        return false;
    }

    MMLOGV("-\n");
    return true;
}

void InstantAudioImp::release()
{
    MMLOGI("+\n");
    if (mLoader)
        mLoader->exit();
    MM_RELEASE(mLoader);

    if (mRecycler)
        mRecycler->stop();
    MM_RELEASE(mRecycler);

    doStopAll();
    MMASSERT(mPlayingChannels.empty());

    mIdleChannels.clear();

    MMLOGV("recycle size: %d\n", mRecyclingChannels.size());
    mRecyclingChannels.clear();

    freeChannelPool();

    doUnloadAll();
    MMASSERT(mSamples.empty());

    MMLOGI("-\n");
}


bool InstantAudioImp::allocChannelPool()
{
    for ( size_t i = 0; i < mMaxChannels; ++i ) {
        Channel * c = new Channel(i + 1, play_competed_cb, this);
        if ( !c ) {
            MMLOGE("no mem\n");
            freeChannelPool();
            return false;
        }

        mChannelPool.push_back(c);
    }

    return true;
}

void InstantAudioImp::freeChannelPool()
{
    MMLOGV("size: %d\n", mChannelPool.size());
    channel_pool_t::iterator i;
    for ( i = mChannelPool.begin(); i != mChannelPool.end(); ++i ) {
        Channel * c = *i;
        MM_RELEASE(c);
    }
    mChannelPool.clear();
}

mm_status_t InstantAudioImp::load(const char* uri)
{
    MMLOGI("uri: %s\n", uri);
    return mLoader->load(uri);
}

mm_status_t InstantAudioImp::load(int fd, int64_t offset, int64_t length)
{
    MMLOGI("fd: %d, offset: %" PRId64 ", length: %" PRId64 "\n", fd, offset, length);
    return mLoader->load(fd, offset, length);
}

mm_status_t InstantAudioImp::load(const MMIDBufferSP & buffer)
{
    if ( !buffer || buffer->getActualSize() == 0 ) {
        MMLOGE("invalid params\n");
        return MM_ERROR_INVALID_PARAM;
    }
    MMLOGI("bufer id: %u, size: %u\n", buffer->id(), buffer->getActualSize());
    return mLoader->load(buffer);
}

/*static*/ void InstantAudioImp::uri_loaded_cb(const char * uri, int status, Sample * sample, void * userData)
{
    MMLOGI("uri: %s, status: %d, sample: %p, userdata: %p\n", uri, status, sample, userData);
    InstantAudioImp * me = static_cast<InstantAudioImp*>(userData);
    if ( !me ) {
        MMLOGE("invalid params\n");
        return;
    }

    me->loaded(uri, status, sample);
}

/*static*/ void InstantAudioImp::fd_loaded_cb(int fd, int status, Sample * sample, void * userData)
{
    MMLOGI("fd: %d, status: %d, sample: %p, userdata: %p\n", fd, status, sample, userData);
    InstantAudioImp * me = static_cast<InstantAudioImp*>(userData);
    if ( !me ) {
        MMLOGE("invalid params\n");
        return;
    }

    me->loaded(fd, status, sample);
}

/*static */void InstantAudioImp::buffer_loaded_cb(MMIDBuffer::id_type bufferId, int status, Sample * sample, void * userData)
{
    MMLOGI("bufferId: %d, status: %d, sample: %p, userdata: %p\n", bufferId, status, sample, userData);
    InstantAudioImp * me = static_cast<InstantAudioImp*>(userData);
    if ( !me ) {
        MMLOGE("invalid params\n");
        return;
    }

    me->loadedBuffer(bufferId, status, sample);
}

void InstantAudioImp::loaded(const char * uri, int status, Sample * sample)
{
    MMLOGI("uri: %s, status: %d, sample: %p\n", uri, status, sample);
    loaded(status, sample);

    if ( !mListener ) {
        MMLOGW("listener not set\n");
        return;
    }

    MMParam param;
    param.writeCString(uri);

    int sampleID = status == MM_ERROR_SUCCESS ? sample->id() : -1;

    MMLOGV("calling Listener\n");
    mListener->onMessage(InstantAudio::Listener::URI_SAMPLE_LOADED, status, sampleID, &param);
}

void InstantAudioImp::loaded(int fd, int status, Sample * sample)
{
    MMLOGI("fd: %d, status: %d, sample: %p\n", fd, status, sample);
    loaded(status, sample);

    if ( !mListener ) {
        MMLOGW("listener not set\n");
        return;
    }

    int sampleID = status == MM_ERROR_SUCCESS ? sample->id() : -1;
    MMParam param;
    param.writeInt32(sampleID);

    MMLOGV("calling Listener\n");
    mListener->onMessage(InstantAudio::Listener::FD_SAMPLE_LOADED, status, fd, &param);
}

void InstantAudioImp::loadedBuffer(MMIDBuffer::id_type bufferId, int status, Sample * sample)
{
    MMLOGI("bufferId: %u, status: %d, sample: %p\n", bufferId, status, sample);
    loaded(status, sample);

    if ( !mListener ) {
        MMLOGW("listener not set\n");
        return;
    }

    MMParam param;
    param.writeInt32(bufferId);

    int sampleID = status == MM_ERROR_SUCCESS ? sample->id() : -1;

    MMLOGV("calling Listener, sampleID: %d\n", sampleID);
    mListener->onMessage(InstantAudio::Listener::BUFFER_SAMPLE_LOADED, status, sampleID, &param);
}

bool InstantAudioImp::loaded(int status, Sample * sample)
{
    MMLOGV("status: %d, sample: %p\n", status, sample);
    if ( status != MM_ERROR_SUCCESS ) {
        MMASSERT(sample == NULL);
        return true;
    }

    MMASSERT(mSamples.count(sample->id()) == 0);
    MMAutoLock lock(mSubLock);
    mSamples.insert(std::pair<int,Sample *>(sample->id(), sample));
    return true;
}

mm_status_t InstantAudioImp::unload(int sampleId)
{
    MMLOGI("sampleId: %d\n", sampleId);
    MMAutoLock lock(mLock);
    {
        channel_list_t::iterator i;
        for ( i = mPlayingChannels.begin(); i != mPlayingChannels.end(); ++i ) {
            if ( (*i)->sampleID() == sampleId ) {
                Channel * c = *i;
                doStop(c);
            }
        }
    }
    std::map<int, Sample *>::iterator i;
    i = mSamples.find(sampleId);
    if ( i == mSamples.end() ) {
        MMLOGI("not found\n");
        return MM_ERROR_SUCCESS;
    }

    Sample * sam = i->second;
    MM_RELEASE(sam);
    mSamples.erase(i);
    return MM_ERROR_SUCCESS;
}

mm_status_t InstantAudioImp::doUnloadAll()
{
    MMLOGV("+sample size: %d\n", mSamples.size());
    MMASSERT(mPlayingChannels.empty());
    if ( !mPlayingChannels.empty() ) {
        MMLOGE("playing\n");
        return MM_ERROR_IVALID_OPERATION;
    }

    while ( !mSamples.empty() ) {
        std::map<int, Sample *>::iterator i = mSamples.begin();
        MMLOGI("releasing: %d\n", i->first);
        Sample * s = i->second;
        MM_RELEASE(s);
        mSamples.erase(i);
    }

    MMLOGI("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t InstantAudioImp::unloadAll()
{
    MMLOGI("+\n");
    MMAutoLock lock(mLock);
    doStopAll();

    return doUnloadAll();
}

Channel * InstantAudioImp::allocChannel(int priority)
{
    recycleChannel();
    MMLOGV("priority: %d, idle size: %d\n", priority, mIdleChannels.size());
    if ( mIdleChannels.empty() ) {
        return NULL;
    }

    do {
        if ( mPlayingChannels.size() >= mMaxChannels ) {
            MMLOGE("no more channels, now count: %d\n", mMaxChannels);
            return NULL;
        }

        if ( mPlayingChannels.empty() ) {
            MMLOGV("no playing\n");
            break;
        }

        Channel * c = *(mPlayingChannels.begin());
        if ( priority < c->priority() ) {
            MMLOGI("req prority: %d, cur: %d\n", priority, c->priority());
            return NULL;
        }
    } while(0);

    Channel * c = mIdleChannels.front();
    MMASSERT(c != NULL);
    mIdleChannels.pop_front();
    c->setPriority(priority);
    return c;
}

void InstantAudioImp::deAllocChannel(Channel * channel)
{
    MMLOGV("+channel: %p, size: %d\n", channel, mIdleChannels.size());
    mIdleChannels.push_back(channel);
    MMLOGV("-channel: %p, size: %d\n", channel, mIdleChannels.size());
}

void InstantAudioImp::insertPlaying(Channel * channel)
{
    MMLOGV("channel: %p\n", channel);
    MMASSERT(mPlayingChannels.empty()
        || (mPlayingChannels.front()->priority() <= channel->priority()));
    mPlayingChannels. push_front(channel);
}

void InstantAudioImp::recycleChannelExternaly()
{
    MMLOGI("+\n");
    MMAutoLock lock(mLock);
    recycleChannel();
    MMLOGI("-\n");
}

void InstantAudioImp::recycleChannel()
{
    MMLOGI("+\n");
    MMAutoLock lock(mSubLock);
    while ( !mRecyclingChannels.empty() ) {
        int channelID = mRecyclingChannels.front();
        Channel * c = removePlaying(channelID);
        if ( c ) {
            deAllocChannel(c);
            lock.unlock();
            MMLOGV("recycling channel %d\n", channelID);
            c->stop();
            MMLOGI("channel %d recycled\n", channelID);
            lock.lock();
        } else {
            MMLOGI("not found\n");
        }
        mRecyclingChannels.pop_front();
    }
    MMLOGI("-\n");
}

int InstantAudioImp::play(int sampleID, const VolumeInfo * volume, int loop, float rate, int priority, int streamType)
{
    MMLOGI("sampleID: %d, volume: %p, loop: %d, rate: %f, priority: %d\n", sampleID, volume, loop, rate, priority);
    MMAutoLock lock(mLock);
    std::map<int, Sample *>::iterator it = mSamples.find(sampleID);
    if ( it == mSamples.end() ) {
        MMLOGE("no such sample\n");
        return -1;
    }

    Sample * sample = it->second;

    Channel * c = allocChannel(priority);
    if ( !c ) {
        MMLOGI("no resouce\n");
        return -1;
    }

    mm_status_t ret = c->play(sample, volume, loop, rate, streamType);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to play\n");
        deAllocChannel(c);
        return -1;
    }

    insertPlaying(c);
    MMLOGI("play success, id: %d\n", c->id());
    return c->id();
}

int InstantAudioImp::play(int sampleID, const VolumeInfo * volume, int loop, float rate, int priority, const char * connectionId)
{
    MMLOGI("sampleID: %d, volume: %p, loop: %d, rate: %f, priority: %d, connectionId: %s\n", sampleID, volume, loop, rate, priority, connectionId);
    MMAutoLock lock(mLock);
    std::map<int, Sample *>::iterator it = mSamples.find(sampleID);
    if ( it == mSamples.end() ) {
        MMLOGE("no such sample\n");
        return -1;
    }

    Sample * sample = it->second;

    Channel * c = allocChannel(priority);
    if ( !c ) {
        MMLOGI("no resouce\n");
        return -1;
    }

    mm_status_t ret = c->play(sample, volume, loop, rate, connectionId);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to play\n");
        deAllocChannel(c);
        return -1;
    }

    insertPlaying(c);
    MMLOGI("play success, id: %d\n", c->id());
    return c->id();
}


/*static */void InstantAudioImp::play_competed_cb(int channelID, int status, void * userData)
{
    MMLOGI("channelID: %d, status: %d\n", channelID, status);
    InstantAudioImp * me = static_cast<InstantAudioImp*>(userData);
    MMASSERT(me != NULL);
    me->playCompleted(channelID);
}

void InstantAudioImp::insertRecycleChannel(int channelID)
{
    MMLOGI("channelID: %d to recycle\n", channelID);
    MMAutoLock lock(mSubLock);
    mRecyclingChannels.push_back(channelID);
}

void InstantAudioImp::removeRecycleChannel(int channelID)
{
    MMAutoLock lock(mSubLock);
    channelid_list_t::iterator i;
    for ( i = mRecyclingChannels.begin(); i != mRecyclingChannels.end(); ++i ) {
        int id = *i;
        if ( id == channelID ) {
            MMLOGI("channelID: %d removed from recycle\n", channelID);
            mRecyclingChannels.erase(i);
            return;
        }
    }
    MMLOGI("channelID: %d expect removed from recycle: but not found\n", channelID);
}

void InstantAudioImp::playCompleted(int channelID)
{
    insertRecycleChannel(channelID);
    mRecycler->recycle();
}

Channel * InstantAudioImp::findChannelInPlaying(int channelID)
{
    MMLOGV("channelID: %d\n", channelID);
    channel_list_t::iterator i;
    for ( i = mPlayingChannels.begin(); i != mPlayingChannels.end(); ++i ) {
        if ( (*i)->id() == channelID ) return *i;
    }

    return NULL;
}

Channel * InstantAudioImp::removePlaying(int channelID)
{
    MMLOGV("channelID: %d, playing size: %d\n", channelID, mPlayingChannels.size());
    channel_list_t::iterator i;
    for ( i = mPlayingChannels.begin(); i != mPlayingChannels.end(); ++i ) {
        if ( (*i)->id() == channelID ) {
            Channel * c = *i;
            mPlayingChannels.erase(i);
            return c;
        }
    }

    return NULL;
}

mm_status_t InstantAudioImp::pause(int channelID)
{
    MMLOGI("channelID: %d\n", channelID);
    MMAutoLock lock(mLock);

    return doPause(findChannelInPlaying(channelID));
}

mm_status_t InstantAudioImp::doPause(Channel * channel)
{
    CHANNEL_OP_CHECK(channel);

    return channel->pause();
}

mm_status_t InstantAudioImp::doResume(Channel * channel)
{
    CHANNEL_OP_CHECK(channel);

    return channel->resume();
}

mm_status_t InstantAudioImp::doStop(Channel * channel)
{
    MMLOGV("cid: %d\n", channel);
    CHANNEL_OP_CHECK(channel);

    channel->stop();
    deAllocChannel(channel);
    removeRecycleChannel(channel->id());
    return MM_ERROR_SUCCESS;
}

mm_status_t InstantAudioImp::doStopAll()
{
    MMLOGV("+playing size: %d\n", mPlayingChannels.size());
    channel_list_t::iterator i;
    while ( !mPlayingChannels.empty() ) {
        Channel * c = mPlayingChannels.front();
        doStop(c);
        mPlayingChannels.pop_front();
    }

    MMLOGV("-");
    return MM_ERROR_SUCCESS;
}

mm_status_t InstantAudioImp::pauseAll()
{
    MMLOGI("+\n");
    MMAutoLock lock(mLock);
    channel_list_t::iterator it;
    for ( it = mPlayingChannels.begin(); it != mPlayingChannels.end(); ++it ) {
        doPause(*it);
    }
    MMLOGI("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t InstantAudioImp::resume(int channelID)
{
    MMLOGI("channelID: %d\n", channelID);
    MMAutoLock lock(mLock);

    return doResume(findChannelInPlaying(channelID));
}

mm_status_t InstantAudioImp::resumeAll()
{
    MMLOGI("+\n");
    MMAutoLock lock(mLock);
    channel_list_t::iterator it;
    for ( it = mPlayingChannels.begin(); it != mPlayingChannels.end(); ++it ) {
        doResume(*it);
    }
    MMLOGI("-\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t InstantAudioImp::stop(int channelID)
{
    MMLOGI("channelID: %d\n", channelID);
    MMAutoLock lock(mLock);

    Channel * c = removePlaying(channelID);
    if ( !c ) {
        MMLOGE("not found\n");
        return MM_ERROR_INVALID_PARAM;
    }

    return doStop(c);
}

mm_status_t InstantAudioImp::stopAll()
{
    MMLOGI("+");
    MMAutoLock lock(mLock);
    return doStopAll();
}

mm_status_t InstantAudioImp::setVolume(int channelID, const VolumeInfo * volume)
{
    MMLOGI("channelID: %d, l: %f, r: %f\n", channelID, volume->left, volume->right);
    MMAutoLock lock(mLock);

    Channel * channel = findChannelInPlaying(channelID);
    if ( !channel ) {
        MMLOGE("no such channel\n");
        return MM_ERROR_INVALID_PARAM;
    }
    return channel->setVolume(volume);
}

mm_status_t InstantAudioImp::setLoop(int channelID, int loop)
{
    MMLOGI("channelID: %d, loop: %d\n", channelID, loop);
    MMAutoLock lock(mLock);

    Channel * channel = findChannelInPlaying(channelID);
    if ( !channel ) {
        MMLOGE("no such channel\n");
        return MM_ERROR_INVALID_PARAM;
    }
    return channel->setLoop(loop);
}

mm_status_t InstantAudioImp::setRate(int channelID, float rate)
{
    MMLOGI("channelID: %d, rate: %f\n", channelID, rate);
    MMAutoLock lock(mLock);

    Channel * channel = findChannelInPlaying(channelID);
    if ( !channel ) {
        MMLOGE("no such channel\n");
        return MM_ERROR_INVALID_PARAM;
    }
    return channel->setRate(rate);
}

mm_status_t InstantAudioImp::setPriority(int channelID, int priority)
{
    MMLOGI("channelID: %d, rate: %d\n", channelID, priority);
    MMAutoLock lock(mLock);

    Channel * channel = removePlaying(channelID);
    if ( !channel ) {
        MMLOGE("no such channel\n");
        return MM_ERROR_INVALID_PARAM;
    }
    channel->setPriority(priority);

    channel_list_t::iterator i;
    for ( i = mPlayingChannels.begin(); i != mPlayingChannels.end(); ++i ) {
        if ( (*i)->priority() <= channel->priority() ) {
            MMLOGV("found pos\n");
            mPlayingChannels.insert(i, channel);
        }
    }

    if ( i == mPlayingChannels.end() ) {
        MMLOGV("push to the end\n");
        mPlayingChannels.push_back(channel);
    }

    return MM_ERROR_SUCCESS;
}

DEFINE_LOGTAG(InstantAudioImp::Recycler)

InstantAudioImp::Recycler::Recycler(InstantAudioImp * watcher)
                            : MMThread("instantaudio-recycle"),
                            mWatcher(watcher),
                            mContinue(false)
{
    MMLOGI("+\n");
    sem_init(&mSem, 0, 0);
    MMLOGI("-\n");
}

InstantAudioImp::Recycler::~Recycler()
{
    MMLOGI("+\n");
    sem_destroy(&mSem);
    MMLOGI("-\n");
}

bool InstantAudioImp::Recycler::start()
{
    MMLOGI("+\n");
    mContinue = true;
    if (MMThread::create()) {
        MMLOGE("failed to create thread\n");
        mContinue = false;
        return false;
    }
    MMLOGI("-\n");
    return true;
}

void InstantAudioImp::Recycler::stop()
{
    MMLOGI("+\n");
    mContinue = false;
    sem_post(&mSem);
    destroy();
    MMLOGI("-\n");
}

void InstantAudioImp::Recycler::recycle()
{
    MMLOGI("+\n");
    sem_post(&mSem);
}

void InstantAudioImp::Recycler::main()
{
    MMLOGI("+\n");
    while (mContinue) {
        mWatcher->recycleChannelExternaly();
        sem_wait(&mSem);
    }
    MMLOGI("-\n");
}

}

