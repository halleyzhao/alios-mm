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

#include <errno.h>
#include <string.h>
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <multimedia/component_factory.h>
#include <multimedia/media_attr_str.h>

#include "mediametaretriever_imp.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

#define SET_STATE(_state) do {\
    MMLOGI("state change from %d to %s\n", mState, #_state);\
    mState = _state;\
}while(0)

#define SET_COMPONENT_STATE(_id, _s, _state) do {\
    MMLOGV("component %d state change from %d to %d\n", _id, _s, _state);\
    _s = _state;\
}while(0)

#define FUNC_ENTER() do { MMLOGV("+\n"); }while(0)
#define FUNC_LEAVE() do { MMLOGV("-\n"); }while(0)

#define CREATE_LISTENER(_m) do {\
    _m = ComponentListenerSP(new ComponentListener(this));\
    if ( !_m ) {\
        MMLOGE("failed to create ComponentListener\n");\
        SET_STATE(kStateError);\
        return;\
    }\
}while(0)

#define SET_LISTENER(_c, _ls) do {\
    if ( (_c)->setListener(_ls) != MM_ERROR_SUCCESS ) {\
        MMLOGE("failed to set listner\n");\
        (_c).reset();\
        return false;\
    }\
}while(0)


#define EXEC_COMPONENT(_c_index, _expect_state, _func) do {\
    ComponentSP & cp = mComponents[_c_index].component;\
    if ( !cp ) {\
        MMLOGV("component(%d) not exists, ignore\n");\
        break;\
    }\
    SET_COMPONENT_STATE(_c_index, mComponents[_c_index].state, ComponentInfo::kStateExecuting);\
    MMLOGV("calling %d -> %s\n", _c_index, #_func);\
    mm_status_t ret = cp->_func;\
    MMLOGV("calling %d -> %s, over\n", _c_index, #_func);\
    switch ( ret ) {\
        case MM_ERROR_SUCCESS:\
            SET_COMPONENT_STATE(_c_index, mComponents[_c_index].state, _expect_state);\
            break;\
        case MM_ERROR_ASYNC:\
            break;\
        default:\
            MMLOGE("calling %d -> %s failed, ret: %d\n", _c_index, #_func, ret);\
            SET_COMPONENT_STATE(_c_index, mComponents[_c_index].state, ComponentInfo::kStateError);\
            break;\
    }\
}while(0)

#define EXEC_ALL(_expect_state, _func) do {\
    for ( int i = 0; i < kComponentCount; ++i ) {\
        EXEC_COMPONENT(i, _expect_state, _func);\
    }\
}while(0)

#define WAIT_SINGLE_STATE(_ci) do {\
        if ( (_ci)->state != ComponentInfo::kStateExecuting ) {\
            break;\
        }\
        MMLOGV("waitting\n");\
        mCondition.timedWait(1000000);\
    } while ( 1 )


#define COMPONENT_STATE_MSGITEM(_ciid, _ci, _msg, _success_state)\
    case _msg:\
            {ComponentInfo::State newState = param1 == MM_ERROR_SUCCESS ?\
                        _success_state : ComponentInfo::kStateError;\
            SET_COMPONENT_STATE(_ciid, _ci->state, newState);\
            mCondition.signal();}\
            return

