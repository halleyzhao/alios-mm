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
#include "multimedia/mm_audio.h"
#include <config.h>
#include "multimedia/mm_audio_compat.h"

#define AUDIOTRACK_CONTAINER_DTSHD  0
#define AUDIOTRACK_CONTAINER_AC3    1
#define AUDIOTRACK_CONTAINER_EAC3   2
#include "mm_pulse_utils.h"
#include <pulsecore/log.h>

DECLARE_PA_LOG_TAG("dts_pa_test");

#define PRINT_DTS_LOG_INFO  1
typedef void (*pa_operation_cb_t)(void);

//just for direct track within this stage
enum {
    SUBCOMMAND_CREATE_TRACK,
    SUBCOMMAND_START_TRACK,
    SUBCOMMAND_STOP_TRACK,
    SUBCOMMAND_DESTROY_TRACK,
};

static void createPulseAudio();
static void context_state_callback(pa_context *c, void *userdata);
static void stream_state_callback(pa_stream *s, void *userdata) ;
static void nop_free_cb(void *p);
static void underflow_cb(struct pa_stream *s, void *userdata);
static void stream_write_cb(pa_stream *p, size_t nbytes, void *userdata);
static void render_audio();
static void pa_create_track_cb(pa_context *c, uint32_t success, void *userdata);
static void pa_destroy_track_cb(pa_context *c, uint32_t success, void *userdata);
static void cork_state_callback(pa_stream*s, int success, void *userdata);

static pa_context *track_context = NULL;
static pa_stream *track_stream;
static pa_mainloop_api *track_mainloop_api = NULL;
static int32_t sample_rate;
static int32_t channel_count;
static int32_t format;
static pa_buffer_attr track_buffer_attr;
static bool can_stop_track;

int main(int argc, char *argv[]) {
    sample_rate = 44100;
    format = 0x08000000UL;
    channel_count = 2;
    mm_pa_log_warn("test main start");
    createPulseAudio();

    mm_pa_log_warn("test main end");

    return 0;
}

void createPulseAudio() {
    pa_mainloop *pm = NULL;
    track_stream = NULL;
    can_stop_track = false;

    mm_pa_log_warn("PA DTS createPulseAudio 000000 \n");
    pm = pa_mainloop_new();
    if (pm == NULL) {
        mm_pa_log_warn("create pa_mainloop failed");
        return -1;
    }
    mm_pa_log_warn("PA DTS createPulseAudio 111111 \n");

    track_mainloop_api = pa_mainloop_get_api(pm);
    if (track_mainloop_api == NULL) {
        mm_pa_log_warn("get pa_mainloop_api failed");
        pa_mainloop_free(pm);
        return -1;
    }
    mm_pa_log_warn("PA DTS createPulseAudio 222222 \n");

    track_context = pa_context_new(track_mainloop_api, "module-dts-test");
    if (track_context == NULL) {
        //pa_log_debug("create track_context failed");
        pa_mainloop_free(pm);
        return -1;
    }
    mm_pa_log_warn("PA DTS createPulseAudio 333333 \n");

    pa_context_set_state_callback(track_context, context_state_callback, NULL);
    if (pa_context_connect(track_context, NULL,(pa_context_flags_t)0, NULL) < 0) {
        mm_pa_log_warn("pa_context_connect track_context failed");
        pa_context_unref(track_context);
        pa_mainloop_free(pm);
        return -1;
    }
    mm_pa_log_warn("PA DTS createPulseAudio 444444 \n");

    int ret;
    if (pa_mainloop_run(pm, &ret) < 0) {
        mm_pa_log_debug("pa_mainloop_run track_context failed");
        pa_context_unref(track_context);
        if (track_stream)
            pa_stream_unref(track_stream);
        pa_mainloop_free(pm);
        return -1;
    }
    mm_pa_log_warn("PA DTS createPulseAudio 555555 \n");
}

