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
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __AUDIO_PULSE__
#include <pulse/sample.h>
#endif
#include "multimedia/audio-decode.h"
static const char *input_url = NULL;

class AudiodecodeTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};


TEST_F(AudiodecodeTest, audiodecodeTest) {
    int ret ;
    audio_sample_spec pcm_spec;
    size_t  size = 10240;
    uint8_t * buf = (uint8_t *)malloc(size);

    ret = audio_decode(input_url, &pcm_spec, buf, &size);
    EXPECT_EQ(ret ,0);
    printf("audio decode  by url OK \n");
    printf("sampleRate = %d\n",pcm_spec.rate);
    printf("channelMask = %d\n",pcm_spec.channels);
    printf("audioFormat = %d\n",pcm_spec.format);
    printf("size = %d\n",size);

    FILE *fp = fopen(input_url, "r");
    if (fp == NULL) {
        free(buf);
        buf = NULL;
        printf("%s fopen fail\n", input_url);
    }
    ASSERT_NE(fp, NULL);
    int fd = fileno(fp);
    size = 10240;
    memset(buf ,0, size);
    memset(&pcm_spec , 0 ,sizeof(pcm_spec));
    int64_t offset =0, length =0;
    ret =  audio_decode_f(fd, offset, length, &pcm_spec, buf, &size);
    free(buf);
    buf = NULL;
    fclose(fp);
    EXPECT_EQ(ret ,0);
    printf("audio decode  by fd OK \n");

    FILE *fp1 = fopen(input_url, "r");
    buf = (uint8_t *)malloc(size);
    size = 10240;
    memset(buf ,0, size);
    memset(&pcm_spec , 0 ,sizeof(pcm_spec));
    uint8_t * in_buf = (uint8_t *)malloc(size);
    size_t buffer_size = fread(in_buf, sizeof(char), size, fp1);
    ret =  audio_decode_b(in_buf, buffer_size, &pcm_spec, buf, &size);
    fclose(fp1);
    free(buf);
    buf = NULL;
    if (in_buf) {
        free(in_buf);
        in_buf = NULL;
    }
    EXPECT_EQ(ret ,0);
    printf("audio decode  by buffer OK \n");
}

int main(int argc, char** argv) {
    int ret ;
    if (argc < 2) {
        printf("no input media file\n");
        return -1;
    }

    input_url = argv[1];
    printf("input_url = %s\n",input_url);
    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
     } catch (...) {
        printf("InitGoogleTest failed!");
        return -1;
    }
    return ret;
}


