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
#include <stdio.h>
#include <string.h>
//#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <deque>
#include "mediacodecPlay.h"
#include "multimedia/mm_debug.h"
#include <multimedia/media_attr_str.h>
#include "multimedia/mm_cpp_utils.h"
#include "multimedia/media_meta.h"
#include "media_surface_texture.h"

#define MEDIACODEEC_DEFER_MESSAGE_TEST 0

MM_LOG_DEFINE_MODULE_NAME("MediaCodecPlay")

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __MM_NATIVE_BUILD__
#include <libavutil/time.h>
#else
#include <libavutil/avtime.h>
#endif
#include <libavutil/opt.h>
#include <libavutil/common.h>
}
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 28, 1)
#define av_frame_alloc avcodec_alloc_frame
  #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 28, 0)
    #define av_frame_free avcodec_free_frame
  #else
    #define av_frame_free av_freep
  #endif
#endif

#define MM_USE_SURFACE_TEXTURE
#include "native_surface_help.h"

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
bool g_use_sub_surface = false;
YUNOS_MM::Lock gNativeBufferLock;
YUNOS_MM::Condition *gBufferCond = NULL;
std::list<int32_t>gBufferList;
int *g_buffer_num = NULL;
void **g_buffers = NULL;
bool g_use_media_texture = false;
bool g_use_surface_v2 = false;
#endif

#include "media_codec.h"

using namespace YunOSMediaCodec;
using namespace YUNOS_MM;

// #define _KEEP_AVCC_DATA_FORMAT  // do not translate avc container format (h264) to byte stream
#ifndef CODEC_RB16
#define CODEC_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
    ((const uint8_t*)(x))[1])
#endif


struct play_info_t {
    AVFormatContext *pFormat;
    AVCodecContext *pVideoCodecCtx;
    int haveVideo;
    double videoClock;//video decode clock
    int64_t videoStreamStartClock; // video stream start time, in us
    int64_t startTimeRealUs;
    int64_t endTimeRealUs;
    int videoStreamIndex;
    bool isInputDone;
    bool isOutputVideoDone;
};

typedef enum {
    VideoRenderOneFrame = 0,
    VideoRenderTryAgain,
    VideoRenderIgnore,
    VideoRenderUnknown,
    VideoRenderEOS = 100,
} VideoRenderResult;

// some command line option parameters
float g_surface_pos_x = 0.0f, g_surface_pos_y = 0.0f, g_surface_alpha = 1.0f;
uint32_t g_surface_width = 1280, g_surface_height = 720;
bool g_full_run_video = false;
int g_exit_after_draw_times = 0;  // try to quit after draw some frames

static WindowSurface *ws = NULL;
static MediaCodec *g_videoDecoder = NULL;
static struct play_info_t g_play_info;
static int renderedFrames = 0;

