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

#ifndef __sample_H
#define __sample_H

//#include <pulse/sample.h>
//#include <pulse/pulseaudio.h>
//#include <pulse/thread-mainloop.h>
#include <multimedia/audio-decode.h>

#include <multimedia/mm_cpp_utils.h>

#define MAX_SAMPLE_SIZE 


namespace YUNOS_MM {

class Sample {
public:
    Sample(int id);
    ~Sample();

public:
    bool setSample(const audio_sample_spec * sampleSpec, const uint8_t * data, size_t size);
    int id() const { return mSampleId; }
    const audio_sample_spec * sampleSpec() const { return &mSampleSpec; }
    const uint8_t * data() const { return mData; }
    size_t size() const { return mSize; }

private:
    Sample();

private:
    int mSampleId;
    audio_sample_spec mSampleSpec;
    uint8_t * mData;
    size_t mSize;

    MM_DISALLOW_COPY(Sample)
    DECLARE_LOGTAG()
};

}
#endif // __soundsample_H