static void context_state_callback(pa_context *c, void *userdata) {
    mm_pa_log_warn("PA DTS context_state_callback 000000 c:0x%x\n",c);

    if (c == NULL) {
        mm_pa_log_warn("context_state_callback context is null");
        return ;
    }

    if (c != track_context) {
        mm_pa_log_warn("context_state_callback c != track_context");
    }

    mm_pa_log_warn("PA DTS context_state_callback 111111\n");
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        mm_pa_log_warn("PA DTS context_state_callback 222222 state:%d\n",pa_context_get_state(c));
        break;

        case PA_CONTEXT_READY: {
        mm_pa_log_warn("PA DTS context_state_callback 333333 READY\n");

        pa_format_info *pa_format[1];
        pa_stream_flags_t flags = PA_STREAM_AUTO_TIMING_UPDATE |
        PA_STREAM_INTERPOLATE_TIMING;

        pa_format[0] = pa_format_info_new();
        pa_format[0]->encoding = PA_ENCODING_PCM;
        //pa_format[0]->encoding = PA_ENCODING_DTS_IEC61937;//PA_ENCODING_PCM;;//format;
        //PA_ENCODING_DTS_IEC61937 PA_ENCODING_EAC3_IEC61937  PA_ENCODING_AC3_IEC61937  PA_ENCODING_MPEG2_AAC_IEC61937
        pa_format_info_set_sample_format(pa_format[0],PA_SAMPLE_S16LE);
        pa_format_info_set_rate(pa_format[0], sample_rate);
        pa_format_info_set_channels(pa_format[0], channel_count);

        mm_pa_log_warn("PA DTS context_state_callback 444444\n");

        //pa_sample_spec g_pa_sample_spec;
        //g_pa_sample_spec.format =PA_SAMPLE_S16LE;
        //g_pa_sample_spec.rate = 44100;//audio data sample_rate
        //g_pa_sample_spec.channels = 2;
        //track_stream = pa_stream_new(track_context, "module-dts-test", &g_pa_sample_spec, NULL);

        track_stream = pa_stream_new_extended(track_context, "module-dts-test", pa_format, 1, NULL);
        if (track_stream == NULL) {
            //pa_log_debug("pa_stream_new_extended failed");
            return;
        }

        mm_pa_log_warn("PA DTS context_state_callback 555555\n");

        memset(&track_buffer_attr, 0, sizeof(track_buffer_attr));
        track_buffer_attr.prebuf = (uint32_t) -1;
        track_buffer_attr.minreq = (uint32_t) -1;
        track_buffer_attr.fragsize = (uint32_t) -1;
        track_buffer_attr.maxlength = (uint32_t) -1;
        //track_buffer_attr.tlength = (uint32_t) pa_usec_to_bytes(2500*PA_USEC_PER_MSEC, &g_pa_sample_spec);
        track_buffer_attr.tlength =(uint32_t) -1;

        //track_buffer_attr = pa_stream_get_buffer_attr(track_stream);
        int ret = pa_stream_connect_playback(track_stream, NULL,
        &track_buffer_attr,
        flags, NULL, NULL);
        mm_pa_log_warn("PA DTS context_state_callback 666666 ret:%d\n",ret);
        if (ret != 0) {
            mm_pa_log_warn("pa_stream_connect_playback failed %d",ret);
            return;
        }
        pa_stream_set_state_callback(track_stream, stream_state_callback, NULL);
        mm_pa_log_warn("PA DTS context_state_callback 777777\n");

        pa_format_info_free(pa_format[0]);
        break;
        }

        case PA_CONTEXT_TERMINATED:
        mm_pa_log_warn("PA DTS context_state_callback 888888 :%s\n", pa_strerror(pa_context_errno(c)));
        track_mainloop_api->quit(track_mainloop_api, 0);
        break;

        case PA_CONTEXT_FAILED:
        default:
        mm_pa_log_warn("PA DTS context_state_callback 999999 %s\n", pa_strerror(pa_context_errno(c)));
        break;
    }
}