const char * AVCodecIDToMime(enum AVCodecID codec_id) {
    const char * mime_type = NULL;
    switch (codec_id) {
    case AV_CODEC_ID_MPEG1VIDEO:
        mime_type = "video/mpeg";
        break;
    case AV_CODEC_ID_MPEG2VIDEO:
        mime_type = MEDIA_MIMETYPE_VIDEO_MPEG2;
        break;
    case AV_CODEC_ID_MPEG4:
        mime_type = MEDIA_MIMETYPE_VIDEO_MPEG4;
        break;
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_H263P:
        mime_type =  MEDIA_MIMETYPE_VIDEO_H263;
        break;
    case AV_CODEC_ID_H263I:
        mime_type =  "video/x-intel-h263";
        break;
    case AV_CODEC_ID_H261:
        mime_type =  "video/x-h261";
        break;
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
    case AV_CODEC_ID_RV30:
    case AV_CODEC_ID_RV40:
        mime_type =  "video/x-pn-realvideo";
        break;
    case AV_CODEC_ID_SP5X:
        mime_type =  "video/sp5x";
        break;
    case AV_CODEC_ID_MJPEGB:
        mime_type =  "video/x-mjpeg-b";
        break;
    case AV_CODEC_ID_MSMPEG4V1:
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
        mime_type =  "video/x-msmpeg";
        break;
    case AV_CODEC_ID_WMV1:
    case AV_CODEC_ID_WMV2:
        mime_type =  "video/x-wmv";
        break;
    case AV_CODEC_ID_FLV1:
        mime_type =  "video/x-flash-video";
        break;
    case AV_CODEC_ID_SVQ1:
    case AV_CODEC_ID_SVQ3:
        mime_type =  "video/x-svq";
        break;
    case AV_CODEC_ID_DVVIDEO:
        mime_type =  "video/x-dv";
        break;
    case AV_CODEC_ID_HUFFYUV:
        mime_type =  "video/x-huffyuv";
        break;
    case AV_CODEC_ID_CYUV:
        mime_type =  "video/x-compressed-yuv";
        break;
    case AV_CODEC_ID_H264:
        mime_type =  MEDIA_MIMETYPE_VIDEO_AVC;
        break;
    case AV_CODEC_ID_HEVC:
        mime_type =  MEDIA_MIMETYPE_VIDEO_HEVC;
        break;
    case AV_CODEC_ID_INDEO5:
    case AV_CODEC_ID_INDEO4:
    case AV_CODEC_ID_INDEO3:
    case AV_CODEC_ID_INDEO2:
        mime_type =  "video/x-indeo";
        break;
    case AV_CODEC_ID_FLASHSV:
        mime_type =  "video/x-flash-screen";
        break;
    case AV_CODEC_ID_VP3:
        mime_type =  "video/x-vp3";
        break;
    case AV_CODEC_ID_VP5:
        mime_type =  "video/x-vp5";
        break;
    case AV_CODEC_ID_VP6:
        mime_type =  "video/x-vp6";
        break;
    case AV_CODEC_ID_VP6F:
        mime_type =  "video/x-vp6-flash";
        break;
    case AV_CODEC_ID_VP6A:
        mime_type =  "video/x-vp6-alpha";
        break;
    case AV_CODEC_ID_VP8:
        mime_type =  MEDIA_MIMETYPE_VIDEO_VP8;
        break;
    case AV_CODEC_ID_THEORA:
        mime_type =  "video/x-theora";
        break;
    case AV_CODEC_ID_ASV1:
    case AV_CODEC_ID_ASV2:
        mime_type =  "video/x-asus";
        break;
    case AV_CODEC_ID_FFV1:
        mime_type =  "video/x-ffv";
        break;
    case AV_CODEC_ID_4XM:
        mime_type =  "video/x-4xm";
        break;
    case AV_CODEC_ID_XAN_WC3:
    case AV_CODEC_ID_XAN_WC4:
        mime_type =  "video/x-xan";
        break;
    case AV_CODEC_ID_CLJR:
        mime_type =  "video/x-cirrus-logic-accupak";
        break;
    case AV_CODEC_ID_FRAPS:
        mime_type =  "video/x-fraps";
        break;
    case AV_CODEC_ID_VCR1:
        mime_type =  "video/x-ati-vcr";
        break;
    case AV_CODEC_ID_RPZA:
        mime_type =  "video/x-apple-video";
        break;
    case AV_CODEC_ID_CINEPAK:
        mime_type =  "video/x-cinepak";
        break;
    case AV_CODEC_ID_MSRLE:
    case AV_CODEC_ID_QTRLE:
        mime_type =  "video/x-rle";
        break;
    case AV_CODEC_ID_MSVIDEO1:
        mime_type =  "video/x-msvideocodec";
        break;
    case AV_CODEC_ID_MSS1:
    case AV_CODEC_ID_MSS2:
    case AV_CODEC_ID_WMV3:
    case AV_CODEC_ID_VC1:
        mime_type =  "video/x-wmv";
        break;
    case AV_CODEC_ID_MSZH:
        mime_type =  "video/x-mszh";
        break;
    case AV_CODEC_ID_ZLIB:
        mime_type =  "video/x-zlib";
        break;
    case AV_CODEC_ID_TRUEMOTION1:
    case AV_CODEC_ID_TRUEMOTION2:
        mime_type =  "video/x-truemotion";
        break;
    case AV_CODEC_ID_ULTI:
        mime_type =  "video/x-ultimotion";
        break;
    case AV_CODEC_ID_TSCC:
        mime_type =  "video/x-camtasia";
        break;
    case AV_CODEC_ID_TSCC2:
        mime_type =  "video/x-tscc";
        break;
    case AV_CODEC_ID_KMVC:
        mime_type =  "video/x-kmvc";
        break;
    case AV_CODEC_ID_NUV:
        mime_type =  "video/x-nuv";
        break;
    case AV_CODEC_ID_SMC:
        mime_type =  "video/x-smc";
        break;
    case AV_CODEC_ID_QDRAW:
        mime_type =  "video/x-qdrw";
        break;
    case AV_CODEC_ID_DNXHD:
        mime_type =  "video/x-dnxhd";
        break;
    case AV_CODEC_ID_PRORES:
        mime_type =  "video/x-prores";
        break;
    case AV_CODEC_ID_MIMIC:
        mime_type =  "video/x-mimic";
        break;
    case AV_CODEC_ID_VMNC:
        mime_type =  "video/x-vmnc";
        break;
    case AV_CODEC_ID_AMV:
        mime_type =  "video/x-amv";
        break;
    case AV_CODEC_ID_AASC:
        mime_type =  "video/x-aasc";
        break;
    case AV_CODEC_ID_LOCO:
        mime_type =  "video/x-loco";
        break;
    case AV_CODEC_ID_ZMBV:
        mime_type =  "video/x-zmbv";
        break;
    case AV_CODEC_ID_LAGARITH:
        mime_type =  "video/x-lagarith";
        break;
    case AV_CODEC_ID_CSCD:
        mime_type =  "video/x-camstudio";
        break;
    case AV_CODEC_ID_AIC:
        mime_type =  "video/x-apple-intermediate-codec";
        break;
    case AV_CODEC_ID_RAWVIDEO:
        mime_type =  "video/raw";
        break;
    case AV_CODEC_ID_DTS:
        mime_type = "audio/dtshd";
        break;
    case AV_CODEC_ID_AC3:
        mime_type = "audio/ac3";
        break;
    case AV_CODEC_ID_EAC3:
        mime_type = "audio/eac3";
        break;
    default:
        ERROR ("Unknown codec ID %d, please add mapping here \n", codec_id);
        break;
    }
    return mime_type;
}

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
bool gIsEglInit = false;
WindowSurface *initEgl(int width, int height, int x, int y, int win);
bool drawBuffer(YNativeSurfaceBuffer* anb, int win);
void eglUninit(int win, bool destroyWindow);


