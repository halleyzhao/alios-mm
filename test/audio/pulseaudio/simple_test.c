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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "multimedia/mm_debug.h"
MM_LOG_DEFINE_MODULE_NAME("SIMPLE_TEST")
#define BUFSIZE 8192*2

int main(int argc, char*argv[]) {

    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    pa_buffer_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.maxlength = (uint32_t) -1;;
    attr.tlength = (uint32_t) pa_usec_to_bytes(250*1000, &ss);
    attr.prebuf = (uint32_t) -1;
    attr.minreq =  (uint32_t) -1;
    attr.fragsize = (uint32_t) -1;

    pa_simple *s = NULL;
    int ret = 1;
    int error;
    ssize_t rBytes = 0, wBytes = 0;

    FILE *fin = fopen("/usr/bin/ut/res/audio/af_mixer_pcm.pcm", "r");

    if(fin == NULL){
        fprintf(stderr, __FILE__": fopen() failed: %s\n", strerror(errno));
        return -1;
    }

    if (!(s = pa_simple_new(NULL, "simple_test", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, &attr, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    if(pa_simple_set_volume(s, 65535/2, &error)!=0){
        fprintf(stderr, __FILE__": pa_simple_set_volume() failed: %s\n", pa_strerror(error));
        goto finish;
    }


    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    while((rBytes = fread(buf, 1, BUFSIZE, fin))>0){
        if ( pa_simple_write(s, buf, (size_t) rBytes, &error) < 0 ) {
            fprintf(stderr, "write failed: %s\n", pa_strerror(error));
            goto finish;
        }
    }

    /* Make sure that every single sample was played */
    if (pa_simple_drain(s, &error) < 0) {
        fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    ret = 0;
finish:

    if (s)
        pa_simple_free(s);

    if(fin != NULL){
        fclose(fin);
    }
    return ret;
}

