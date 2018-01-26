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

#ifndef __instantaudio_H
#define __instantaudio_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mmlistener.h>
#include <multimedia/mm_buffer.h>

namespace YUNOS_MM {

class InstantAudio {
public:
    static InstantAudio * create(size_t maxChannels);
    static void destroy(InstantAudio * instantaudio);

public:
    InstantAudio();
    virtual ~InstantAudio();

public:
    class Listener : public MMListener {
    public:
        Listener(){}
        virtual ~Listener(){}

        enum message_type {
            // URI sample loaded
            // param1: error code. MM_ERROR_SUCCESS means success, or is the error code.
            // param2: sampleID
            // obj: be the following orderly:
            //        string -- the uri specified when call load().
            URI_SAMPLE_LOADED = 0,
            // URI sample loaded
            // param1: error code. MM_ERROR_SUCCESS means success, or is the error code.
            // param2: fd. the fd specified when call load().
            // obj: be the following orderly:
            //        int32 -- sampleID.
            FD_SAMPLE_LOADED = 1,
            // URI sample loaded
            // param1: error code. MM_ERROR_SUCCESS means success, or is the error code.
            // param2: sampleID
            // obj: be the following orderly:
            //        int32 -- the bufferId specified when call load().
            BUFFER_SAMPLE_LOADED = 2,
        };
    };

public:
    struct VolumeInfo {
        float left;
        float right;
    };

public:
    virtual mm_status_t setListener(Listener * listener);

    virtual mm_status_t load(const char* uri) = 0;
    virtual mm_status_t load(int fd, int64_t offset, int64_t length) = 0;
    virtual mm_status_t load(const MMIDBufferSP & buffer) = 0;
    virtual mm_status_t unload(int sampleId) = 0;
    virtual mm_status_t unloadAll() = 0;
    virtual int play(int sampleID, const VolumeInfo * volume, int loop, float rate, int priority, int streamType) = 0;
    virtual int play(int sampleID, const VolumeInfo * volume, int loop, float rate, int priority, const char * connectionId) = 0;
    virtual mm_status_t pause(int channelID) = 0;
    virtual mm_status_t pauseAll() = 0;
    virtual mm_status_t resume(int channelID) = 0;
    virtual mm_status_t resumeAll() = 0;
    virtual mm_status_t stop(int channelID) = 0;
    virtual mm_status_t stopAll() = 0;
    virtual mm_status_t setVolume(int channelID, const VolumeInfo * volume) = 0;
    virtual mm_status_t setLoop(int channelID, int loop) = 0;
    virtual mm_status_t setRate(int channelID, float rate) = 0;
    virtual mm_status_t setPriority(int channelID, int priority) = 0;


protected:
    Listener * mListener;

    MM_DISALLOW_COPY(InstantAudio)
    DECLARE_LOGTAG()
};

}
#endif