class TestTextureListener : public SurfaceTextureListener {
public:
    TestTextureListener() {}
    virtual ~TestTextureListener() {}

    virtual void onMessage(int msg, int param1, int param2) {
        MMAutoLock locker(gNativeBufferLock);
        gBufferList.push_back(param1);
        //PRINTF("notify: buffer index %d, gBufferList size %d\n", param1, gBufferList.size());
        gBufferCond->broadcast();

    }
};
MMSharedPtr < TestTextureListener > mTestTextureListener;

#endif


MediaMetaSP createVideoFormat(char *mime,int width,int height, int64_t duration, MediaMeta::ByteBuffer *csds = NULL ) {
    MediaMetaSP format = MediaMeta::create();

    format->setString(MEDIA_ATTR_MIME, mime);
    format->setInt32(MEDIA_ATTR_WIDTH, width);
    format->setInt32(MEDIA_ATTR_HEIGHT, height);

    //have codec data
    DEBUG("have csds to handle , csd[0].size=%d , csds[1].size=%d\n",csds[0].size,csds[1].size);
    if (csds[0].data && csds[0].size) {
        format->setByteBuffer(MEDIA_ATTR_EXTRADATA0, csds[0].data, csds[0].size);
    }

    if (csds[1].data && csds[1].size) {
        format->setByteBuffer(MEDIA_ATTR_EXTRADATA1, csds[1].data, csds[1].size);
    }
    DEBUG("videoFormat created \n");
    return format;
}

static int getCodecDataFromExtradata(uint8_t* extradata, size_t extradata_size, MediaMeta::ByteBuffer csds[2]) {
    uint8_t *spsStartPos,*ppsStartPos;
    uint8_t *p = extradata;
    if (extradata_size <7) {
        ERROR("avcC too short\n");
        return -1;
    }
    uint8_t tmp[4] = {0,0,0,1};

    // Decode sps from avcC

    /*get  sps startpos need skip 6 bytes:
    8 bit for configurationVersion
    8 bit for AVCProfileIndication
    8 bit for profile_compatibility
    8 bit for AVCLevelIndication
    6 bit for reserved
    2 bit for lengthSizeMinusOne
    3 bit for reserved
    5 bit for numOfSequenceParameterSets
    */
    int spsCnt = *(p + 5) & 0x1f; // Number of sps

    VERBOSE(" Number of sps is %d, ", spsCnt);
    p += 6;

    int spsLength = CODEC_RB16(p);
    int spsNalsize=spsLength+2;
    //skip sequenceParameterSetLength
    spsStartPos = p+2;
    VERBOSE("sps length =%d\n ", spsLength);
    //hexDump(spsStartPos,spsLength, 16);
    //cp csd0
    csds[0].size = 4+spsLength;
    csds[0].data= (uint8_t *)malloc(csds[0].size);
    memcpy(csds[0].data, tmp, 4);
    memcpy(csds[0].data+4, spsStartPos, spsLength);
    hexDump(csds[0].data, csds[0].size, 16);

    //go on handle Extradata
    p += spsNalsize;

    // Decode pps from avcC
    int ppsCnt = *(p++); // Number of pps
    VERBOSE("Number of pps is %d, ", ppsCnt);

    int ppsLength = CODEC_RB16(p);
    //int ppsNalsize = ppsLength + 2;
    ppsStartPos = p+2;
    VERBOSE("pps length =%d \n ",ppsLength);
    //hexDump(ppsStartPos,ppsLength, 16);
    //cp csd1
    csds[1].size = 4+ppsLength;
    csds[1].data= (uint8_t *)malloc(csds[1].size);
    memcpy(csds[1].data, tmp, 4);
    memcpy(csds[1].data+4, ppsStartPos, ppsLength);
    hexDump(csds[1].data, csds[1].size, 16);

    return 1;
}

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
MediaSurfaceTexture *mst;
void initMediaTexture() {
    mst = new MediaSurfaceTexture();
}

