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
#include <dirent.h>
#include <unistd.h>
 #include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "multimedia/mm_debug.h"
MM_LOG_DEFINE_MODULE_NAME("PA_TEST")

#define BUFSIZE 8192*2
static const char *DATA_PATH = "/usr/bin/ut/res/audio/audiocompat/pcms";

#define get_element(_ele, _end) do {\
    char * q = _ele;\
    while ( *p != '\0' && *p != _end ) {\
        *q++ = *p++;\
    }\
    if ( *_ele == '\0' ) {\
        printf("invalid file name, line: %d\n", __LINE__);\
        return -1;\
    }\
    /*printf("ele: %s\n", _ele);*/\
    ++p;\
}while(0)



static int str2fmt(const char * str, pa_sample_format * fmt)
{
#define see(_str, _fmt) do {\
    if ( !strcmp(str, _str) ) {\
        *fmt = _fmt;\
        return 0;\
    }\
}while(0)

    see("U8", PA_SAMPLE_U8);
    see("ALAW", PA_SAMPLE_ALAW);
    see("ULAW", PA_SAMPLE_ULAW);
    see("S16LE", PA_SAMPLE_S16LE);
    see("S16BE", PA_SAMPLE_S16BE);
    see("FLOAT32LE", PA_SAMPLE_FLOAT32LE);
    see("FLOAT32BE", PA_SAMPLE_FLOAT32BE);
    see("S32LE", PA_SAMPLE_S32LE);
    see("S32BE", PA_SAMPLE_S32BE);
    see("S24LE", PA_SAMPLE_S24LE);
    see("S24BE", PA_SAMPLE_S24BE);
    see("S24-32LE", PA_SAMPLE_S24_32LE);
    see("S24-32BE", PA_SAMPLE_S24_32BE);
    return -1;
}

