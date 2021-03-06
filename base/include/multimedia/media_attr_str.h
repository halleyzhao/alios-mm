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

#ifndef media_attr_str_h
#define media_attr_str_h

#include <multimedia/codec.h>

namespace YUNOS_MM {

    #define DEFINE_MEDIA_MIMETYPE(TYPE) extern const char* MEDIA_MIMETYPE_##TYPE;
    #define DEFINE_MEDIA_ATTR(TYPE)  extern const char* MEDIA_ATTR_##TYPE;
    #define DEFINE_MEDIA_META(TYPE)  extern const char* MEDIA_META_##TYPE;

    // mimetype
    DEFINE_MEDIA_MIMETYPE(IMAGE_JPEG)
    DEFINE_MEDIA_MIMETYPE(IMAGE_PNG)
    DEFINE_MEDIA_MIMETYPE(IMAGE_BMP)
    DEFINE_MEDIA_MIMETYPE(IMAGE_GIF)
    DEFINE_MEDIA_MIMETYPE(IMAGE_WEBP)
    DEFINE_MEDIA_MIMETYPE(IMAGE_WBMP)

    DEFINE_MEDIA_MIMETYPE(VIDEO_VP3)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VP5)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VP6)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VP6A)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VP6F)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VP7)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VP8)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VP9)
    DEFINE_MEDIA_MIMETYPE(VIDEO_AVC)
    DEFINE_MEDIA_MIMETYPE(VIDEO_HEVC)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MPEG4)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MPEG4V1)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MPEG4V2)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MPEG4V3)
    DEFINE_MEDIA_MIMETYPE(VIDEO_H263)
    DEFINE_MEDIA_MIMETYPE(VIDEO_H263_P)
    DEFINE_MEDIA_MIMETYPE(VIDEO_H263_I)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MPEG2)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MPEG2_XVMC)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MPEG1)
    DEFINE_MEDIA_MIMETYPE(VIDEO_H261)
    DEFINE_MEDIA_MIMETYPE(VIDEO_RV10)
    DEFINE_MEDIA_MIMETYPE(VIDEO_RV20)
    DEFINE_MEDIA_MIMETYPE(VIDEO_RV30)
    DEFINE_MEDIA_MIMETYPE(VIDEO_RV40)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MJPEG)
    DEFINE_MEDIA_MIMETYPE(VIDEO_MJPEGB)
    DEFINE_MEDIA_MIMETYPE(VIDEO_LJPEG)
    DEFINE_MEDIA_MIMETYPE(VIDEO_JPEGGLS)
    DEFINE_MEDIA_MIMETYPE(VIDEO_SP5X)
    DEFINE_MEDIA_MIMETYPE(VIDEO_WMV)
    DEFINE_MEDIA_MIMETYPE(VIDEO_WMV1)
    DEFINE_MEDIA_MIMETYPE(VIDEO_WMV2)
    DEFINE_MEDIA_MIMETYPE(VIDEO_WMV3)
    DEFINE_MEDIA_MIMETYPE(VIDEO_WMV3IMAGE)
    DEFINE_MEDIA_MIMETYPE(VIDEO_VC1)
    DEFINE_MEDIA_MIMETYPE(VIDEO_FLV)
    DEFINE_MEDIA_MIMETYPE(VIDEO_DV)
    DEFINE_MEDIA_MIMETYPE(VIDEO_HUFFYUV)
    DEFINE_MEDIA_MIMETYPE(VIDEO_CYUV)
    DEFINE_MEDIA_MIMETYPE(VIDEO_INDEOX)
    DEFINE_MEDIA_MIMETYPE(VIDEO_THEORA)
    DEFINE_MEDIA_MIMETYPE(VIDEO_TIFF)
    DEFINE_MEDIA_MIMETYPE(VIDEO_GIF)
    DEFINE_MEDIA_MIMETYPE(VIDEO_DVB)
    DEFINE_MEDIA_MIMETYPE(VIDEO_FLASH)
    DEFINE_MEDIA_MIMETYPE(VIDEO_RAW)

    DEFINE_MEDIA_MIMETYPE(CONTAINER_WVM)
    DEFINE_MEDIA_MIMETYPE(CONTAINER_WAV)
    DEFINE_MEDIA_MIMETYPE(CONTAINER_MPEG2PS)
    DEFINE_MEDIA_MIMETYPE(CONTAINER_OGG)
    DEFINE_MEDIA_MIMETYPE(CONTAINER_AVI)
    DEFINE_MEDIA_MIMETYPE(CONTAINER_MATROSKA)
    DEFINE_MEDIA_MIMETYPE(CONTAINER_MPEG4)
    DEFINE_MEDIA_MIMETYPE(CONTAINER_MPEG2TS)

    DEFINE_MEDIA_MIMETYPE(AUDIO_AAC_ADTS)
    DEFINE_MEDIA_MIMETYPE(AUDIO_AAC_LATM)
    DEFINE_MEDIA_MIMETYPE(AUDIO_AMR_NB)
    DEFINE_MEDIA_MIMETYPE(AUDIO_3GPP)
    DEFINE_MEDIA_MIMETYPE(AUDIO_WMALOSSLESS)
    DEFINE_MEDIA_MIMETYPE(AUDIO_AMR_WB)
    DEFINE_MEDIA_MIMETYPE(AUDIO_WMAPRO)
    DEFINE_MEDIA_MIMETYPE(AUDIO_MPEG)   // layer III
    DEFINE_MEDIA_MIMETYPE(AUDIO_MPEG_ADU)   // layer III
    DEFINE_MEDIA_MIMETYPE(AUDIO_MPEG_ON4)   // layer III
    DEFINE_MEDIA_MIMETYPE(AUDIO_WMA)
    DEFINE_MEDIA_MIMETYPE(AUDIO_MPEG_LAYER_I)
    DEFINE_MEDIA_MIMETYPE(AUDIO_WAV)
    DEFINE_MEDIA_MIMETYPE(AUDIO_MPEG_LAYER_II)
    DEFINE_MEDIA_MIMETYPE(AUDIO_WMV)
    DEFINE_MEDIA_MIMETYPE(AUDIO_AAC)
    DEFINE_MEDIA_MIMETYPE(AUDIO_MP3)
    DEFINE_MEDIA_MIMETYPE(AUDIO_ADTS_PROFILE)
    DEFINE_MEDIA_MIMETYPE(AUDIO_MP2)
    DEFINE_MEDIA_MIMETYPE(AUDIO_QCELP)
    DEFINE_MEDIA_MIMETYPE(AUDIO_DRA288)
    DEFINE_MEDIA_MIMETYPE(AUDIO_VORBIS)
    DEFINE_MEDIA_MIMETYPE(AUDIO_DRA144)
    DEFINE_MEDIA_MIMETYPE(AUDIO_OPUS)
    DEFINE_MEDIA_MIMETYPE(AUDIO_DTSHD)
    DEFINE_MEDIA_MIMETYPE(AUDIO_G711_ALAW)
    DEFINE_MEDIA_MIMETYPE(AUDIO_EC3)
    DEFINE_MEDIA_MIMETYPE(AUDIO_G711_MLAW)
    DEFINE_MEDIA_MIMETYPE(AUDIO_AC3)
    DEFINE_MEDIA_MIMETYPE(AUDIO_RAW)
    DEFINE_MEDIA_MIMETYPE(AUDIO_MSGSM)
    DEFINE_MEDIA_MIMETYPE(AUDIO_FLAC)
    DEFINE_MEDIA_MIMETYPE(AUDIO_DTS)

    DEFINE_MEDIA_MIMETYPE(TEXT_3GPP)
    DEFINE_MEDIA_MIMETYPE(TEXT_SUBRIP)
    DEFINE_MEDIA_MIMETYPE(TEXT_VTT)
    DEFINE_MEDIA_MIMETYPE(TEXT_CEA_608)

    DEFINE_MEDIA_ATTR(FILE_MAJOR_BAND)
    DEFINE_MEDIA_ATTR(FILE_MINOR_VERSION)
    DEFINE_MEDIA_ATTR(FILE_COMPATIBLE_BRANDS)
    DEFINE_MEDIA_ATTR(FILE_CREATION_TIME)

    DEFINE_MEDIA_ATTR(MIME)
    DEFINE_MEDIA_ATTR(CONTAINER)
    DEFINE_MEDIA_ATTR(CODECID)
    DEFINE_MEDIA_ATTR(CODECTAG)
    DEFINE_MEDIA_ATTR(STREAMCODECTAG)
    DEFINE_MEDIA_ATTR(CODECPROFILE)
    DEFINE_MEDIA_ATTR(CODEC_CONTEXT)
    DEFINE_MEDIA_ATTR(CODEC_CONTEXT_MUTEX)
    DEFINE_MEDIA_ATTR(TARGET_TIME)

    //component mimeType
    DEFINE_MEDIA_MIMETYPE(AUDIO_RENDER)
    DEFINE_MEDIA_MIMETYPE(VIDEO_RENDER)
    DEFINE_MEDIA_MIMETYPE(MEDIA_DEMUXER)
    DEFINE_MEDIA_MIMETYPE(MEDIA_APP_SOURCE)
    DEFINE_MEDIA_MIMETYPE(MEDIA_RTP_DEMUXER)
    DEFINE_MEDIA_MIMETYPE(MEDIA_DASH_DEMUXER)
    DEFINE_MEDIA_MIMETYPE(MEDIA_MUXER)
    DEFINE_MEDIA_MIMETYPE(MEDIA_RTP_MUXER)
    DEFINE_MEDIA_MIMETYPE(VIDEO_SOURCE)
    DEFINE_MEDIA_MIMETYPE(AUDIO_SOURCE)
    DEFINE_MEDIA_MIMETYPE(AUDIO_SOURCE_FILE)
    DEFINE_MEDIA_MIMETYPE(VIDEO_TEST_SOURCE)
    DEFINE_MEDIA_MIMETYPE(VIDEO_UVC_SOURCE)
    DEFINE_MEDIA_MIMETYPE(VIDEO_FILE_SOURCE)
    DEFINE_MEDIA_MIMETYPE(VIDEO_WFD_SOURCE)
    DEFINE_MEDIA_MIMETYPE(VIDEO_SCREEN_SOURCE)
    DEFINE_MEDIA_MIMETYPE(VIDEO_SURFACE_SOURCE)
    DEFINE_MEDIA_MIMETYPE(MEDIA_FILE_SINK)
    DEFINE_MEDIA_MIMETYPE(IMAGE_CAMERA_SOURCE)
    DEFINE_MEDIA_MIMETYPE(MEDIA_CAMERA_SOURCE)
    DEFINE_MEDIA_MIMETYPE(SUBTITLE_SOURCE)
    DEFINE_MEDIA_MIMETYPE(SUBTITLE_SINK)
    DEFINE_MEDIA_MIMETYPE(MEDIA_EXTERNAL_SOURCE)

    // media attributes
    DEFINE_MEDIA_ATTR(AAC_PROFILE)
    DEFINE_MEDIA_ATTR(BIT_RATE)
    DEFINE_MEDIA_ATTR(BIT_RATE_AUDIO)
    DEFINE_MEDIA_ATTR(BIT_RATE_VIDEO)
    DEFINE_MEDIA_ATTR(CHANNEL_COUNT)
    DEFINE_MEDIA_ATTR(CHANNEL_MASK)
    DEFINE_MEDIA_ATTR(SAMPLE_FORMAT)
    DEFINE_MEDIA_ATTR(COLOR_FORMAT)
    DEFINE_MEDIA_ATTR(COLOR_FOURCC)
    DEFINE_MEDIA_ATTR(DURATION)
    DEFINE_MEDIA_ATTR(FLAC_COMPRESSION_LEVEL)
    DEFINE_MEDIA_ATTR(FRAME_RATE)
    DEFINE_MEDIA_ATTR(HEIGHT)
    DEFINE_MEDIA_ATTR(IS_ADTS)
    DEFINE_MEDIA_ATTR(IS_AUTOSELECT)
    DEFINE_MEDIA_ATTR(IS_DEFAULT)
    DEFINE_MEDIA_ATTR(IS_FORCED_SUBTITLE)
    DEFINE_MEDIA_ATTR(I_FRAME_INTERVAL)
    DEFINE_MEDIA_ATTR(LANGUAGE)
    DEFINE_MEDIA_ATTR(MAX_HEIGHT)
    DEFINE_MEDIA_ATTR(SLICE_HEIGHT)
    DEFINE_MEDIA_ATTR(MAX_INPUT_SIZE)
    DEFINE_MEDIA_ATTR(MAX_WIDTH)
    DEFINE_MEDIA_ATTR(MAX_FILE_SIZE)
    DEFINE_MEDIA_ATTR(MAX_DURATION)
    DEFINE_MEDIA_ATTR(PUSH_BLANK_BUFFERS_ON_STOP)
    DEFINE_MEDIA_ATTR(REPEAT_PREVIOUS_FRAME_AFTER)
    DEFINE_MEDIA_ATTR(SAMPLE_RATE)
    DEFINE_MEDIA_ATTR(WIDTH)
    DEFINE_MEDIA_ATTR(PREVIEW_WIDTH)
    DEFINE_MEDIA_ATTR(PREVIEW_HEIGHT)
    DEFINE_MEDIA_ATTR(IMAGE_WIDTH)
    DEFINE_MEDIA_ATTR(IMAGE_HEIGHT)
    DEFINE_MEDIA_ATTR(STRIDE)
    DEFINE_MEDIA_ATTR(STRIDE_X)
    DEFINE_MEDIA_ATTR(STRIDE_Y)
    DEFINE_MEDIA_ATTR(CROP_RECT)
    DEFINE_MEDIA_ATTR(VOLUME)
    DEFINE_MEDIA_ATTR(MUTE)
    DEFINE_MEDIA_ATTR(AVG_FRAMERATE)
    DEFINE_MEDIA_ATTR(AVC_PROFILE)
    DEFINE_MEDIA_ATTR(AVC_LEVEL)
    DEFINE_MEDIA_ATTR(HEVC_PROFILE)
    DEFINE_MEDIA_ATTR(HEVC_LEVEL)
    DEFINE_MEDIA_ATTR(IDR_FRAME)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE)

    DEFINE_MEDIA_ATTR(STREAM_INDEX)
    DEFINE_MEDIA_ATTR(AUDIO_SAMPLES)
    DEFINE_MEDIA_ATTR(SAMPLE_ASPECT_RATION)
    DEFINE_MEDIA_ATTR(CHANNEL_LAYOUT)

    DEFINE_MEDIA_ATTR(DECODE_MODE)
    DEFINE_MEDIA_ATTR(DECODE_THUMBNAIL)
    DEFINE_MEDIA_ATTR(THUMB_PATH)
    DEFINE_MEDIA_ATTR(THUMB_WIDTH)
    DEFINE_MEDIA_ATTR(THUMB_HEIGHT)
    DEFINE_MEDIA_ATTR(MICROTHUMB_PATH)
    DEFINE_MEDIA_ATTR(MICROTHUMB_WIDTH)
    DEFINE_MEDIA_ATTR(MICROTHUMB_HEIGHT)
    DEFINE_MEDIA_ATTR(COVER_PATH)
    DEFINE_MEDIA_ATTR(MIME_AUDIO_3GPP)
    DEFINE_MEDIA_ATTR(MIME_AUDIO_QUICKTIME)
    DEFINE_MEDIA_ATTR(MIME_AUDIO_MP4)
    DEFINE_MEDIA_ATTR(MIME_VIDEO_3GPP)
    DEFINE_MEDIA_ATTR(MIME_VIDEO_QUICKTIME)
    DEFINE_MEDIA_ATTR(MIME_VIDEO_MP4)
    DEFINE_MEDIA_ATTR(ALBUM)
    DEFINE_MEDIA_ATTR(HAS_AUDIO)
    DEFINE_MEDIA_ATTR(HAS_VIDEO)
    DEFINE_MEDIA_ATTR(HAS_COVER)
    DEFINE_MEDIA_ATTR(ATTACHEDPIC)
    DEFINE_MEDIA_ATTR(ATTACHEDPIC_SIZE)
    DEFINE_MEDIA_ATTR(ATTACHEDPIC_CODECID)
    DEFINE_MEDIA_ATTR(ATTACHEDPIC_MIME)
    DEFINE_MEDIA_ATTR(YES)
    DEFINE_MEDIA_ATTR(NO)
    DEFINE_MEDIA_ATTR(TITLE)
    DEFINE_MEDIA_ATTR(SUBTITLE)
    DEFINE_MEDIA_ATTR(AUTHOR)
    DEFINE_MEDIA_ATTR(COMMENT)
    DEFINE_MEDIA_ATTR(DESCRIPTION)
    DEFINE_MEDIA_ATTR(CATEGORY)
    DEFINE_MEDIA_ATTR(GENRE)
    DEFINE_MEDIA_ATTR(YEAR)
    DEFINE_MEDIA_ATTR(DATE)
    DEFINE_MEDIA_ATTR(USER_RATING)
    DEFINE_MEDIA_ATTR(KEYWORDS)
    DEFINE_MEDIA_ATTR(PUBLISHER)
    DEFINE_MEDIA_ATTR(COPYRIGHT)
    DEFINE_MEDIA_ATTR(PARENTAL_RATING)
    DEFINE_MEDIA_ATTR(RATING_ORGANIZATION)
    DEFINE_MEDIA_ATTR(SIZE)
    DEFINE_MEDIA_ATTR(BUFFER_INDEX)
    DEFINE_MEDIA_ATTR(IS_VIDEO_RENDER)
    DEFINE_MEDIA_ATTR(VIDEO_FORCE_RENDER)
    DEFINE_MEDIA_ATTR(MEDIA_TYPE)
    DEFINE_MEDIA_ATTR(AUDIO_CODEC)
    DEFINE_MEDIA_ATTR(AVERAGE_LEVEL)
    DEFINE_MEDIA_ATTR(BLOCK_ALIGN)
    DEFINE_MEDIA_ATTR(PEAK_VALUE)
    DEFINE_MEDIA_ATTR(ALBUM_TITLE)
    DEFINE_MEDIA_ATTR(ALBUM_ARTIST)
    DEFINE_MEDIA_ATTR(CONTRIBUTING_ARTIST)
    DEFINE_MEDIA_ATTR(COMPOSER)
    DEFINE_MEDIA_ATTR(CONDUCTOR)
    DEFINE_MEDIA_ATTR(LYRICS)
    DEFINE_MEDIA_ATTR(MOOD)
    DEFINE_MEDIA_ATTR(TRACK_NUMBER)
    DEFINE_MEDIA_ATTR(TRACK_COUNT)
    DEFINE_MEDIA_ATTR(COVER_ART_URL_SMALL)
    DEFINE_MEDIA_ATTR(COVER_ART_URL_LARGE)
    DEFINE_MEDIA_ATTR(RESOLUTION)
    DEFINE_MEDIA_ATTR(VIDEO_CODEC)
    DEFINE_MEDIA_ATTR(POSTER_URL)
    DEFINE_MEDIA_ATTR(CHAPTER_NUMBER)
    DEFINE_MEDIA_ATTR(DIRECTOR)
    DEFINE_MEDIA_ATTR(LEAD_PERFORMER)
    DEFINE_MEDIA_ATTR(WRITER)
    DEFINE_MEDIA_ATTR(FILE_PATH)
    DEFINE_MEDIA_ATTR(FILE_HANDLE)
    DEFINE_MEDIA_ATTR(OUTPUT_FORMAT)
    DEFINE_MEDIA_ATTR(JPEG_QUALITY)
    DEFINE_MEDIA_ATTR(EXTRADATA0)
    DEFINE_MEDIA_ATTR(EXTRADATA1)
    DEFINE_MEDIA_ATTR(ROTATION)  // in degree
    DEFINE_MEDIA_ATTR(CODEC_DATA)
    DEFINE_MEDIA_ATTR(LOCATION)
    DEFINE_MEDIA_ATTR(LOCATION_LATITUDE)
    DEFINE_MEDIA_ATTR(LOCATION_LONGITUDE)
    DEFINE_MEDIA_ATTR(DATETAKEN)
    DEFINE_MEDIA_ATTR(ORIENTATION)

    // Time Base
    // type: Fraction
    DEFINE_MEDIA_ATTR(TIMEBASE)
    DEFINE_MEDIA_ATTR(TIMEBASE2)

    // seek offset
    // type: int64
    DEFINE_MEDIA_ATTR(SEEK_OFFSET)

    // seek whence
    // type: int32, it is one of the following: SEEK_SET, SEEK_CUR, SEEK_END
    DEFINE_MEDIA_ATTR(SEEK_WHENCE)
    DEFINE_MEDIA_ATTR(PALY_RATE)
    DEFINE_MEDIA_ATTR(VARIABLE_RATE_SUPPORT)
    DEFINE_MEDIA_ATTR(STREAM_IS_SEEKABLE)

    DEFINE_MEDIA_ATTR(START_TIME)
    DEFINE_MEDIA_ATTR(START_DELAY_TIME)

    DEFINE_MEDIA_ATTR(AVC_PROFILE_BASELINE)
    DEFINE_MEDIA_ATTR(AVC_PROFILE_MAIN)
    DEFINE_MEDIA_ATTR(AVC_PROFILE_HIGH)
    DEFINE_MEDIA_ATTR(AVC_PROFILE_CONSTRAINEDBASELINE)
    DEFINE_MEDIA_ATTR(AVC_LEVEL1)
    DEFINE_MEDIA_ATTR(AVC_LEVEL1b)
    DEFINE_MEDIA_ATTR(AVC_LEVEL11)
    DEFINE_MEDIA_ATTR(AVC_LEVEL12)
    DEFINE_MEDIA_ATTR(AVC_LEVEL13)
    DEFINE_MEDIA_ATTR(AVC_LEVEL2)
    DEFINE_MEDIA_ATTR(AVC_LEVEL21)
    DEFINE_MEDIA_ATTR(AVC_LEVEL22)
    DEFINE_MEDIA_ATTR(AVC_LEVEL3)
    DEFINE_MEDIA_ATTR(AVC_LEVEL31)
    DEFINE_MEDIA_ATTR(AVC_LEVEL32)
    DEFINE_MEDIA_ATTR(AVC_LEVEL4)
    DEFINE_MEDIA_ATTR(AVC_LEVEL41)
    DEFINE_MEDIA_ATTR(AVC_LEVEL42)
    DEFINE_MEDIA_ATTR(AVC_LEVEL5)
    DEFINE_MEDIA_ATTR(GOP_SIZE)

    DEFINE_MEDIA_META(BUFFER_TYPE)  // specify MediaBuffer::MediaBufferType in MediaMeta
    //for camera
    DEFINE_MEDIA_ATTR(PHOTO_COUNT)
    DEFINE_MEDIA_ATTR(CAMERA_OBJECT)
    DEFINE_MEDIA_ATTR(RECORDING_PROXY_OBJECT)
    DEFINE_MEDIA_ATTR(TIME_LAPSE_ENABLE)
    DEFINE_MEDIA_ATTR(TIME_LAPSE_FPS)

    //audio device
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_MIC)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_VOICE_UPLINK)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_VOICE_DOWNLINK)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_VOICE_CALL)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_CAMCORDER)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_VOICE_RECOGNITION)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_VOICE_COMMUNICATION)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_REMOTE_SUBMIX)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_FM_TUNER)
    DEFINE_MEDIA_ATTR(AUDIO_SOURCE_CNT)

    DEFINE_MEDIA_ATTR(VIDEO_RAW_DATA)
    DEFINE_MEDIA_ATTR(VIDEO_SURFACE)
    DEFINE_MEDIA_ATTR(VIDEO_SURFACE_TEXTURE)
    DEFINE_MEDIA_ATTR(VIDEO_BQ_PRODUCER)
    DEFINE_MEDIA_ATTR(NATIVE_WINDOW_BUFFER)
    DEFINE_MEDIA_ATTR(NATIVE_DISPLAY)
    DEFINE_MEDIA_ATTR(INPUT_BUFFER_NUM)
    DEFINE_MEDIA_ATTR(INPUT_BUFFER_SIZE)
    DEFINE_MEDIA_ATTR(STORE_META_INPUT)
    DEFINE_MEDIA_ATTR(PREPEND_SPS_PPS)
    DEFINE_MEDIA_ATTR(STORE_META_OUTPUT)
    DEFINE_MEDIA_ATTR(REPEAT_FRAME_DELAY_US)
    DEFINE_MEDIA_ATTR(BITRATE_MODE)
    DEFINE_MEDIA_ATTR(MUSIC_SPECTRUM)
    DEFINE_MEDIA_ATTR(BUFFER_LIST)

    DEFINE_MEDIA_ATTR(CODEC_MEDIA_DECRYPT)
    DEFINE_MEDIA_ATTR(CODEC_RESOURCE_ID)
    DEFINE_MEDIA_ATTR(CODEC_LOW_DELAY)
    DEFINE_MEDIA_ATTR(CODEC_DISABLE_HW_RENDER)
    DEFINE_MEDIA_ATTR(CODEC_DROP_ERROR_FRAME)

    // when one buffer is ahead of other stream than this threshold, muxer will reject it (MM_ERROR_AGAIN). unit is ms
    DEFINE_MEDIA_ATTR(MUXER_STREAM_DRIFT_MAX)

    DEFINE_MEDIA_ATTR(FILE_DOWNLOAD_PATH)
///////////////////////////////////////////////////////////////////
//codecId2Mime
   const char * codecId2Mime(CowCodecID id);
   CowCodecID mime2CodecId(const char *mime);


}// namespace YUNOS_MM

#endif // media_attr_str_h