#include "SimpleSubWindow.h"
static void* draw_video(void *) {
    if (!gIsEglInit) {
#ifndef __TV_BOARD_MSTAR__
        MMLOGI("init elg for the first time");
        initEgl(500,500,100,100,0);
#endif
        gIsEglInit = true;
    }
    while(1) {
        int index = -1;
        if (g_play_info.isOutputVideoDone) {
            INFO("video output done, retuRn\n");
            break;
        }

        {
            MMAutoLock locker(gNativeBufferLock);
            if (gBufferList.empty()) {
                //PRINTF("before wait\n");
                gBufferCond->wait();
                //PRINTF("after wait\n");
                continue;
            }

            index = *gBufferList.begin();
            gBufferList.pop_front();
        }

        YNativeSurfaceBuffer* anb = mst->acquireBuffer(index);
        if (anb) {
#ifndef __TV_BOARD_MSTAR__
            drawBuffer(anb, 0);
#endif
            int ret = mst->returnBuffer(anb);
            DEBUG("acquireBuffer %p, return buffer %d, ret: %d\n", anb, index, ret);
        }
    }

    if (gIsEglInit) {
#ifndef __TV_BOARD_MSTAR__
        DEBUG("unint egl\n");
        eglUninit(0, true);
        DEBUG("unint egl done\n");
#endif
        gIsEglInit = false;
    }
    return NULL;
}
#endif

static VideoRenderResult render_video_once() {
    VideoRenderResult result = VideoRenderUnknown;
    size_t  offset,size;
    int64_t presentationTimeUs;
    uint32_t flags;
    size_t outIndex = -1;
    int outRet;

    outRet = g_videoDecoder->dequeueOutputBuffer(&outIndex,&offset,&size, &presentationTimeUs,&flags,10000);
    //PRINTF(".");
    MediaMetaSP meta;
    int32_t newWidth =0 , newHeight = 0;
    const char *newMime = NULL;

    switch (outRet) {
    case MediaCodec::MEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
      #ifdef __MM_YUNOS_CNTRHAL_BUILD__
        // FIXME, MediaSurfaceTexture doesn't run into render_video_once. it doesn't handle buffer change for mst in fact
        DEBUG( "INFO_OUTPUT_BUFFERS_CHANGED \n");
        if (g_buffer_num != NULL) {
            *g_buffer_num = g_videoDecoder->getOutputBufferCount();
            for (int i = 0; i < *g_buffer_num; i++) {
                g_buffers[i] = g_videoDecoder->getNativeWindowBuffer((size_t)i);
                if (g_buffers[i] == NULL) {
                    ERROR("cannot get NativeWindowBuffer from MediaCodec");
                    break;
                }
            }
        }
      #endif
        //outputBuffers = decoder.getOutputBuffers();
        result = VideoRenderIgnore;
        break;
    case MediaCodec::MEDIACODEC_INFO_FORMAT_CHANGED:
        meta = g_videoDecoder->getOutputFormat();
        if (!meta) {
            ERROR("no output format");
            break;
        }
        meta->getString(MEDIA_ATTR_MIME, newMime);
        meta->getInt32(MEDIA_ATTR_WIDTH, newWidth);
        meta->getInt32(MEDIA_ATTR_HEIGHT, newHeight);

        DEBUG("New format: newMime=%s, newWidth=%d, newHeight=%d \n",newMime,newWidth,newHeight);
        result = VideoRenderIgnore;
        break;
    case MediaCodec::MEDIACODEC_INFO_TRY_AGAIN_LATER:
        // ERROR( "dequeueOutputBuffer timed out!\n");
        result = VideoRenderTryAgain;
        break;
    case MediaCodec::MEDIACODEC_OK:
        // We use a very simple clock to keep the video FPS, or the video playback will be too fast
        // ERROR("got output video frame with timestamp %" PRId64 "\n",presentationTimeUs);
        if (!g_full_run_video) {
            int64_t delta = 0;
            // when audio stream is not active, use wall clock of av_gettime()
            delta = (presentationTimeUs - g_play_info.videoStreamStartClock)  - (av_gettime() - g_play_info.startTimeRealUs);

            if (delta > 0)
                usleep(delta);
        }

        if (flags == MediaCodec::BUFFER_FLAG_EOS)
            break;

        if (g_videoDecoder->releaseOutputBuffer(outIndex, true) >=0) {
            renderedFrames++;
            result = VideoRenderOneFrame;
        } else {
            result = VideoRenderUnknown;
            WARNING("releaseOutputBuffer failed\n");
        }
        break;
    default:
        WARNING("unknown ret=%d from dequeueOutputBuffer()\n", outRet);
        result = VideoRenderUnknown;
        break;
    }

    // All decoded frames have been rendered, we can stop playing now
    if (flags == MediaCodec::BUFFER_FLAG_EOS) {
        DEBUG("OutputBuffer BUFFER_FLAG_END_OF_STREAM \n");
        result = VideoRenderEOS;
    }

    return result;
}

