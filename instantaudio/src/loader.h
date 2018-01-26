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

#ifndef __loader_H
#define __loader_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mmmsgthread.h>
#include <multimedia/mm_buffer.h>
#include <sample.h>

namespace YUNOS_MM {

// #define DUMP_LOADED

// status: MM_ERROR_SUCCESS: success
typedef void (*uri_loaded_cb_t)(const char * uri, int status, Sample * sample, void * userData);
typedef void (*fd_loaded_cb_t)(int fd, int status, Sample * sample, void * userData);
typedef void (*buffer_loaded_cb_t)(MMIDBuffer::id_type bufferId, int status, Sample * sample, void * userData);

class Loader : public MMMsgThread {
public:
    Loader(uri_loaded_cb_t uriLoadedCB,
            fd_loaded_cb_t fdLoadedCB,
            buffer_loaded_cb_t bufferLoadedCB,
            void * loadedCBUserData);
    ~Loader();

public:
    bool init();
    mm_status_t load(const char * uri);
    mm_status_t load(int fd, int64_t offset, int64_t length);
    mm_status_t load(const MMIDBufferSP & buffer);

private:
    struct LoadFDMsg {
        LoadFDMsg(int fd, int64_t offset, int64_t length) : mFd(fd), mOffset(offset), mLength(length)
        {}

        ~LoadFDMsg(){}

        int mFd;
        int64_t mOffset;
        int64_t mLength;
    };

#define MSG_LOAD_URI (msg_type)1
#define MSG_LOAD_FD (msg_type)2
#define MSG_LOAD_BUF (msg_type)3

private:
    Loader();

    Sample * allocSample(const audio_sample_spec * sampleSpec, const uint8_t * data, size_t size);

#ifdef DUMP_LOADED
    void dumpLoaded(const char * fname, const uint8_t * buf, size_t size);
#endif

private:
    uri_loaded_cb_t mURILoadedCB;
    fd_loaded_cb_t mFDLoadedCB;
    buffer_loaded_cb_t mBufferLoadedCB;
    void * mLoadedUserData;
    int mNextSampleId;

    DECLARE_MSG_LOOP()
    DECLARE_MSG_HANDLER(onLoadURI)
    DECLARE_MSG_HANDLER(onLoadFD)
    DECLARE_MSG_HANDLER2(onLoadBuffer)

    MM_DISALLOW_COPY(Loader)
    DECLARE_LOGTAG()
};

}

#endif // __soundloader_H

