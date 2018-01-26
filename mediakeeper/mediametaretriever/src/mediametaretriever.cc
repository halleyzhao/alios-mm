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

#include <multimedia/mediametaretriever.h>
#include <multimedia/mediametar.h>
#include "mediametaretriever_imp.h"
#include <multimedia/media_attr_str.h>

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(MediaMetaRetriever)

/*static */MediaMetaRetriever * MediaMetaRetriever::create()
{
    MMLOGI("+\n");

    MediaMetaRetriever * m = new MediaMetaRetrieverImp();
    if ( !m ) {
        MMLOGE("no mem\n");
        return NULL;
    }

    if ( !m->initCheck() ) {
        MMLOGE("init failed\n");
        MM_RELEASE(m);
        return NULL;
    }

    MMLOGI("-\n");
    return m;
}

/*static */void MediaMetaRetriever::destroy(MediaMetaRetriever * retriever)
{
    MMLOGI("+\n");
    MM_RELEASE(retriever);
    MMLOGI("-\n");
}

}

extern "C"
{
static const char *MM_LOG_TAG = "MediaMetaR";
static YUNOS_MM::MediaMetaRetriever * r = NULL;
int extractAudioFileMetadata(char * file, comp_audio_meta_tt *audio_meta)
{
    if ( !r ) {
        r = YUNOS_MM::MediaMetaRetriever::create();
        if ( !r ) {
            MMLOGE("failed to create r\n");
            return MM_ERROR_OP_FAILED;
        }
    }
    mm_status_t ret = r->setDataSource(file);
    if ( ret != MM_ERROR_SUCCESS ) {
        return ret;
    }

    YUNOS_MM::MediaMetaSP fileMeta;
    YUNOS_MM::MediaMetaSP audioMeta;
    YUNOS_MM::MediaMetaSP videoMeta;
    ret = r->extractMetadata(fileMeta, audioMeta, videoMeta);
    if ( ret != MM_ERROR_SUCCESS ) {
        return ret;
    }

    if ( fileMeta ) {
        int64_t durationMs;
        fileMeta->getInt64(YUNOS_MM::MEDIA_ATTR_DURATION, durationMs);
        audio_meta->commonmeta.duration = durationMs;

        int32_t bitrate;
        fileMeta->getInt32(YUNOS_MM::MEDIA_ATTR_BIT_RATE, bitrate);
        audio_meta->commonmeta.audio_bitrate = bitrate;
    }

    if ( audioMeta ) {
        int32_t sample_rate;
        fileMeta->getInt32(YUNOS_MM::MEDIA_ATTR_SAMPLE_RATE, sample_rate);
        audio_meta->commonmeta.sample_rate = sample_rate;

        int32_t num_channel;
        fileMeta->getInt32(YUNOS_MM::MEDIA_ATTR_CHANNEL_COUNT, num_channel);
        audio_meta->commonmeta.num_channel = num_channel;
        audioMeta->dump();
    }

    return (int)MM_ERROR_SUCCESS;

}

int extractVideoFileMetadata(char * file, comp_video_meta_tt *video_meta)
{

    if ( !r ) {
        r = YUNOS_MM::MediaMetaRetriever::create();
        if ( !r ) {
            MMLOGE("failed to create r\n");
            return MM_ERROR_NO_MEM;
        }
    }
    mm_status_t ret = r->setDataSource(file);
    if ( ret != MM_ERROR_SUCCESS ) {
        return ret;
    }

    YUNOS_MM::MediaMetaSP fileMeta;
    YUNOS_MM::MediaMetaSP audioMeta;
    YUNOS_MM::MediaMetaSP videoMeta;
    ret = r->extractMetadata(fileMeta, audioMeta, videoMeta);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGE("no mem\n");
        return ret;
    }

    if ( fileMeta ) {
        int64_t durationMs;
        fileMeta->getInt64(YUNOS_MM::MEDIA_ATTR_DURATION, durationMs);
        video_meta->commonmeta.duration = durationMs;

        int32_t bitrate;
        fileMeta->getInt32(YUNOS_MM::MEDIA_ATTR_BIT_RATE, bitrate);
        video_meta->videometa.video_br = bitrate;
    }

    if ( videoMeta ) {
        int width;
        videoMeta->getInt32(YUNOS_MM::MEDIA_ATTR_WIDTH, width);
        video_meta->videometa.video_w = width;

        int height;
        videoMeta->getInt32(YUNOS_MM::MEDIA_ATTR_HEIGHT, height);
        video_meta->videometa.video_h = height;

        int framerate;
        videoMeta->getInt32(YUNOS_MM::MEDIA_ATTR_AVG_FRAMERATE, framerate);
        video_meta->videometa.video_fps = framerate;
    }

    return (int)MM_ERROR_SUCCESS;

}

void releaseMediaMetaRetriever()
{
    if ( r ) {
        YUNOS_MM::MediaMetaRetriever::destroy(r);
        r = NULL;
    }
}

}