#define COMPONENT_STATE_MSGITEM_NOFAIL(_ciid, _ci, _msg, _state)\
    case _msg:\
            MMLOGV("msg: %s\n", #_msg);\
            SET_COMPONENT_STATE(_ciid, _ci->state, _state);\
            mCondition.signal();\
            return

#define COMPONENT_STATE_MSGITEM_INFO(_ciid, _ci, _info, _state)\
    case Component::kEventInfo:\
            if ( param1 == RSink::kEventDataReceived ) {\
                MMLOGV("msg: %s\n", #_info);\
                SET_COMPONENT_STATE(_ciid, _ci->state, _state);\
                MMLOGV("signal\n");\
                mCondition.signal();\
                break;\
            }\
            if (param1 == Component::kEventCostMemorySize) {\
                MMLOGV("kEventCostMemorySize: %d, type: %d\n", param2, _ciid);\
                mComponents[_ciid].mMemCosts = (int)param2;\
                return;\
            }\
            MMLOGV("ignore info: %d\n", param1);\
            break;


#define VIDEO_EXTRACT_PREPARE_EXEC(_func) do {\
    mm_status_t ret = _func;\
    if ( ret != MM_ERROR_SUCCESS ) {\
        mComponents[kIdVideoDec].component.reset();\
        MMLOGE("%s failed\n", #_func);\
        return ret;\
    }\
}while(0)


DEFINE_LOGTAG(MediaMetaRetrieverImp::RSink)
DEFINE_LOGTAG(MediaMetaRetrieverImp::ComponentListener)
DEFINE_LOGTAG(MediaMetaRetrieverImp)
DEFINE_LOGTAG(MediaMetaRetrieverImp::RSink::RWriter)


MediaMetaRetrieverImp::RSink::RWriter::RWriter(MediaMetaRetrieverImp::RSink * watcher) : mWatcher(watcher)
{
}

MediaMetaRetrieverImp::RSink::RWriter::~RWriter()
{
    FUNC_ENTER();
    FUNC_LEAVE();
}

mm_status_t MediaMetaRetrieverImp::RSink::RWriter::write(const MediaBufferSP & buffer)
{
    FUNC_ENTER();
    mWatcher->bufferReiceived(buffer);
    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::RSink::RWriter::setMetaData(const MediaMetaSP & metaData)
{
    FUNC_ENTER();
    mWatcher->setMetaData(metaData);
    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

MediaMetaRetrieverImp::RSink::RSink(MediaType mediaType) : mMediaType(mediaType)
{
    FUNC_ENTER();
    FUNC_LEAVE();
}

MediaMetaRetrieverImp::RSink::~RSink()
{
    FUNC_ENTER();
    FUNC_LEAVE();
}

Component::WriterSP MediaMetaRetrieverImp::RSink::getWriter(MediaType mediaType)
{
    FUNC_ENTER();
    return WriterSP(new RWriter(this));
}

mm_status_t MediaMetaRetrieverImp::RSink::addSource(Component * component, MediaType mediaType)
{
    FUNC_ENTER();
    MMLOGV("mediaType: %d\n", mediaType);
    if ( !component ) {
        MMLOGE("invalid params\n");
        return MM_ERROR_INVALID_PARAM;
    }

    mMediaType = mediaType;
    mReader = component->getReader(mediaType);
    if ( !mReader ) {
        MMLOGE("failed to get reader\n");
        return MM_ERROR_INVALID_PARAM;
    }

    mMeta = mReader->getMetaData();
//    mMeta->dump();

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::RSink::getParameter(MediaMetaSP & meta) const
{
    FUNC_ENTER();
    meta = mMeta;
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::RSink::reset()
{
    FUNC_ENTER();
    mMeta.reset();
    mVideoFrame.reset();
    mReader.reset();
    return MM_ERROR_SUCCESS;
}

VideoFrameSP MediaMetaRetrieverImp::RSink::getFrame() const
{
    FUNC_ENTER();
    return mVideoFrame;
}

void MediaMetaRetrieverImp::RSink::cleanFrame()
{
    FUNC_ENTER();
    mVideoFrame.reset();
}

void MediaMetaRetrieverImp::RSink::setMetaData(const MediaMetaSP & meta)
{
    FUNC_ENTER();
    mMeta = meta;
}

#define bufferReiceived_check(_exp) do {\
    if ( !(_exp) ) {\
        MMLOGE("%s check failed\n", #_exp);\
        notify(kEventError, 0, 0, MMParamSP((MMParam*)NULL));\
        return;\
    }\
}while(0)

/*
static void hexDumpLocal(const uint8_t* data, uint32_t size, uint32_t bytesPerLine)
{
    assert(data && size && bytesPerLine);

    char oneLineData[bytesPerLine*4+1];
    uint32_t offset = 0, lineStart = 0, i= 0;
    while (offset < size) {
        sprintf(&oneLineData[i*4], "%02x, ", *(data+offset));
        offset++;
        i++;
        if (offset == size || (i % bytesPerLine == 0)) {
            oneLineData[4*i-1] = '\0';
            printf("%04x: %s\n", lineStart, oneLineData);
            lineStart += bytesPerLine;
            i = 0;
        }
    }
}
*/
void MediaMetaRetrieverImp::RSink::bufferReiceived(const MediaBufferSP & buffer)
{
    FUNC_ENTER();
    if ( mMediaType != kMediaTypeVideo || mVideoFrame != NULL ) {
        MMLOGV("me: not video, or already received, ignore(mMediaType: %d, mVideoFrame: %p\n", mMediaType, mVideoFrame.get());
        return;
    }

    bufferReiceived_check(buffer);
    bufferReiceived_check(buffer->type() == MediaBuffer::MBT_RawVideo);

    if ( buffer->isFlagSet(MediaBuffer::MBFT_EOS) ) {
        MMLOGV("eos\n");
        notify(Component::kEventEOS, 0, 0, MMParamSP((MMParam*)NULL));
        return;
    }

    uint8_t * img;
    int32_t offsets;
    int32_t strides;
    bufferReiceived_check(buffer->getBufferInfo((uintptr_t*)&img, &offsets, &strides, 1));
    bufferReiceived_check(img);


    size_t sz = (size_t)buffer->size();
    MMLOGV("creating buffer, size: %zu\n", sz);
    mVideoFrame = VideoFrameSP(new VideoFrame());
    bufferReiceived_check(mVideoFrame);

    mVideoFrame->mData = MMBuffer::create(sz);
    bufferReiceived_check(mVideoFrame->mData);

    uint8_t * p = mVideoFrame->mData->getWritableBuffer();
    memcpy(p , img, sz);
    mVideoFrame->mData->setActualSize(sz);

    MediaMetaSP meta = buffer->getMediaMeta();
    if ( meta ) {
        meta->getInt32(MEDIA_ATTR_WIDTH, mVideoFrame->mWidth);
        meta->getInt32(MEDIA_ATTR_HEIGHT, mVideoFrame->mHeight);
        meta->getInt32(MEDIA_ATTR_COLOR_FOURCC, mVideoFrame->mColorFormat);
        meta->getInt32(MEDIA_ATTR_ROTATION, mVideoFrame->mRotationAngle);
        meta->getInt32(MEDIA_ATTR_STRIDE, mVideoFrame->mStride);
    }
    MMLOGV("image size: %zu, width: %d, height: %d, ColorFormat: %d, rotation: %d, stride: %d, img pointer: %p\n",
        sz,
        mVideoFrame->mWidth,
        mVideoFrame->mHeight,
        mVideoFrame->mColorFormat,
        mVideoFrame->mRotationAngle,
        mVideoFrame->mStride,
        img);
    //hexDumpLocal(img, sz, 16);
    notify(kEventInfo, kEventDataReceived, 0, MMParamSP((MMParam*)NULL));
    FUNC_LEAVE();
}

MediaMetaRetrieverImp::ComponentListener::ComponentListener(MediaMetaRetrieverImp * wathcer)
                    : mWatcher(wathcer)
{
    FUNC_ENTER();
    FUNC_LEAVE();
}

void MediaMetaRetrieverImp::ComponentListener::onMessage(
                    int msg,
                    int param1,
                    int param2,
                    const MMParamSP obj,
                    const Component * sender)
{
    FUNC_ENTER();
    mWatcher->onComponentMessage(msg, param1, param2, obj, sender);
}

MediaMetaRetrieverImp::MediaMetaRetrieverImp()
    : mCondition(mLock), mState(kStateNone)
{
    FUNC_ENTER();
    if ( !createListeners()
        || !initSource()
        || !initSinks() ) {
        SET_STATE(kStateError);
        return;
    }

    SET_STATE(kStateInited);
    FUNC_LEAVE();
}

MediaMetaRetrieverImp::~MediaMetaRetrieverImp()
{
    FUNC_ENTER();
    reset();
    FUNC_LEAVE();
}

bool MediaMetaRetrieverImp::initCheck()
{
    MMLOGV("state: %d\n", mState);
    return mState > kStateError;
}

bool MediaMetaRetrieverImp::createListeners()
{
    FUNC_ENTER();
    ComponentInfo * p = mComponents;
    for ( int i = 0; i < kComponentCount; ++i, ++p ) {
        p->listener = ComponentListenerSP(new ComponentListener(this));
        if ( !p->listener ) {
            MMLOGE("failed to create listener\n");
            return false;
        }
    }

    FUNC_LEAVE();
    return true;
}
//#define __USEING_SOFT_VIDEO_CODEC_FOR_MS__
bool MediaMetaRetrieverImp::createByFactory(int componentIndex, const char * mime)
{
    FUNC_ENTER();
    ComponentSP & c = mComponents[componentIndex].component;
#ifdef __USEING_SOFT_VIDEO_CODEC_FOR_MS__
    if (componentIndex == kIdVideoDec) {
        MMLOGV("video sink force to ffmpeg\n");
        c = ComponentFactory::create("FFmpeg", "video/", false);
    } else {
#endif
    c = ComponentFactory::create(NULL, mime, false);
#ifdef __USEING_SOFT_VIDEO_CODEC_FOR_MS__
    }
#endif
    if ( !c ) {
        MMLOGE("failed to create souce component\n");
        return false;
    }
    SET_LISTENER(c,mComponents[componentIndex].listener);
    SET_COMPONENT_STATE(componentIndex, mComponents[componentIndex].state, ComponentInfo::kStateCreated);
    FUNC_LEAVE();
    return true;
}

bool MediaMetaRetrieverImp::initSource()
{
    FUNC_ENTER();
    if ( !createByFactory(kIdSouce, MEDIA_MIMETYPE_MEDIA_DEMUXER) ) {
        return false;
    }

    PlaySourceComponentSP souce = std::tr1::dynamic_pointer_cast<PlaySourceComponent>(mComponents[kIdSouce].component);
    MediaMetaSP param = MediaMeta::create();
    if ( !param ) {
        MMLOGE("no mem\n");
        return false;
    }

    if ( !param->setInt64(PlaySourceComponent::PARAM_KEY_BUFFERING_TIME, 1) ) {
        MMLOGE("failed to setInt64\n");
        return false;
    }
    if ( souce->setParameter(param) != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to set param\n");
        return false;
    }

    FUNC_LEAVE();
    return true;
}

bool MediaMetaRetrieverImp::initSinks()
{
    FUNC_ENTER();
    return initSink(kIdAudioSink, Component::kMediaTypeAudio)
        && initSink(kIdVideoSink, Component::kMediaTypeVideo);
}

bool MediaMetaRetrieverImp::initSink(int index, Component::MediaType type)
{
    FUNC_ENTER();
    ComponentSP & c = mComponents[index].component;
    c = RSinkSP(new RSink(type));
    if ( !c ) {
        MMLOGE("failed to create audio sink component\n");
        return false;
    }

    SET_LISTENER(c, mComponents[index].listener);
    SET_COMPONENT_STATE(index, mComponents[index].state, ComponentInfo::kStateCreated);

    FUNC_LEAVE();
    return true;
}

mm_status_t MediaMetaRetrieverImp::setDataSource(
        const char * dataSourceUrl,
        const std::map<std::string, std::string> * headers/* = NULL*/)
{
    MMASSERT(initCheck());
    MMLOGV("url: %s\n", dataSourceUrl);
    MMAutoLock locker(mLock);
    reset();

    if ( std::tr1::dynamic_pointer_cast<PlaySourceComponent>(mComponents[kIdSouce].component)->setUri(dataSourceUrl, headers) != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to set uri\n");
        return MM_ERROR_OP_FAILED;
    }

    return extractMeta();
}

mm_status_t MediaMetaRetrieverImp::setDataSource(int fd, int64_t offset, int64_t length)
{
    MMLOGI("fd: %d, offset: %" PRId64 ", len: %" PRId64 "\n", fd, offset, length);
    MMASSERT(initCheck());
    MMAutoLock locker(mLock);
    reset();

    if ( std::tr1::dynamic_pointer_cast<PlaySourceComponent>(mComponents[kIdSouce].component)->setUri(fd, offset, length) != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to set fd\n");
        return MM_ERROR_OP_FAILED;
    }

    return extractMeta();
}

mm_status_t MediaMetaRetrieverImp::extractMeta()
{
    FUNC_ENTER();

    std::string audioMime, videoMime;
    mm_status_t ret = prepareSource(audioMime, videoMime);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to prepare source\n");
        return ret;
    }

    if ( !audioMime.empty() ) {
        MMLOGV("has audio\n");
        if ( mComponents[kIdAudioSink].component->addSource(
            mComponents[kIdSouce].component.get(), Component::kMediaTypeAudio
            ) != MM_ERROR_SUCCESS ) {
            MMLOGE("failed to add source to audio sink\n");
            return MM_ERROR_OP_FAILED;
        }
    }

    if ( !videoMime.empty() ) {
        MMLOGV("has video\n");
        if ( mComponents[kIdVideoSink].component->addSource(
            mComponents[kIdSouce].component.get(), Component::kMediaTypeVideo
            ) != MM_ERROR_SUCCESS ) {
            MMLOGE("failed to add source to video sink\n");
            return MM_ERROR_OP_FAILED;
        }
    }

    mAudioMime = audioMime;
    mVideoMime = videoMime;

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

void MediaMetaRetrieverImp::reset()
{
    FUNC_ENTER();
    if ( mComponents[kIdVideoDec].component ) {
        EXEC_COMPONENT(kIdVideoDec, ComponentInfo::kStateStopped, stop());
        WAIT_SINGLE_STATE(&mComponents[kIdVideoDec]);
    }

    resetComponents();

    for ( int i = 0; i < kComponentCount; ++i ) {
        mComponents[i].mMemCosts = 0;
    }

    if ( mComponents[kIdVideoDec].component ) {
        MMLOGV("detach decoder\n");
        mComponents[kIdVideoDec].component.reset();
        MMLOGV("detach decoder over\n");
        SET_COMPONENT_STATE(kIdVideoDec, mComponents[kIdVideoDec].state, ComponentInfo::kStateNotCreated);
    }
    mAudioMime = "";
    mVideoMime = "";

    FUNC_LEAVE();
}
void MediaMetaRetrieverImp::onComponentMessage(int msg,
        int param1,
        int param2,
        const MMParamSP obj,
        const Component * sender)
{
    MMLOGV("msg: %d, param1: %d, param2: %d, sender: %s\n", msg, param1, param2, sender->name());

    ComponentInfo * ci = NULL;
    int i;
    for ( i = 0; i < kComponentCount; ++i ) {
        if ( mComponents[i].component.get() == sender ) {
            ci = &mComponents[i];
            break;
        }
    }

    if ( ci == NULL ) {
        MMLOGE("sender not found\n");
        return;
    }

    switch ( msg ) {
        COMPONENT_STATE_MSGITEM(i, ci, Component::kEventPrepareResult, ComponentInfo::kStatePrepared);
        COMPONENT_STATE_MSGITEM(i, ci, Component::kEventSeekComplete, ComponentInfo::kStateSeekComplete);
        COMPONENT_STATE_MSGITEM(i, ci, Component::kEventStartResult, ComponentInfo::kStateStarted);
        COMPONENT_STATE_MSGITEM_NOFAIL(i, ci, Component::kEventStopped, ComponentInfo::kStateStopped);
        COMPONENT_STATE_MSGITEM_NOFAIL(i, ci, Component::kEventPaused, ComponentInfo::kStatePaused);
        COMPONENT_STATE_MSGITEM_NOFAIL(i, ci, Component::kEventFlushComplete, ComponentInfo::kStateFlushed);
        COMPONENT_STATE_MSGITEM_NOFAIL(i, ci, Component::kEventResetComplete, ComponentInfo::kStateCreated);
        COMPONENT_STATE_MSGITEM_NOFAIL(i, ci, Component::kEventEOS, ComponentInfo::kStateEOS);
        COMPONENT_STATE_MSGITEM_INFO(i, ci, RSink::kEventDataReceived, ComponentInfo::kStateDataReceived);
        case Component::kEventError:
            MMLOGV("msg: %s\n", "kEventError");
            SET_COMPONENT_STATE(i, ci->state, ComponentInfo::kStateError);
            mCondition.signal();
            return;
        default:
            MMLOGV("unknown msg: %d\n", msg);
            break;
    }

    FUNC_LEAVE();
}

mm_status_t MediaMetaRetrieverImp::extractMetadata(MediaMetaSP & fileMeta, MediaMetaSP & audioMeta, MediaMetaSP & videoMeta)
{
    FUNC_ENTER();
    MMASSERT(initCheck());
    MMAutoLock locker(mLock);
    if ( mAudioMime.empty() && mVideoMime.empty() ) {
        MMLOGV("no media\n");
        return MM_ERROR_MALFORMED;
    }

    mComponents[kIdSouce].component->getParameter(fileMeta);
    if ( !mAudioMime.empty() ) {
        mComponents[kIdAudioSink].component->getParameter(audioMeta);
    } else
        audioMeta.reset();
    if ( !mVideoMime.empty() ) {
        mComponents[kIdVideoSink].component->getParameter(videoMeta);
    } else
        videoMeta.reset();

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::prepareSource(std::string & audioMime, std::string & videoMime)
{
    FUNC_ENTER();

    EXEC_COMPONENT(kIdSouce, ComponentInfo::kStatePrepared, prepare());
    ComponentInfo * sci = &mComponents[kIdSouce];
    WAIT_SINGLE_STATE(sci);
    if ( sci->state != ComponentInfo::kStatePrepared ) {
        MMLOGE("source prepare failed\n");
        return MM_ERROR_OP_FAILED;
    }

    PlaySourceComponentSP source(std::tr1::dynamic_pointer_cast<PlaySourceComponent>(mComponents[kIdSouce].component));
    MMASSERT(source != NULL);
    MMParamSP tracks = source->getTrackInfo();
    if ( !tracks ) {
        MMLOGV("no track\n");
        return MM_ERROR_MALFORMED;
    }

    MediaMetaSP meta;
    MMLOGV("getting param\n");
    source->getParameter(meta);
    MMLOGV("getting param over\n");

    int32_t streamCount = tracks->readInt32();
    MMLOGV("stream count: %d\n", streamCount);
    for ( int i=0; i < streamCount; ++i ) {
        int32_t trackType = tracks->readInt32();
        int32_t trackCount = tracks->readInt32();
        MMLOGV("trackType: %d, trackCount=%d\n", trackType, trackCount);

        for ( int j = 0; j < trackCount; j++) {
            int32_t trackId = tracks->readInt32();
            int32_t codecId = tracks->readInt32();
            const char* codecName = tracks->readCString();
            const char* mime = tracks->readCString();
            const char* title = tracks->readCString();
            const char* language = tracks->readCString();

            MMLOGV("trackId=%d, codecId=%d, codecName=%s, mime = %s, title=%s, language=%s\n",
                trackId, codecId, codecName, mime, title, language);
            if ( trackType == Component::kMediaTypeAudio && audioMime.empty() ) {
                audioMime = mime;
            } else if ( trackType == Component::kMediaTypeVideo && videoMime.empty() ) {
                videoMime = mime;
                tracks->readInt32();
                tracks->readInt32();
            }
        }
    }

    if ( audioMime.empty() && videoMime.empty() ) {
        MMLOGV("no av\n");
        return MM_ERROR_MALFORMED;
    }

    if ( audioMime.empty() ) {
        MMLOGV("no audio\n");
        meta->setString(MEDIA_ATTR_HAS_AUDIO, "n");
    } else {
        MMLOGV("has audio\n");
        meta->setString(MEDIA_ATTR_HAS_AUDIO, "y");
    }
    if ( videoMime.empty() ) {
        MMLOGV("no video\n");
        meta->setString(MEDIA_ATTR_HAS_VIDEO, "n");
    } else {
        MMLOGV("has video\n");
        meta->setString(MEDIA_ATTR_HAS_VIDEO, "y");
    }

//    meta->dump();

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

bool MediaMetaRetrieverImp::waitComponentsState(ComponentInfo::State state)
{
    FUNC_ENTER();
    MMLOGV("wait all components to state %d\n", state);
    do {
        int i;
        for ( i = 0; i < kComponentCount; ++i ) {
            ComponentInfo * ci = &mComponents[i];
            if ( !ci->component ) {
                continue;
            }
            if ( ci->state == ComponentInfo::kStateExecuting ) {
                MMLOGV("No. %d not changed\n", i);
                break;
            }
        }
        if ( i >= kComponentCount ) {
            MMLOGV("all state changed\n");
            break;
        }

        mCondition.timedWait(1000000);
    } while ( 1 );

    for ( int i = 0; i < kComponentCount; ++i ) {
        ComponentInfo * ci = &mComponents[i];
        if ( !ci->component ) {
            continue;
        }
        if ( ci->state != state ) {
            MMLOGE("No. %d not reach to sate %d, it reached: %d\n", i, state, ci->state);
            return false;
        }
    }

    MMLOGV("all successfuly changed state\n");
    return true;
}

void MediaMetaRetrieverImp::resetComponents()
{
    FUNC_ENTER();
    for ( int i = 0; i < kComponentCount; ++i ) {
        if ( mComponents[i].component &&
            mComponents[i].state != ComponentInfo::kStateCreated ) {
            EXEC_COMPONENT(i, ComponentInfo::kStateCreated, reset());
        }
    }
    waitComponentsState(ComponentInfo::kStateCreated);
    FUNC_LEAVE();
}

VideoFrameSP MediaMetaRetrieverImp::extractVideoFrame(int64_t timeUs, int32_t * memCosts/* = NULL*/)
{
    FUNC_ENTER();
    MMASSERT(initCheck());
    MMAutoLock locker(mLock);
    if ( mVideoMime.empty() ) {
        MMLOGV("no video\n");
        return VideoFrameSP((VideoFrame*)NULL);
    }

    if ( prepareVideoExtract(timeUs) != MM_ERROR_SUCCESS ) {
        return VideoFrameSP((VideoFrame*)NULL);
    }

    if ( extractVideoFrame() != MM_ERROR_SUCCESS ) {
        return VideoFrameSP((VideoFrame*)NULL);
    }

    RSinkSP videoSink(std::tr1::dynamic_pointer_cast<RSink>(mComponents[kIdVideoSink].component));
    VideoFrameSP frame = videoSink->getFrame();

    EXEC_ALL(ComponentInfo::kStateStopped, pause());
    waitComponentsState(ComponentInfo::kStatePaused);

    EXEC_ALL(ComponentInfo::kStateStopped, flush());
    waitComponentsState(ComponentInfo::kStateFlushed);

    if (frame) {
        MediaMetaSP videoMeta;
        if ( !mVideoMime.empty() )
            mComponents[kIdVideoSink].component->getParameter(videoMeta);

        if (videoMeta) {
            if (frame->mStride == 0) {
                MMLOGW("no stride provided, try set to width\n");
                frame->mStride = frame->mWidth;
            }
            if (frame->mColorFormat == -1) {
                MMLOGW("no colorformat provided, try set to YV12\n");
                frame->mColorFormat = 'YV12';
            }
            if (frame->mRotationAngle == -1) {
                int32_t rot;
                if (videoMeta->getInt32(MEDIA_ATTR_ROTATION, rot)) {
                    MMLOGW("no rotation provided, try set to source provided: %d\n", rot);
                    frame->mRotationAngle = rot;
                } else {
                    MMLOGV("no rotation and no rot at meta, set to 0\n");
                    frame->mRotationAngle = 0;
                }
            }
        }

        if (memCosts) {
            MMLOGV("set memcosts: %d\n", mComponents[kIdVideoSink].mMemCosts);
            *memCosts = mComponents[kIdVideoDec].mMemCosts;
        }
    }

    FUNC_LEAVE();
    return frame;
}

bool MediaMetaRetrieverImp::convertNV12(VideoFrameSP & videoFrame)
{
    MMASSERT(videoFrame != NULL);
    int32_t pixCount = videoFrame->mWidth * videoFrame->mHeight;

    struct SwsContext * sws = sws_getContext(videoFrame->mWidth,
                                videoFrame->mHeight,
                                AV_PIX_FMT_NV12,
                                videoFrame->mWidth,
                                videoFrame->mHeight,
                                AV_PIX_FMT_YUV420P,
                                SWS_BICUBIC,
                                NULL,
                                NULL,
                                NULL);

    MMBufferSP dstBuf = MMBuffer::create((pixCount * 3) >> 1);
    if (!dstBuf) {
        MMLOGE("no mem\n");
        return false;
    }

    const uint8_t * input[4];
    input[0] = videoFrame->mData->getBuffer();
    input[1] = videoFrame->mData->getBuffer() + videoFrame->mStride * videoFrame->mHeight;
    input[2] = input[3] = NULL;
    int inputSize[4] = {(int)videoFrame->mStride, (int)videoFrame->mStride};
    input[2] = input[3] = 0;
    uint8_t * output[4];
    output[0] = dstBuf->getWritableBuffer();
    output[1] = dstBuf->getWritableBuffer() + pixCount;
    output[2] = dstBuf->getWritableBuffer() + ((pixCount * 5) >> 2);
    output[3] = NULL;
    int outputSize[4] = {(int)videoFrame->mWidth, (int)(videoFrame->mWidth >> 1), (int)(videoFrame->mWidth >> 1), 0};
    int dh = sws_scale(sws,
        input,
        inputSize,
        0,
        videoFrame->mHeight,
        output,
        outputSize);
    MMLOGV("scal ret: %d, w: %u, h: %u\n", dh, videoFrame->mWidth, videoFrame->mHeight);
    dstBuf->setActualSize(dstBuf->getSize());
    videoFrame->mData = dstBuf;
    sws_freeContext(sws);
    sws = NULL;

    videoFrame->mColorFormat = 'YV12';
    videoFrame->mWidth = videoFrame->mWidth;
    videoFrame->mHeight = videoFrame->mHeight;
    videoFrame->mStride = videoFrame->mWidth;

    return true;
}

VideoFrameSP MediaMetaRetrieverImp::extractVideoFrame(int64_t timeUs,
                RGB::Format format,
                uint32_t scaleNum/* = 1*/,
                uint32_t scaleDenom/* = 1*/)
{
    MMLOGV("timeUs: %" PRId64 ", format: %d, scaleNum: %u, scaleDemon: %u\n", timeUs, format, scaleNum, scaleDenom);
    if (scaleDenom == 0) {
        MMLOGE("invalid demon: %u\n", scaleDenom);
        return VideoFrameSP((VideoFrame*)NULL);
    }
    VideoFrameSP vf = extractVideoFrame(timeUs);
    if (!vf) {
        return vf;
    }

    if (vf->mColorFormat == 'NV12') {
        MMLOGV("NV12, convert\n");
        if (!convertNV12(vf)) {
            MMLOGE("failed to convert nv12\n");
            return VideoFrameSP((VideoFrame*)NULL);
        }
    }

    if (vf->mColorFormat != 'YV12' && vf->mColorFormat != '420p') {
        MMLOGE("unsupported colorformat: %d\n", vf->mColorFormat);
        return VideoFrameSP((VideoFrame*)NULL);
    }

    int32_t dstSize;
    AVPixelFormat dstFormat;
    int32_t dstFourcc;
    int dstStride;
    RGB::Format rightFormat = RGB::kFormatRGB24;
    int32_t dstWidth;
    int32_t dstHeight;
    if (scaleNum != scaleDenom) {
        double factor = (double)scaleNum / (double)scaleDenom;
        dstWidth = vf->mWidth * factor;
        dstHeight = vf->mHeight * factor;
        if (dstWidth & 1) {
            dstWidth++;
        }
        if (dstHeight & 1) {
            dstHeight++;
        }
        MMLOGV("dstWidth: %u, dstHeight: %u\n", dstWidth, dstHeight);
    } else {
        dstWidth = vf->mWidth;
        dstHeight = vf->mHeight;
    }
    switch (rightFormat) {
        case RGB::kFormatRGB32:
            dstFormat = AV_PIX_FMT_RGBA;
            dstFourcc = 'RGB4';
            dstSize = (dstWidth * dstHeight) << 2;
            dstStride = 4;
            break;
        case RGB::kFormatRGB24:
            dstFormat = AV_PIX_FMT_RGB24;
            dstFourcc = 'RGB3';
            dstSize = (dstWidth * dstHeight) * 3;
            dstStride = 3;
            break;
        case RGB::kFormatRGB565:
            {
                unsigned short a = 0x1234;
                if (((char *)&a)[0] == 0x12) {
                    dstFormat = AV_PIX_FMT_RGB565BE;
                } else {
                    dstFormat = AV_PIX_FMT_RGB565LE;
                }
                dstFourcc = 'RGB2';
                dstSize = (dstWidth * dstHeight) << 1;
                dstStride = 2;
            }
            break;
        default:
            MMLOGE("unsupported format: %d\n", format);
            return VideoFrameSP((VideoFrame*)NULL);
    }

    struct SwsContext * sws = sws_getContext(vf->mWidth,
                                vf->mHeight,
                                AV_PIX_FMT_YUV420P,
                                dstWidth,
                                dstHeight,
                                dstFormat,
                                SWS_BICUBIC,
                                NULL,
                                NULL,
                                NULL);
    if (!sws) {
        MMLOGE("failed to get conext\n");
        return VideoFrameSP((VideoFrame*)NULL);
    }

    MMBufferSP dstBuf = MMBuffer::create(dstSize);
    if (!dstBuf) {
        MMLOGE("no mem, need: %d\n", dstSize);
        sws_freeContext(sws);
        return VideoFrameSP((VideoFrame*)NULL);
    }

    int32_t pixCountSrc = vf->mWidth * vf->mHeight;
    const uint8_t * input[4];
    input[0] = vf->mData->getBuffer();
    input[1] = vf->mData->getBuffer() + pixCountSrc;
    input[2] = vf->mData->getBuffer() + ((pixCountSrc * 5) >> 2);
    input[3] = NULL;
    int inputSize[4] = {(int)vf->mWidth, (int)(vf->mWidth >> 1), (int)(vf->mWidth >> 1), 0};
    uint8_t * output[4];
    output[0] = dstBuf->getWritableBuffer();
    output[1] = output[2] = output[3] = NULL;
    int outputSize[4] = {dstStride * dstWidth, 0, 0, 0};
    MMLOGV("call scale, dstSize: %d\n", dstSize);
    sws_scale(sws,
        input,
        inputSize,
        0,
        vf->mHeight,
        output,
        outputSize);
    dstBuf->setActualSize(dstBuf->getSize());
    vf->mData = dstBuf;
    vf->mColorFormat = dstFourcc;
    vf->mStride = 1;
    vf->mWidth = dstWidth;
    vf->mHeight = dstHeight;
    sws_freeContext(sws);
    sws = NULL;

    if (rightFormat != format) {
        MMLOGV("convert format\n");
        MMBufferSP b = RGB::convertFormat(rightFormat,
            format,
            dstWidth,
            dstHeight,
            vf->mData);
        if (!b) {
            MMLOGE("no mem\n");
            return VideoFrameSP((VideoFrame*)NULL);
        }
        vf->mData = b;
    }

    MMLOGV("over\n");
    return vf;
}

mm_status_t MediaMetaRetrieverImp::prepareVideoExtract(int64_t timeUs)
{
    FUNC_ENTER();
    if ( mComponents[kIdVideoDec].component ) {
        RSinkSP videoSink(std::tr1::dynamic_pointer_cast<RSink>(mComponents[kIdVideoSink].component));
        videoSink->cleanFrame();
    } else {
        VIDEO_EXTRACT_PREPARE_EXEC(createVideoDec());
        VIDEO_EXTRACT_PREPARE_EXEC(connectComponentsForVideoExtract());
        VIDEO_EXTRACT_PREPARE_EXEC(prepareVideoDec());
    }

    if (seekSource(timeUs) != MM_ERROR_SUCCESS) {
        MMLOGV("seek failed, clean video dec\n");
        EXEC_COMPONENT(kIdVideoDec, ComponentInfo::kStateStopped, stop());
        WAIT_SINGLE_STATE(&mComponents[kIdVideoDec]);
        EXEC_COMPONENT(kIdVideoDec, ComponentInfo::kStateCreated, reset());
        WAIT_SINGLE_STATE(&mComponents[kIdVideoDec]);
        mComponents[kIdVideoDec].component.reset();
        return MM_ERROR_OP_FAILED;
    }

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::createVideoDec()
{
    FUNC_ENTER();
    if ( !createByFactory(kIdVideoDec, mVideoMime.c_str()) ) {
        MMLOGE("Failed to create videodec component\n");
        return MM_ERROR_OP_FAILED;
    }

    MMLOGV("success\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::prepareVideoDec()
{
    EXEC_COMPONENT(kIdVideoDec, ComponentInfo::kStatePrepared, prepare());
    WAIT_SINGLE_STATE(&mComponents[kIdVideoDec]);
    if ( mComponents[kIdVideoDec].state != ComponentInfo::kStatePrepared ) {
        MMLOGE("failed to set video listner\n");
        return MM_ERROR_OP_FAILED;
    }

    MMLOGV("success\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::seekSource(int64_t timeUs)
{
    FUNC_ENTER();
    PlaySourceComponentSP source(std::tr1::dynamic_pointer_cast<PlaySourceComponent>(mComponents[kIdSouce].component));
    int pos;
    int64_t durationMs;
    if ( source->getDuration(durationMs) != MM_ERROR_SUCCESS ) {
        MMLOGV("dur not determined\n");
        return MM_ERROR_OP_FAILED;
    }
    if ( timeUs < 0 ) {
        if ( (int)durationMs > 5000 ) {
            pos = 5000;
            MMLOGV("timeus < 0, set to %d(duration: %d)\n", pos, (int)durationMs);
        } else {
            pos = durationMs / 2;
            MMLOGV("timeus < 0, set to %d(duration: %d)\n", pos, (int)durationMs);
        }
    } else {
        pos = timeUs / 1000;
        MMLOGV("us req timems: %d\n", pos);
    }

    EXEC_COMPONENT(kIdSouce, ComponentInfo::kStateSeekComplete, seek(pos, 0));
    WAIT_SINGLE_STATE(&mComponents[kIdSouce]);
    if ( mComponents[kIdSouce].state != ComponentInfo::kStateSeekComplete ) {
        MMLOGE("failed to seek to %d\n", pos);
        return MM_ERROR_OP_FAILED;
    }

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::connectComponentsForVideoExtract()
{
    FUNC_ENTER();
    if ( !mAudioMime.empty() ) {
        MMLOGV("has audio\n");
        if ( mComponents[kIdAudioSink].component->addSource(
            mComponents[kIdSouce].component.get(), Component::kMediaTypeAudio
            ) != MM_ERROR_SUCCESS ) {
            MMLOGE("failed to add source to audio sink\n");
            return MM_ERROR_OP_FAILED;
        }
    }

    if ( mComponents[kIdVideoDec].component->addSource(
        mComponents[kIdSouce].component.get(), Component::kMediaTypeVideo)
        != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to add source to video decoder\n");
        return MM_ERROR_OP_FAILED;
    }

    if ( mComponents[kIdVideoDec].component->addSink(
        mComponents[kIdVideoSink].component.get(), Component::kMediaTypeVideo)
        != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to add source to video decoder\n");
        return MM_ERROR_OP_FAILED;
    }

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaMetaRetrieverImp::extractVideoFrame()
{
    FUNC_ENTER();
    EXEC_ALL(ComponentInfo::kStateStarted, start());
    if ( !waitComponentsState(ComponentInfo::kStateStarted) &&
        mComponents[kIdVideoSink].state != ComponentInfo::kStateDataReceived ) {
        MMLOGE("start failed\n");
        return MM_ERROR_OP_FAILED;
    }

    ComponentInfo * ci = &mComponents[kIdVideoSink];
    while ( ci->state == ComponentInfo::kStateStarted ) {
        MMLOGV("video sink now state: %d\n", ci->state);
        RSinkSP videoSink(std::tr1::dynamic_pointer_cast<RSink>(ci->component));
        if (videoSink->getFrame() != NULL) {
            ci->state = ComponentInfo::kStateDataReceived;
        }
        if (hasComponentError()) {
            break;
        }
        mCondition.timedWait(1000000);
    };

    if ( ci->state != ComponentInfo::kStateDataReceived ) {
        RSinkSP videoSink(std::tr1::dynamic_pointer_cast<RSink>(ci->component));
        if ( ci->state == ComponentInfo::kStateEOS &&
            videoSink->getFrame() != NULL ) {
            MMLOGV("EOS and no frame\n");
            return MM_ERROR_SUCCESS;
        }

        MMLOGV("no data received\n");
        return MM_ERROR_OP_FAILED;
    }

    FUNC_LEAVE();
    return MM_ERROR_SUCCESS;
}

CoverSP MediaMetaRetrieverImp::extractCover()
{
    FUNC_ENTER();
    MMASSERT(initCheck());
    MMAutoLock locker(mLock);
    if ( mAudioMime.empty() ) {
        MMLOGV("no audio\n");
        return (CoverSP((Cover*)NULL));
    }

    MediaMetaSP meta;
    mComponents[kIdSouce].component->getParameter(meta);

    void * p;
    if ( !meta->getPointer(MEDIA_ATTR_ATTACHEDPIC, p) ) {
        MMLOGV("no cover\n");
        return (CoverSP((Cover*)NULL));
    }
    int32_t size;
    if ( !meta->getInt32(MEDIA_ATTR_ATTACHEDPIC_SIZE, size) ) {
        MMLOGE("has cover, but size not set\n");
        return (CoverSP((Cover*)NULL));
    }

    CoverSP cover(new Cover());
    if ( !cover ) {
        MMLOGE("no mem\n");
        return (CoverSP((Cover*)NULL));
    }

    cover->mData = MMBuffer::create(size);
    if ( !cover->mData ) {
        MMLOGE("no mem, need %d\n", size);
        return (CoverSP((Cover*)NULL));
    }
    uint8_t * buf_p = cover->mData->getWritableBuffer();
    memcpy(buf_p, p, size);
    cover->mData->setActualSize(size);

    MMLOGV("ret size: %zu\n", size);
    return cover;
}

bool MediaMetaRetrieverImp::hasComponentError() const
{
    for ( int i = 0; i < kComponentCount; ++i ) {
        const ComponentInfo * ci = &mComponents[i];
        if ( !ci->component ) {
            continue;
        }
        if ( ci->state == ComponentInfo::kStateError ) {
            MMLOGE("No. %d in errr state\n", i);
            return true;
        }
    }

    return false;
}


}
