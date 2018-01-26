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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "multimedia/audio-decode.h"


#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif

#include "multimedia/mm_debug.h"
#include "multimedia/mm_audio_compat.h"
MM_LOG_DEFINE_MODULE_NAME("AudioDecode");

//#define DUMP_PCM_DATA

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avtime.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
}
#endif

#define DEFAULT_SAMPLE_RATE 44100
#define RESAMPLE_AUDIO_SIZE 8192 * 4

static snd_format_t format_convert(int format) {
    snd_format_t format_to;
    switch (AVSampleFormat(format)) {
    case AV_SAMPLE_FMT_U8:
        format_to = SND_FORMAT_PCM_8_BIT;
        break;
    case AV_SAMPLE_FMT_S16:
        format_to = SND_FORMAT_PCM_16_BIT;
        break;
    case AV_SAMPLE_FMT_S32:
        format_to = SND_FORMAT_PCM_32_BIT;
        break;
    case AV_SAMPLE_FMT_S16P:
        format_to = SND_FORMAT_PCM_16_BIT;
        break;
    case AV_SAMPLE_FMT_FLTP:
        format_to = SND_FORMAT_PCM_32_BIT;
        break;
    default:
        format_to = SND_FORMAT_PCM_16_BIT;
        break;
    }
    return format_to;
}
static int mSampleRate = 0;
bool get_output_samplerate()
{
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    YunOSAudioNS::AudioManager* am = YunOSAudioNS::AudioManager::create();
    if (!am) {
        MMLOGE("Fail to create Audio Manager");
        return false;
    } else {
        int ret = 0;
        ret = am->getOutputSampleRate(&mSampleRate);
        MMLOGI("mSampleRate : %d", mSampleRate);
        if(am != NULL) {
            delete am;
            am = NULL;
        }
        if (ret) {
            MMLOGE("get output samplerate error");
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

int audio_decode(const char* url, audio_sample_spec* pcm_spec, uint8_t* buf, size_t* size) {
    if (!get_output_samplerate()) {
        mSampleRate = DEFAULT_SAMPLE_RATE;
    }
    av_register_all();
    AVFormatContext* pInFmtCtx = avformat_alloc_context();
    if (!pInFmtCtx) {
        MMLOGE("avfomat alloc fail\n");
        return -1;
    }
    AVCodecContext* pInCodecCtx = NULL;
    struct SwrContext* mAVResample = NULL;
    int64_t wanted_channel_layout = 0;
    int audioStream = -1;
    AVPacket* packet = NULL;
#ifdef DUMP_PCM_DATA
    FILE* pcm = NULL;
#endif
    int buffersize = 0;
    long start = 0;
    AVFrame* pAudioFrame = NULL;
    // open input
    if (avformat_open_input(&pInFmtCtx, url, NULL, NULL) < 0) {
        MMLOGE("fail to open input url: %s by avformat\n", url);
        goto fail;
    }

    if (avformat_find_stream_info(pInFmtCtx, NULL) < 0) {
        MMLOGE("av_find_stream_info error\n");
        goto fail;
    }

    {
        unsigned int j;
        // Find the first audio stream
        for (j = 0; j < pInFmtCtx->nb_streams; j++) {
            if (pInFmtCtx->streams[j]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioStream = j;
                break;
            }
        }
        MMLOGV("audioStream = %d\n", audioStream);

        if (audioStream == -1) {
            MMLOGW("input file has no audio stream\n");
            goto fail; // Didn't find a audio stream
        }
        MMLOGV("audio stream num: %d\n", audioStream);
        pInCodecCtx = pInFmtCtx->streams[audioStream]->codec;
        if (!pInCodecCtx) {
            MMLOGE("no codec context\n");
            goto fail;
        }
        AVCodec* pInCodec = NULL;



        pInCodec = avcodec_find_decoder(pInCodecCtx->codec_id);
        if (pInCodec == NULL) {
            MMLOGE("error no Codec found\n");
            goto fail; // Codec not found
        }

        { // init resampler
            mAVResample = swr_alloc();
            wanted_channel_layout =
                (pInCodecCtx->channel_layout &&
                pInCodecCtx->channels ==
                av_get_channel_layout_nb_channels(pInCodecCtx->channel_layout)) ? pInCodecCtx->channel_layout : av_get_default_channel_layout(pInCodecCtx->channels);
            wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;

            av_opt_set_int(mAVResample, "in_channel_layout", wanted_channel_layout, 0);
            av_opt_set_int(mAVResample, "in_sample_fmt", pInCodecCtx->sample_fmt, 0);
            av_opt_set_int(mAVResample, "in_sample_rate", pInCodecCtx->sample_rate, 0);
            av_opt_set_int(mAVResample, "out_channel_layout", wanted_channel_layout, 0);
            av_opt_set_int(mAVResample, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
            av_opt_set_int(mAVResample, "out_sample_rate", mSampleRate, 0);
            if (swr_init(mAVResample) < 0) {
                MMLOGE("error initializing libswresample\n");
                goto fail;
            }
            pcm_spec->format = format_convert(AV_SAMPLE_FMT_S16);
            pcm_spec->rate = mSampleRate;
            pcm_spec->channels = pInCodecCtx->channels;
        }
        if (avcodec_open2(pInCodecCtx, pInCodec, NULL) < 0) {
            MMLOGE("error avcodec_open failed.\n");
            goto fail; // Could not open codec
        }

        uint8_t* pktdata;
        int pktsize;
        packet = (AVPacket*)malloc(sizeof(AVPacket));
        if (!packet) {
            MMLOGE("error packet malloc failed.\n");
            goto fail;
        }
        av_init_packet(packet);

#ifdef DUMP_PCM_DATA
        pcm = fopen("/data/result.pcm", "wb");
#endif
        start = clock();
        int got_frame = 0;
        pAudioFrame = av_frame_alloc();
        uint8_t* buffer = buf;
        uint8_t* decodebuffer = NULL;
        int bufferSize = 0;
        int decodedSize = 0;

        while (av_read_frame(pInFmtCtx, packet) >= 0) {
            if (packet->stream_index == audioStream) {
                pktdata = packet->data;
                pktsize = packet->size;
                while (pktsize > 0) {

                    int len = avcodec_decode_audio4(pInCodecCtx, pAudioFrame, &got_frame, packet);
                    if (len < 0) {
                        MMLOGE("Error while decoding.\n");
                        break;
                    }
                    if (got_frame > 0) {

                        bufferSize = RESAMPLE_AUDIO_SIZE;
                        decodebuffer = (uint8_t*)malloc(bufferSize);
                        decodedSize = swr_convert(mAVResample, &decodebuffer,
                            bufferSize/pInCodecCtx->channels/av_get_bytes_per_sample(AV_SAMPLE_FMT_S16),
                            pAudioFrame->data,
                            pAudioFrame->nb_samples);
                        decodedSize = decodedSize * pInCodecCtx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                        if (*size > (size_t)decodedSize) {
                            memcpy(buffer, decodebuffer, decodedSize);
                            buffer += decodedSize;
                            *size -= decodedSize;
                            buffersize += decodedSize;
#ifdef DUMP_PCM_DATA
                            fwrite(decodebuffer, 1, decodedSize, pcm);
#endif
                        } else {
                            memcpy(buffer, decodebuffer, *size);
#ifdef DUMP_PCM_DATA
                            fwrite(decodebuffer, 1, decodedSize, pcm);
#endif
                            buffersize += *size;
                            free(decodebuffer);
                            decodebuffer = NULL;
                            goto done;
                        }

                        free(decodebuffer);
                        decodebuffer = NULL;
                        pktsize -= len;
                        pktdata += len;
                    }
                }
            }
            av_free_packet(packet);
        }
    }
done : {
#ifdef DUMP_PCM_DATA
    fclose(pcm);
#endif
    if (packet != NULL) {
        av_free_packet(packet);
        free(packet);
        packet = NULL;
    }
    long end = clock();
    *size = buffersize;
    MMLOGI("cost time :%f\n", double(end - start) / (double)CLOCKS_PER_SEC);
    av_free(pAudioFrame);
    swr_free(&mAVResample);
    avcodec_close(pInCodecCtx);
    pInCodecCtx = NULL;
    avformat_close_input(&pInFmtCtx);
    return 0;
}
fail:
    if (mAVResample) {
        swr_free(&mAVResample);
    }

    if (pInCodecCtx) {
        avcodec_close(pInCodecCtx);
    }
    if (pInFmtCtx) {
        avformat_close_input(&pInFmtCtx);
    }
    return -1;
}

int audio_decode_f(int fd, int64_t offset, int64_t length, audio_sample_spec* pcm_spec, uint8_t* buf, size_t* size) {
    if (!get_output_samplerate()) {
        mSampleRate = DEFAULT_SAMPLE_RATE;
    }
    bool check_range = false;
    if (offset >= 0 && length > 0) {
        check_range = true;
        // check param validation
        struct stat sb;
        int ret = fstat(fd, &sb);
        if (ret != 0) {
            MMLOGE("fstat(%d) failed: %d, %s", fd, ret, strerror(errno));
            return -1;
        }
        MMLOGV("st_dev  = %llu, st_mode = %u, st_uid  = %lu, st_gid  = %lu, st_size = %llu, st_size = %llu", sb.st_dev, sb.st_mode, sb.st_uid, sb.st_gid, sb.st_size);

        if (offset >= sb.st_size) {
            MMLOGE("offset error");
            return -1;
        }
        if (offset + length > sb.st_size) {
            length = sb.st_size - offset;
            MMLOGI("calculated length = %" PRId64 "", length);
        }
    }

    int64_t pos = 0, decoded_len = 0;
#define F_MAX_LEN 1024
    char temp[F_MAX_LEN] = {'\0'};
    char url[F_MAX_LEN] = {'\0'};
    snprintf(temp, sizeof(temp), "/proc/self/fd/%d", fd);
    if (readlink(temp, url, sizeof(url) - 1) < 0) {
        MMLOGE("get file path fail \n");
    }

    MMLOGD("url = %s , offset = %" PRId64 " , length = %" PRId64 " ,check_range = %d\n",url, offset, length,check_range);

    av_register_all();
    AVFormatContext* pInFmtCtx = avformat_alloc_context();
    AVCodecContext* pInCodecCtx = NULL;
    AVPacket* packet = NULL;
    struct SwrContext* mAVResample = NULL;
    int64_t wanted_channel_layout = 0;
    long start = 0;
#ifdef DUMP_PCM_DATA
    FILE* pcm = NULL;
#endif
    AVFrame* pAudioFrame = NULL;
    int buffersize = 0;
    int audioStream = -1;
    if (!pInFmtCtx) {
        MMLOGE("avfomat alloc fail\n");
        goto fail;
    }
    // open input
    if (avformat_open_input(&pInFmtCtx, url, NULL, NULL) < 0) {
        MMLOGE("fail to open input url: %s by avformat\n", url);
        goto fail;
    }

    if (avformat_find_stream_info(pInFmtCtx, NULL) < 0) {
        MMLOGE("av_find_stream_info error\n");
        goto fail;
    }

    {
        unsigned int j;
        // Find the first audio stream
        for (j = 0; j < pInFmtCtx->nb_streams; j++) {
            if (pInFmtCtx->streams[j]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioStream = j;
                break;
            }
        }
        MMLOGV("audioStream = %d\n", audioStream);

        if (audioStream == -1) {
            MMLOGW("input file has no audio stream\n");
            goto fail; // Didn't find a audio stream
        }
        printf("audio stream num: %d\n", audioStream);
        pInCodecCtx = pInFmtCtx->streams[audioStream]->codec;
        if (!pInCodecCtx) {
            MMLOGE("no codec context\n");
            goto fail;
        }
        AVCodec* pInCodec = NULL;

        pInCodec = avcodec_find_decoder(pInCodecCtx->codec_id);
        if (pInCodec == NULL) {
            MMLOGE("error no Codec found\n");
            goto fail; // Codec not found
        }
        { // init resampler
            mAVResample = swr_alloc();
            wanted_channel_layout =
                (pInCodecCtx->channel_layout &&
                pInCodecCtx->channels ==
                av_get_channel_layout_nb_channels(pInCodecCtx->channel_layout)) ? pInCodecCtx->channel_layout : av_get_default_channel_layout(pInCodecCtx->channels);
            wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;

            av_opt_set_int(mAVResample, "in_channel_layout", wanted_channel_layout, 0);
            av_opt_set_int(mAVResample, "in_sample_fmt", pInCodecCtx->sample_fmt, 0);
            av_opt_set_int(mAVResample, "in_sample_rate", pInCodecCtx->sample_rate, 0);
            av_opt_set_int(mAVResample, "out_channel_layout", wanted_channel_layout, 0);
            av_opt_set_int(mAVResample, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
            av_opt_set_int(mAVResample, "out_sample_rate", mSampleRate, 0);
            if ( swr_init(mAVResample) < 0) {
                MMLOGE("error initializing libswresample\n");
                goto fail;
            }
            pcm_spec->format = format_convert(AV_SAMPLE_FMT_S16);
            pcm_spec->rate = mSampleRate;
            pcm_spec->channels = pInCodecCtx->channels;
        }

        if (avcodec_open2(pInCodecCtx, pInCodec, NULL) < 0) {
            MMLOGE("error avcodec_open failed.\n");
            goto fail; // Could not open codec
        }

        uint8_t* pktdata;
        int pktsize;
        packet = (AVPacket*)malloc(sizeof(AVPacket));
        if (!packet) {
            MMLOGE("error packet malloc failed.\n");
            goto fail;
        }
        av_init_packet(packet);

#ifdef DUMP_PCM_DATA
        pcm = fopen("/data/result1.pcm", "wb");
#endif
        start = clock();
        int got_frame = 0;
        pAudioFrame = av_frame_alloc();
        uint8_t* buffer = buf;
        uint8_t* decodebuffer = NULL;
        int bufferSize = 0;
        int decodedSize = 0;

        while (av_read_frame(pInFmtCtx, packet) >= 0) {
            if (packet->stream_index == audioStream) {
                pktdata = packet->data;
                pktsize = packet->size;
                if (check_range) {
                    pos += packet->size;
                    // MMLOGV(" read pos = %" PRId64 " \n", pos);
                    if (pos < offset) {
                        MMLOGD("now pos = %" PRId64 "  < offset = %" PRId64 " , skip it \n", pos ,offset);
                        continue;
                    }
                    if (decoded_len >= length) {
                        MMLOGD("now decoded_len  =%" PRId64 " >= length =%" PRId64 " , decode end\n", decoded_len , length);
                        goto done;
                    }
                }
                while (pktsize > 0) {

                    int len = avcodec_decode_audio4(pInCodecCtx, pAudioFrame, &got_frame, packet);
                    if (len < 0) {
                        MMLOGE("Error while decoding.\n");
                        break;
                    }
                    if (got_frame > 0) {
                        bufferSize = RESAMPLE_AUDIO_SIZE;
                        decodebuffer = (uint8_t*)malloc(bufferSize);
                        decodedSize = swr_convert(mAVResample, &decodebuffer,
                            bufferSize/pInCodecCtx->channels/av_get_bytes_per_sample(AV_SAMPLE_FMT_S16),
                            pAudioFrame->data,
                            pAudioFrame->nb_samples);
                        decodedSize = decodedSize * pInCodecCtx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                        if (*size > (size_t)decodedSize) {
                            memcpy(buffer, decodebuffer, decodedSize);
                            buffer += decodedSize;
                            *size -= decodedSize;
                            buffersize += decodedSize;
#ifdef DUMP_PCM_DATA
                            fwrite(decodebuffer, 1, decodedSize, pcm);
#endif
                        } else {
                            memcpy(buffer, decodebuffer, *size);
#ifdef DUMP_PCM_DATA
                            fwrite(decodebuffer, 1, decodedSize, pcm);
#endif
                            buffersize += *size;
                            free(decodebuffer);
                            decodebuffer = NULL;
                            goto done;
                        }

                        free(decodebuffer);
                        decodebuffer = NULL;
                        pktsize -= len;
                        pktdata += len;
                    }
                }
                if (check_range) {
                    decoded_len += packet->size;
                    // MMLOGV("decoded_len = %" PRId64 " \n", decoded_len);
                }
            }
            av_free_packet(packet);
        }
    }
done : {
#ifdef DUMP_PCM_DATA
    fclose(pcm);
#endif
    if (packet != NULL) {
        av_free_packet(packet);
        free(packet);
        packet = NULL;
    }
    long end = clock();
    *size = buffersize;
    MMLOGI("cost time :%f\n", double(end - start) / (double)CLOCKS_PER_SEC);
    av_free(pAudioFrame);
    swr_free(&mAVResample);
    avcodec_close(pInCodecCtx);
    pInCodecCtx = NULL;
    avformat_close_input(&pInFmtCtx);
    return 0;
}
fail:
    if (mAVResample) {
        swr_free(&mAVResample);
    }
    if (pInCodecCtx) {
        avcodec_close(pInCodecCtx);
    }
    if (pInFmtCtx) {
        avformat_close_input(&pInFmtCtx);
    }
    return -1;
}

static uint8_t* mInBuffer = NULL;
static int mInBufferSize = 0;
#define IOSIZE 32768
int read_buffer(void* opaque, uint8_t* buf, int bufsize) {
    if (mInBuffer) {
        int read_size = bufsize < mInBufferSize ? bufsize : mInBufferSize;
        memcpy(buf, mInBuffer, read_size);
        mInBuffer += read_size;
        mInBufferSize -= read_size;
        return read_size;
    } else {
        return -1;
    }
}

int audio_decode_b(uint8_t* in_buf, int buf_size, audio_sample_spec* pcm_spec, uint8_t* out_buf, size_t* size) {
    if (!get_output_samplerate()) {
        mSampleRate = DEFAULT_SAMPLE_RATE;
    }
    mInBuffer = (uint8_t*)malloc(buf_size);
    memcpy(mInBuffer, in_buf, buf_size);
    mInBufferSize = buf_size;
    uint8_t* inBufferFree = mInBuffer;

    av_register_all();
    AVFormatContext* pInFmtCtx = avformat_alloc_context();
    AVCodecContext* pInCodecCtx = NULL;
    AVIOContext* pIOContext = NULL;
    struct SwrContext* mAVResample = NULL;
    int64_t wanted_channel_layout = 0;
    unsigned char* iobuffer = (unsigned char*)av_malloc(IOSIZE);
#ifdef DUMP_PCM_DATA
    FILE* pcm = NULL;
#endif
    long start = clock();
    AVFrame* pAudioFrame = NULL;
    AVPacket* packet = NULL;
    int buffersize = 0;
    if (!pInFmtCtx) {
        MMLOGE("avformat alloc failed!\n");
        goto fail;
    }
    pIOContext = avio_alloc_context(iobuffer, IOSIZE, 0, NULL, read_buffer, NULL, NULL);
    if (!pIOContext) {
        MMLOGE("avio alloc failed!\n");
        goto fail;
    }

    pInFmtCtx->pb = pIOContext;

    if (avformat_open_input(&pInFmtCtx, NULL, NULL, NULL) < 0) {
        MMLOGE("avformat open failed.\n");
        goto fail;
    } else {
        MMLOGV("open stream success!\n");
    }


    if (avformat_find_stream_info(pInFmtCtx, NULL) < 0) {
        MMLOGE("av_find_stream_info error\n");
        if (pInFmtCtx) {
            avformat_close_input(&pInFmtCtx);
        }
        goto fail;
    }

    {
        unsigned int j;
        // Find the first audio stream
        int audioStream = -1;
        for (j = 0; j < pInFmtCtx->nb_streams; j++) {
            if (pInFmtCtx->streams[j]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioStream = j;
                break;
            }
        }
        MMLOGV("audioStream = %d\n", audioStream);

        if (audioStream == -1) {
            MMLOGW("input file has no audio stream\n");
            goto fail; // Didn't find a audio stream
        }
        MMLOGV("audio stream num: %d\n", audioStream);
        pInCodecCtx = pInFmtCtx->streams[audioStream]->codec;
        if (!pInCodecCtx) {
            MMLOGE("no codec context\n");
            goto fail;
        }
        AVCodec* pInCodec = NULL;

        pInCodec = avcodec_find_decoder(pInCodecCtx->codec_id);
        if (pInCodec == NULL) {
            MMLOGE("error no Codec found\n");
            goto fail; // Codec not found
        }
        { //init resampler
            mAVResample = swr_alloc();
            wanted_channel_layout =
                (pInCodecCtx->channel_layout &&
                pInCodecCtx->channels ==
                av_get_channel_layout_nb_channels(pInCodecCtx->channel_layout)) ? pInCodecCtx->channel_layout : av_get_default_channel_layout(pInCodecCtx->channels);
            wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;

            av_opt_set_int(mAVResample, "in_channel_layout", wanted_channel_layout, 0);
            av_opt_set_int(mAVResample, "in_sample_fmt", pInCodecCtx->sample_fmt, 0);
            av_opt_set_int(mAVResample, "in_sample_rate", pInCodecCtx->sample_rate, 0);
            av_opt_set_int(mAVResample, "out_channel_layout", wanted_channel_layout, 0);
            av_opt_set_int(mAVResample, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
            av_opt_set_int(mAVResample, "out_sample_rate", mSampleRate, 0);
            if (swr_init(mAVResample) < 0) {
                MMLOGE("error initializing libswresample\n");
                goto fail;
            }
            pcm_spec->format = format_convert(AV_SAMPLE_FMT_S16);
            pcm_spec->rate = mSampleRate;
            pcm_spec->channels = pInCodecCtx->channels;
        }

        if (avcodec_open2(pInCodecCtx, pInCodec, NULL) < 0) {
            MMLOGE("error avcodec_open failed.\n");
            goto fail; // Could not open codec
        }

        uint8_t* pktdata;
        int pktsize;
        packet = (AVPacket*)malloc(sizeof(AVPacket));
        if (!packet) {
            MMLOGE("error packet malloc failed.\n");
            goto fail;
        }
        av_init_packet(packet);

#ifdef DUMP_PCM_DATA
        pcm = fopen("/data/result2.pcm", "wb");
#endif
        start = clock();
        int got_frame = 0;
        pAudioFrame = av_frame_alloc();
        uint8_t* buffer = out_buf;
        uint8_t* decodebuffer = NULL;
        int bufferSize = 0;
        int decodedSize = 0;

        while (av_read_frame(pInFmtCtx, packet) >= 0) {
            if (packet->stream_index == audioStream) {
                pktdata = packet->data;
                pktsize = packet->size;
                while (pktsize > 0) {

                    int len = avcodec_decode_audio4(pInCodecCtx, pAudioFrame, &got_frame, packet);
                    if (len < 0) {
                        MMLOGE("Error while decoding.\n");
                        break;
                    }
                    if (got_frame > 0) {
                        bufferSize = RESAMPLE_AUDIO_SIZE;
                        decodebuffer = (uint8_t*)malloc(bufferSize);
                        decodedSize = swr_convert(mAVResample, &decodebuffer,
                            bufferSize/pInCodecCtx->channels/av_get_bytes_per_sample(AV_SAMPLE_FMT_S16),
                            pAudioFrame->data,
                            pAudioFrame->nb_samples);
                        decodedSize = decodedSize * pInCodecCtx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                        if (*size > (size_t)decodedSize) {
                            memcpy(buffer, decodebuffer, decodedSize);
                            buffer += decodedSize;
                            *size -= decodedSize;
                            buffersize += decodedSize;
#ifdef DUMP_PCM_DATA
                            fwrite(decodebuffer, 1, decodedSize, pcm);
#endif
                        } else {
                            memcpy(buffer, decodebuffer, *size);
#ifdef DUMP_PCM_DATA
                            fwrite(decodebuffer, 1, decodedSize, pcm);
#endif
                            buffersize += *size;
                            free(decodebuffer);
                            decodebuffer = NULL;
                            goto done;
                        }

                        free(decodebuffer);
                        decodebuffer = NULL;
                        pktsize -= len;
                        pktdata += len;
                    }
                }
            }
            av_free_packet(packet);
        }
    }
done : {
#ifdef DUMP_PCM_DATA
    fclose(pcm);
#endif
    if (packet != NULL) {
        av_free_packet(packet);
        free(packet);
        packet = NULL;
    }
    long end = clock();
    *size = buffersize;
    MMLOGI("cost time :%f\n", double(end - start) / (double)CLOCKS_PER_SEC);
    av_free(pAudioFrame);
    pAudioFrame = NULL;
    avcodec_close(pInCodecCtx);
    pInCodecCtx = NULL;
    swr_free(&mAVResample);
    av_freep(&pIOContext);
    pIOContext = NULL;

    avformat_close_input(&pInFmtCtx);
    pInFmtCtx = NULL;
    if (inBufferFree) {
        free(inBufferFree);
        inBufferFree = NULL;
    }

    return 0;
}
fail:
    if (iobuffer) {
        av_free(iobuffer);
    }
    if (pIOContext) {
        av_freep(&pIOContext);
    }
    if (mAVResample) {
        swr_free(&mAVResample);
    }
    if (pInCodecCtx) {
        avcodec_close(pInCodecCtx);
    }
    if (pInFmtCtx) {
        avformat_close_input(&pInFmtCtx);
    }

    return -1;
}
