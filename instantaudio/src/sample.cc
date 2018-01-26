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

#include <string.h>

#include "sample.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(Sample)
#define DEFAULT_SAMPLE_RATE     44100
#define DEFAULT_CHANNEL         2

Sample::Sample(int id) :
                            mSampleId(id),
                            mData(NULL),
                            mSize(0)
{
    MMLOGI("id: %d\n", mSampleId);

    mSampleSpec.format = SND_FORMAT_PCM_16_BIT;
    mSampleSpec.rate = DEFAULT_SAMPLE_RATE;
    mSampleSpec.channels = DEFAULT_CHANNEL;
}

Sample::~Sample()
{
    MMLOGI("+\n");
    MM_RELEASE_ARRAY(mData);
}

bool Sample::setSample(const audio_sample_spec * sampleSpec, const uint8_t * data, size_t size)
{
    MMASSERT(mData == NULL);
    if ( !sampleSpec || !data || !size > 0 ) {
        MMLOGE("invalid args\n");
        return false;
    }

    MMLOGI("format: %d, rate: %u, channels: %u, data: %p, size: %u",
                sampleSpec->format,
                sampleSpec->rate,
                sampleSpec->channels,
                data,
                size);

    mData = new uint8_t[size];
    if ( !mData ) {
        MMLOGE("no mem\n");
        return false;
    }

    memcpy(&mSampleSpec, sampleSpec, sizeof(audio_sample_spec));
    memcpy(mData, data, size);
    mSize = size;
    return true;
}

}