static void stream_state_callback(pa_stream *s, void *userdata)  {
    mm_pa_log_warn("PA DTS stream_state_callback 000000\n");

    if (NULL == s) {
        //pa_log_debug("stream_state_callback stream is null");
        return;
    }

    if (s != track_stream) {
        //pa_log_debug("stream_state_callback s != track_stream");
    }

    mm_pa_log_warn("PA DTS stream_state_callback 111111\n");
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
        mm_pa_log_warn("PA DTS stream_state_callback 222222:%s\n", pa_stream_get_state(s));
        break;

        case PA_STREAM_READY: {
            mm_pa_log_warn("PA DTS stream_state_callback 333333 READ\n");

            pa_stream_set_underflow_callback(track_stream, underflow_cb, NULL);
            pa_stream_set_write_callback(track_stream, stream_write_cb, NULL);

            pa_operation *opr_create;
            opr_create = createDirectTrack(track_context, NULL, pa_create_track_cb, sample_rate, format);

            mm_pa_log_warn("PA DTS stream_state_callback 444444 READ\n");

            break;
        }

        case PA_STREAM_FAILED:
        default:
        mm_pa_log_warn("stream_state_callback start 555555 %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
        break;
    }
}

#define WRITE_BUFFER_SIZE 1024
static void render_audio(){
    size_t pa_size;
    size_t written_length;
    char buffer[WRITE_BUFFER_SIZE];//from player
    FILE *fin = fopen("/usr/bin/ut/res/audio/af_mixer_pcm.pcm", "r");

    mm_pa_log_warn("PA DTS render_audio 000000\n");

    if(fin == NULL){
        mm_pa_log_warn("render_audio start open file failed 111111");
        return;
    }
    memset(buffer, 0, WRITE_BUFFER_SIZE);

    mm_pa_log_warn("PA DTS render_audio 111111\n");

    while ((written_length = fread(buffer, 1, WRITE_BUFFER_SIZE, fin)) > 0 ) {
        retry:
        pa_size = pa_stream_writable_size(track_stream);
        mm_pa_log_warn("PA DTS render_audio 222222 :%d\n",pa_size);
        if (pa_size < written_length) {
            mm_pa_log_warn("render_audio retry  write buffer not enough 00000000000");
            usleep(5000);
            goto retry;
        }
        if (pa_stream_write(track_stream, buffer, written_length, nop_free_cb, 0, (pa_seek_mode_t)0) != 0) {
            mm_pa_log_warn("render_audio start  333333");
        }

        mm_pa_log_warn("PA DTS render_audio 3333333:%d,%d,pa_size=%d", written_length, sizeof(buffer), pa_size);
    }

    mm_pa_log_warn("PA DTS render_audio write finished, we should destroy the direct track");
    //pa_operation *opr_destroy;
    //opr_destroy = destroyDirectTrack(track_context, NULL, pa_destroy_track_cb);
    mm_pa_log_warn("PA DTS render_audio 444444:%d\n");

    fclose(fin);
}

static void nop_free_cb(void *p) {

}

static void underflow_cb(struct pa_stream *s, void *userdata) {

}

static void stream_write_cb(pa_stream *p, size_t nbytes, void *userdata) {

}

static void pa_create_track_cb(pa_context *c, uint32_t success, void *userdata) {
    mm_pa_log_warn("DTA PA pa_create_track_cb start\n");
    //if (success == 1) {
    mm_pa_log_warn("DTA PA pa_create_track_cb 000000  success=%d\n", success);
    //pa_operation *opr_cork;
    //opr_cork = pa_stream_cork(track_stream, 1, NULL, NULL);
    //opr_cork = pa_stream_cork(track_stream, 0, NULL, NULL);
    //while ( pa_operation_get_state (opr_cork) == PA_OPERATION_RUNNING) {
    //mm_pa_log_warn("DTA PA pa_create_track_cb wait for cork complete\n");
    //continue;
    //}

    mm_pa_log_warn("DTS PA pa_create_track_cb start render_audio 000000\n");
    pthread_t audio_render_thread;
    pthread_create(&audio_render_thread, NULL, render_audio, NULL);
//}
}

static void pa_destroy_track_cb(pa_context *c, uint32_t success, void *userdata) {

}

static void cork_state_callback(pa_stream*s, int success, void *userdata) {
mm_pa_log_warn("DTA PA cork_state_callback start\n");
    if (success == 1) {
        mm_pa_log_warn("DTA PA cork_state_callback 000000\n");
        pthread_t audio_render_thread;
        pthread_create(&audio_render_thread, NULL, render_audio, NULL);
    }
}