static void* render_video(void *) {
    while (1) {
        VideoRenderResult result = render_video_once();
        if (result == VideoRenderEOS)
            break;
    }
    g_play_info.isOutputVideoDone = true;
    DEBUG("video render thread exit\n");
    return NULL;
}

#if MEDIACODEEC_DEFER_MESSAGE_TEST
static void* flush_video(void *) {
    DEBUG("flush\n");
    int ret = g_videoDecoder->flush();
    DEBUG("flush, ret %d\n", ret);
    MMASSERT(ret==0);
    return NULL;
}

static void* stop_video(void *) {
    DEBUG("stop\n");
    int ret = g_videoDecoder->stop();
    DEBUG("stop, ret %d\n", ret);
    MMASSERT(ret==0);
    return NULL;
}

static void* start_video(void *) {
    DEBUG("start\n");
    int ret = g_videoDecoder->start();
    DEBUG("start, ret %d\n", ret);
    MMASSERT(ret==0);
    return NULL;
}

#endif

static int _playVideo(const char* input_url, void *surface = NULL, GLuint texture_id = 0, drawFrameFunc drawframe_func = NULL, void* texture_context = NULL, int *buffer_num = NULL, void *buffers[] = NULL) {
    int video_stream_index = -1;
    AVStream *video_st = NULL;
    uint32_t i;
    MediaMeta::ByteBuffer csds[2]= {{0,0},{0,0}};
    int ret = 0;
    uint8_t *extradata = NULL;
    int extradata_size = 0;
    AVPacket pkt;
    bool requireFreePkt = false;
    int inputFrames = 0;
    MediaMetaSP format;
    pthread_t video_render_thread = 0;  // dequeueOutputBuffer/releaseOutputBuffer
    pthread_t draw_video_thread = 0;    // acquireBuffer/returnBuffer
#if MEDIACODEEC_DEFER_MESSAGE_TEST
    pthread_t video_flush_thread = 0;
    pthread_t video_stop_thread = 0;
    pthread_t video_start_thread = 0;
#endif
    //unsigned long startMS = systemTimeMS(SYSTEM_TIME_MONOTONIC);
    //ERROR("startMS =%lu ,decoding \n",startMS);

    DEBUG("\n test inpute url  = %s \n",input_url);
    g_play_info.videoClock=0;
    g_play_info.startTimeRealUs = -1;
    g_play_info.videoStreamStartClock = 0;
    g_play_info.endTimeRealUs = -1;
    g_play_info.pVideoCodecCtx = NULL;
    g_play_info.pFormat = NULL;
    g_play_info.videoStreamIndex = -1;
    g_play_info.isInputDone = false;
    g_play_info.isOutputVideoDone = false;

    DEBUG(" INIT WINDOWS ------\n");
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    setenv("HYBRIS_EGLPLATFORM", MP_HYBRIS_EGLPLATFORM,1);
    if (g_use_sub_surface) {
        ws = createSimpleSurface(480, 320, NST_PagewindowSub);
        setSubWindowFit(ws);
    } else if (g_use_media_texture) {
        initMediaTexture();
        mTestTextureListener.reset(new TestTextureListener());
        mst->setListener(mTestTextureListener.get());

        gBufferCond = new YUNOS_MM::Condition(gNativeBufferLock);
        pthread_create(&draw_video_thread, NULL, (void *(*)(void *))draw_video, NULL);
    } else
#endif
    {
        g_use_surface_v2 = mm_check_env_str(NULL, "USE_BUFFER_QUEUE", "1");
        if (g_use_surface_v2) {
            ws = createSimpleSurface(480, 320, NST_PagewindowMain);
        } else
            ws = createSimpleSurface(176, 144);
    }
    DEBUG(" INIT WINDOWS ++++++\n");

    av_log_set_level(AV_LOG_VERBOSE);
    // libav* init
    av_register_all();
    //enable network for http live streaming play
    avformat_network_init();

    int ret1 = avformat_open_input(&(g_play_info.pFormat), input_url, NULL, NULL);
    // open input
    if (ret1 < 0) {
        ERROR("fail to open input url: %s by avformat, ret %d\n", input_url, ret1);
        return -1;
    }
    AVFormatContext *pFormat = g_play_info.pFormat;
    if (!pFormat) {
        ERROR("pFormat is NULL\n");
        return -1;
    }
    DEBUG("avformat_open_input ok!\n");

    if (avformat_find_stream_info(pFormat, NULL) < 0) {
        ERROR("fail to find out stream info\n");
        if (pFormat) {
            avformat_close_input(&pFormat);
        }
        return -1;
    }
    DEBUG("avformat_find_stream_info ok!\n");

    av_dump_format(pFormat,0,input_url,0);

    // find out video / audio stream
    for (i=0; i<pFormat->nb_streams; i++) {
        if (pFormat->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
            video_stream_index=i;
            g_play_info.haveVideo = 1;
            g_play_info.videoStreamIndex = video_stream_index;
            break;
        }
    }

    if (!g_play_info.haveVideo) {
        ERROR("not find video stream!\n");//we mainly test video play
        if (pFormat) {
            avformat_close_input(&pFormat);
        }
        return -1;
    }
    //create video decoder with yunos mediacodec decoder
    g_play_info.pVideoCodecCtx=pFormat->streams[video_stream_index]->codec;
    MMASSERT(g_play_info.pVideoCodecCtx && video_stream_index>=0);

    const int width = g_play_info.pVideoCodecCtx->width;
    const int height = g_play_info.pVideoCodecCtx->height;
    const int64_t duration = pFormat->duration;
    const char * mime_type = AVCodecIDToMime(g_play_info.pVideoCodecCtx->codec_id);

    void *bufferProducer = (void *)ws;
    uint32_t codecFlags = 0;

    g_videoDecoder = MediaCodec::CreateByType(mime_type ,false);
    if (g_videoDecoder ==NULL) {
        ERROR("fail to create video decoder!\n");
        ret = -1;
        goto finish;
    }
    INFO("mime_type=%s , width= %d ,height= %d, codec_id=%d ,video_stream_index =%d\n",mime_type, width, height,g_play_info.pVideoCodecCtx->codec_id,video_stream_index );
    extradata =g_play_info.pVideoCodecCtx->extradata;
    extradata_size = g_play_info.pVideoCodecCtx->extradata_size;
    //ERROR("extradata ptr=%p , extradata_size= %d \n", extradata, extradata_size);
    //hexDump(extradata ,extradata_size, 16);

    if (g_play_info.pVideoCodecCtx->codec_id == AV_CODEC_ID_H264) {
        VERBOSE("decode sps/pps from extradata \n");
#ifdef _KEEP_AVCC_DATA_FORMAT
        csds[0].data = extradata;
        csds[0].size = extradata_size;
        csds[1].data = NULL;
        csds[1].size = 0;
#else
        int ret =getCodecDataFromExtradata(extradata, extradata_size, csds);
        if (ret <0) {
            ERROR("AVC getCodecDataFromExtradata fail \n");
            ret = -1;
            goto finish;
        }
#endif
    }
    format=createVideoFormat((char *)mime_type,width,height ,duration,csds);

#if defined(__MM_YUNOS_CNTRHAL_BUILD__)
    if (g_use_media_texture) {
        INFO("use media texture");
        codecFlags |= MediaCodec::CONFIGURE_FLAG_SURFACE_TEXTURE;
        bufferProducer = mst;
    }
#endif

    DEBUG("configure: bufferProducer: %p", bufferProducer);
#ifndef __TV_BOARD_MSTAR__
    if (g_videoDecoder->configure(format, (WindowSurface*)bufferProducer, codecFlags) < 0) {
#else
    if (g_videoDecoder->configure(format, NULL, codecFlags) < 0) {
#endif
        ERROR("fail to configure decoder!\n");
        ret = -1;
        goto finish;
    }

    video_st = g_play_info.pFormat->streams[g_play_info.videoStreamIndex];


#if !MEDIACODEEC_DEFER_MESSAGE_TEST
    if (g_videoDecoder->start()< 0) {
        ERROR("Can't start video decoder!\n");
        ret =-1;
        goto finish;
    }

    // test api:
    {
        int inBufCount = g_videoDecoder->getInputBufferCount();
        int outBufCount = g_videoDecoder->getOutputBufferCount();
        const char *decoderName = g_videoDecoder->getName();
        DEBUG("inBufCount = %d,outBufCount =%d,decoderName=%s",inBufCount,outBufCount,decoderName);
    }

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    if (buffer_num != NULL) {
        g_buffer_num = buffer_num;
        g_buffers = buffers;
        *buffer_num = g_videoDecoder->getOutputBufferCount();
        for (int i = 0; i < *buffer_num; i++) {
            buffers[i] = g_videoDecoder->getNativeWindowBuffer((size_t)i);
            if (buffers[i] == NULL) {
                ERROR("cannot get NativeWindowBuffer from MediaCodec");
                goto finish;
            }
        }
    }
#endif

    g_play_info.startTimeRealUs = av_gettime();
    DEBUG("startTimeRealUs =%" PRId64 " ,decoding \n", g_play_info.startTimeRealUs);

    while (1) {
        bool readEOS = false;
        int mediaCodecRet;
        size_t inputBufferIndex = -1;

        if (requireFreePkt)
            av_free_packet(&pkt);

        // 1. read one video pkt
        av_init_packet(&pkt);
        if (av_read_frame(pFormat, &pkt) < 0) { // read to the end of stream
            readEOS = true;
        } else  {
            requireFreePkt = true;
            if (g_exit_after_draw_times && inputFrames >= g_exit_after_draw_times) { // early quit after render the given number of frames
                readEOS = true;
            } else if (pkt.stream_index != video_stream_index) {
                continue;
            }
        }

        // 2. deque one input buffer
        while(1) {
            mediaCodecRet = g_videoDecoder->dequeueInputBuffer(&inputBufferIndex, 10000);
            if (mediaCodecRet == MediaCodec::MEDIACODEC_INFO_TRY_AGAIN_LATER)
                usleep(5000);
            else
                break;
        };
        if (mediaCodecRet < 0 || inputBufferIndex < 0) {
            ERROR("deque input buffer fail");
            break;
        }

        // 3. prepare input buffer
        size_t offset = 0;
        size_t size = 0;
        int64_t videopts = 0;
        uint32_t flags = 0;
        if (!readEOS) {
            size_t inputBufferCapacity =0;
            uint8_t *in_buffer = g_videoDecoder->getInputBuffer(inputBufferIndex, &inputBufferCapacity);
            if (in_buffer == NULL) {
                ERROR( "get InputBuffer fail\n");
                break;
            }
            if (inputBufferCapacity < (uint32_t)pkt.size) {
                ERROR( "InputBuffer size insufficient: insize=%zu ,pkt->size =%d\n", inputBufferCapacity, pkt.size);
                break;
            }
            // replace nal size with start code
            uint8_t *ptr = pkt.data;
            #ifndef _KEEP_AVCC_DATA_FORMAT
            if ((g_play_info.pVideoCodecCtx->codec_id == AV_CODEC_ID_H264)
                    &&(!(*ptr==0 && *(ptr+1)==0 && *(ptr+2) == 0 && *(ptr+3) == 1))) {
                do {
                    // assume nal size field is 4 bytes
                    int nal_size = (*(ptr) <<24) + (*(ptr+1)<<16) + (*(ptr+2)<<8) + (*(ptr+3));
                    // replace with start code
                    *ptr= *(ptr+1) = *(ptr+2) = 0;
                    *(ptr+3) = 1;
                    ptr += nal_size + 4;
                    //ERROR("nal_size=%d, ptr=%p, pkt.data=%p\n", nal_size, ptr, pkt.data);
                } while (ptr < pkt.data + pkt.size);
            }
            #endif
            memcpy(in_buffer, pkt.data, pkt.size);

            //convert video presentationTimeUs
            videopts = pkt.pts * 1000000ll * video_st->time_base.num/video_st->time_base.den;
            DEBUG("video pkt.pts = %" PRId64 " ,video_st->time_base.num =%d, video_st->time_base.den = %d, videopts = %" PRId64 " \n",
                pkt.pts, video_st->time_base.num, video_st->time_base.den, videopts);
            size = pkt.size;
            if (!g_play_info.videoStreamStartClock) {
                g_play_info.videoStreamStartClock = videopts;
            }
        }else {
            flags = MediaCodec::BUFFER_FLAG_EOS;
        }

        // 4. queue input buffer
        while(1) {
            mediaCodecRet = g_videoDecoder->queueInputBuffer(inputBufferIndex, offset, size, videopts, flags);
            if (mediaCodecRet == MediaCodec::MEDIACODEC_INFO_TRY_AGAIN_LATER)
                usleep(5000);
            else
                break;
        }
        if (mediaCodecRet <0) {
            ERROR("queue input buffer fail");
            break;
        }

        inputFrames++;
        if (readEOS) {
            break;
        }

        if (!video_render_thread) {
            pthread_create(&video_render_thread, NULL, (void *(*)(void *))render_video, NULL);
        }

    }

    if (requireFreePkt)
        av_free_packet(&pkt);

    g_play_info.isInputDone = true;
    while ((g_play_info.haveVideo && video_render_thread && !g_play_info.isOutputVideoDone)) {
        usleep(5000);
    }

#if defined(__MM_YUNOS_CNTRHAL_BUILD__)
    if (gBufferCond) {
        gBufferCond->broadcast();
    }
#endif

    g_play_info.endTimeRealUs = av_gettime();
    DEBUG("endTimeRealUs =%" PRId64 " ,closing \n", g_play_info.endTimeRealUs);
    DEBUG("total input frame count inputFrames = %d, render frame count renderedFrames = %d, fps=%.2f\n",
          inputFrames, renderedFrames, renderedFrames*1000000.0/(g_play_info.endTimeRealUs - g_play_info.startTimeRealUs));

#else
    if (!video_start_thread) {
        pthread_create(&video_start_thread, NULL, (void *(*)(void *))start_video, NULL);
    }

#endif

finish:

    if (g_videoDecoder !=NULL) {
#if !MEDIACODEEC_DEFER_MESSAGE_TEST
        DEBUG("flush decoder\n");
        if (g_videoDecoder->flush() < 0) {
            ERROR("video decoder flush fail \n");
            ret =-1;
        }
        DEBUG("stop decoder\n");
        if (g_videoDecoder->stop() < 0) {
            ERROR("video decoder stop fail \n");
            ret =-1;
        }
        DEBUG("release decoder\n");
        if (g_videoDecoder->release() < 0) {
            ERROR("video decoder release fail \n");
            ret =-1;
        }

#else
        if (!video_flush_thread) {
            pthread_create(&video_flush_thread, NULL, (void *(*)(void *))flush_video, NULL);
        }

        if (!video_stop_thread) {
            pthread_create(&video_stop_thread, NULL, (void *(*)(void *))stop_video, NULL);
        }

        usleep(2000000);
        DEBUG("release decoder\n");
        ret = g_videoDecoder->release();
        MMASSERT(ret==0);
        ret = -1;
#endif

    }

    usleep(1000000);
    for (int i =0; i< 2; i++) {
        if (csds[i].data != NULL) {
            free(csds[i].data);
            csds[i].size=0;
        }
    }


    if (g_play_info.haveVideo) {
        if (g_play_info.pVideoCodecCtx) {
            avcodec_close(g_play_info.pVideoCodecCtx);
        }
    }

    if (pFormat) {
        avformat_close_input(&pFormat);
    }


#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    if (g_use_media_texture) {
        DEBUG("delete mst\n");
        delete mst;
        DEBUG("delete mst done\n");
    }

    destroySimpleSurface(ws);
    ws = NULL;
#ifndef __MM_NATIVE_BUILD__
    cleanupSurfaces();
    DEBUG("delete page window done\n");
#endif

    delete gBufferCond;
#endif

    DEBUG("play to the end\n");
    return ret;
}

int playVideo(const char* input_url, void *surface, int id) {
    _playVideo (input_url, surface);
    return 0;
}

int playVideoTexture(const char* input_url, GLuint texture_id, drawFrameFunc draw_frame, void* data, int *buffer_num, void *buffers[]) {
    _playVideo(input_url, NULL, texture_id, draw_frame, data, buffer_num, buffers);
    return 0;
}

