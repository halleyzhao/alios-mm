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

#include <stdint.h>
#include "multimedia/mm_audio.h"

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct audio_sample_spec {
        snd_format_t format;
        /**< The sample format */
        uint32_t rate;
        /**< The sample rate. (e.g. 44100) */
        uint8_t channels;
        /**< Audio channels. (1 for mono, 2 for stereo, ...) */
    } audio_sample_spec;

    int audio_decode(const char* url, audio_sample_spec *pcm_spec, uint8_t* buf, size_t* size);
    int audio_decode_f(int fd, int64_t offset, int64_t length, audio_sample_spec *pcm_spec, uint8_t* buf, size_t* size);
    int audio_decode_b(uint8_t *in_buf, int buf_size, audio_sample_spec *pcm_spec, uint8_t* out_buf, size_t* size);
    bool get_output_samplerate();

#ifdef __cplusplus
};
#endif