static int test(const char * fname)
{
    const char * p = fname;
    char buf[BUFSIZE];
    ssize_t rBytes = 0;
    unsigned int ii;

#define TMP_STR_SIZE 16
    char format[TMP_STR_SIZE];
    char rate[TMP_STR_SIZE];
    char channel[TMP_STR_SIZE];
    memset(format, 0, TMP_STR_SIZE);
    memset(rate, 0, TMP_STR_SIZE);
    memset(channel, 0, TMP_STR_SIZE);

    while ( *p != '\0' ) {
        if ( *p != '[' ) {
            ++p;
            continue;
        }
        ++p;
        break;
    }


    get_element(rate, '_');
    get_element(format, '_');
    get_element(channel, ']');

    pa_sample_spec ss;
    if ( str2fmt(format, &ss.format) ) {
        printf("invalid file name(format)\n");
        return -1;
    }

    sscanf(rate, "%d", &ss.rate);
    sscanf(channel, "%u", &ii);
    ss.channels = (uint8_t)ii;

//    printf("format: %d, rate: %u, channel: %d\n", ss.format, ss.rate, ss.channels);

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


    FILE *fin = fopen(fname, "r");

    if(fin == NULL){
        printf(__FILE__": fopen() failed: %s\n", strerror(errno));
        return -1;
    }

    if (!(s = pa_simple_new(NULL, "simple_test", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, &attr, &error))) {
        printf(__FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    if(pa_simple_set_volume(s, 65535, &error)!=0){
        printf(__FILE__": pa_simple_set_volume() failed: %s\n", pa_strerror(error));
        goto finish;
    }


    memset(buf, 0, BUFSIZE);
    while((rBytes = fread(buf, 1, BUFSIZE, fin))>0){
        if ( pa_simple_write(s, buf, (size_t) rBytes, &error) < 0 ) {
            printf("write failed: %s\n", pa_strerror(error));
            goto finish;
        }
    }

    /* Make sure that every single sample was played */
    if (pa_simple_drain(s, &error) < 0) {
        printf(__FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
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

static int compatible_test()
{
    int success = 0;
    int fail = 0;
    struct dirent    *dp;
    DIR              *dfd;
    dfd = opendir(DATA_PATH);
    if ( !dfd ) {
        printf("failed to read dir\n");
        return -1;
    }

    while ( 1 ) {
        dp = readdir(dfd);
        if ( !dp ) {
            break;
        }

        if ( dp->d_type != 8 ) {
            continue;
        }

        printf("testing %s...\n", dp->d_name);
        char path[128];
        sprintf(path, "%s/%s%s", DATA_PATH, dp->d_name, "\0");
        int ret = test(path);
        if ( ret ) {
            ++fail;
            printf("[failed]\n");
        } else {
            ++success;
            printf("[success]\n");
        }
    }

    printf("***************\nsuccess: %d, failed: %d, total: %d\n***************\n", success, fail, success + fail);
    free(dp);
    if (dfd) {
        closedir(dfd);
    }
    return 0;
}

#define FORK_COUNT 10

#define FORK_TIMES 5

struct PidInfo {
    pid_t pid;
    int state; // > 0, running
};

static int stress_multi_precoss_test()
{
    for ( int j = 0; j < FORK_TIMES; ++j ) {
        PidInfo info[FORK_COUNT];
        for ( int i = 0; i < FORK_COUNT; ++i ) {
            pid_t p = fork();
            info[i].pid = p;
            info[i].state = 0;
            if ( p < 0 ) {
                printf("failed to fork: %d\n", i);
                info[i].state = -1;
                usleep(1000000);
                continue;
            }

            if ( p == 0 ) {
                int i = execl("/usr/bin/async-test", "async-test", "async-test", (char*)0);
                printf("i: %d, errno: %d\n", i, errno);
                return 0;
            }

            printf("forked: %d\n", i);
            usleep(1000000);
        }
        for ( int ii = 0; ii < FORK_COUNT; ++ii ) {
            int status;
            pid_t w;
            printf("waitting %d\n", ii);
            do {
                   w = waitpid(info[ii].pid, &status, WUNTRACED | WCONTINUED);
                   if (w == -1) {
                       perror("waitpid");
                       exit(-1);
                   }

                   if (WIFEXITED(status)) {
                       printf("exited, status=%d\n", WEXITSTATUS(status));
                   } else if (WIFSIGNALED(status)) {
                       printf("killed by signal %d\n", WTERMSIG(status));
                   } else if (WIFSTOPPED(status)) {
                       printf("stopped by signal %d\n", WSTOPSIG(status));
                   } else if (WIFCONTINUED(status)) {
                       printf("continued\n");
                   }
               } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        }
    }
    return 0;
}


static int long_time_play_10_process_test_do()
{
    const char * fname = "/usr/bin/ut/res/audio/af_mixer_pcm.pcm";
    char buf[BUFSIZE];
    ssize_t rBytes = 0;

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


    FILE *fin = fopen(fname, "r");

    if(fin == NULL){
        printf(__FILE__": fopen() failed: %s\n", strerror(errno));
        return -1;
    }

    if (!(s = pa_simple_new(NULL, "simple_test", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, &attr, &error))) {
        printf(__FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    if(pa_simple_set_volume(s, 65535, &error)!=0){
        printf(__FILE__": pa_simple_set_volume() failed: %s\n", pa_strerror(error));
        goto finish;
    }


retry:
    memset(buf, 0, BUFSIZE);
    while((rBytes = fread(buf, 1, BUFSIZE, fin))>0){
        if ( pa_simple_write(s, buf, (size_t) rBytes, &error) < 0 ) {
            printf("write failed: %s\n", pa_strerror(error));
            goto finish;
        }
    }
    if (feof(fin)) {
        /* Make sure that every single sample was played */
        if (pa_simple_drain(s, &error) < 0) {
            printf(__FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        ret = 0;
    } else {
        fseek(fin, 0, SEEK_SET);
        goto retry;
    }

finish:

    if (s)
        pa_simple_free(s);

    if(fin != NULL){
        fclose(fin);
    }
    return ret;
}

static int long_time_play_10_process_test()
{
    PidInfo info[FORK_COUNT];
    for ( int i = 0; i < FORK_COUNT; ++i ) {
        pid_t p = fork();
        info[i].pid = p;
        info[i].state = 0;
        if ( p < 0 ) {
            printf("failed to fork: %d\n", i);
            info[i].state = -1;
            usleep(1000000);
            continue;
        }

        if ( p == 0 ) {
            return long_time_play_10_process_test_do();
        }

        printf("forked: %d\n", i);
        usleep(1000000);
    }
    for ( int ii = 0; ii < FORK_COUNT; ++ii ) {
        int status;
        pid_t w;
        printf("waitting %d\n", ii);
        do {
               w = waitpid(info[ii].pid, &status, WUNTRACED | WCONTINUED);
               if (w == -1) {
                   perror("waitpid");
                   exit(-1);
               }

               if (WIFEXITED(status)) {
                   printf("exited, status=%d\n", WEXITSTATUS(status));
               } else if (WIFSIGNALED(status)) {
                   printf("killed by signal %d\n", WTERMSIG(status));
               } else if (WIFSTOPPED(status)) {
                   printf("stopped by signal %d\n", WSTOPSIG(status));
               } else if (WIFCONTINUED(status)) {
                   printf("continued\n");
               }
           } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    }

    return 0;
}

typedef int (*test_func_t)();
struct TestItem {
    const char * item;
    const char * desc;
    test_func_t func;
};

static TestItem testItem[] = {
    {
        "cp",
        "compatible test",
        compatible_test
    },
    {
        "sm",
        "stress test, multiple process",
        stress_multi_precoss_test
    },
    {
        "lp",
        "long time play test, 10 process",
        long_time_play_10_process_test
    }
};

#define testItemCount (sizeof(testItem) / sizeof(TestItem))

static void show_usage(const char * name)
{
    printf("usage: \n");
    printf("%s item\n", name);
    printf("item:\n");
    for ( size_t i = 0; i < testItemCount; ++i ) {
        printf("\t%s -- %s\n", testItem[i].item, testItem[i].desc);
    }
    printf("\tall -- all the above.\n");
}

int main(int argc, char*argv[])
{

    if ( argc != 2 ) {
        show_usage(argv[0]);
        return 0;
    }

    for ( size_t i = 0; i < testItemCount; ++i ) {
        if ( !strcmp(argv[1], testItem[i].item) ) {
            testItem[i].func();
            return 0;
        }
    }

    show_usage(argv[0]);
    return 0;

#if 0
    int success = 0;
    int fail = 0;
    struct dirent    *dp;
    DIR              *dfd;
    dfd = opendir(DATA_PATH);
    if ( !dfd ) {
        printf("failed to read dir\n");
        return -1;
    }

    while ( 1 ) {
        dp = readdir(dfd);
        if ( !dp ) {
            break;
        }

        if ( dp->d_type != 8 ) {
            continue;
        }

        printf("testing %s...\n", dp->d_name);
        char path[128];
        sprintf(path, "%s/%s%s", DATA_PATH, dp->d_name, "\0");
        int ret = test(path);
        if ( ret ) {
            ++fail;
            printf("[failed]\n");
        } else {
            ++success;
            printf("[success]\n");
        }
    }

    printf("***************\nsuccess: %d, failed: %d, total: %d\n***************\n", success, fail, success + fail);
#endif
}

