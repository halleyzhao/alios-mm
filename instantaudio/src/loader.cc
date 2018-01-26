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

#include "loader.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

#define MAX_SAMEPLE_SIZE 204800
static const char * MMMSGTHREAD_NAME = "Loader";

DEFINE_LOGTAG(Loader)

BEGIN_MSG_LOOP(Loader)
    MSG_ITEM(MSG_LOAD_URI, onLoadURI)
    MSG_ITEM(MSG_LOAD_FD, onLoadFD)
    MSG_ITEM2(MSG_LOAD_BUF, onLoadBuffer)
END_MSG_LOOP()

Loader::Loader(uri_loaded_cb_t uriLoadedCB,
                        fd_loaded_cb_t fdLoadedCB,
                        buffer_loaded_cb_t bufferLoadedCB,
                        void * loadedCBUserData)
                        : MMMsgThread(MMMSGTHREAD_NAME),
                        mURILoadedCB(uriLoadedCB),
                        mFDLoadedCB(fdLoadedCB),
                        mBufferLoadedCB(bufferLoadedCB),
                        mLoadedUserData(loadedCBUserData),
                        mNextSampleId(0)
{
    MMASSERT(uriLoadedCB != NULL);
    MMASSERT(fdLoadedCB != NULL);
    MMASSERT(loadedCBUserData != NULL);
}

Loader::~Loader()
{
}

bool Loader::init()
{
    MMLOGI("+\n");
    return create() == 0;
}

mm_status_t Loader::load(const char * uri)
{
    MMLOGV("uri: %s\n", uri);
    char * param2 = new char [strlen(uri) + 1];
    if ( !param2 ) {
        MMLOGE("no mem\n");
        return MM_ERROR_NO_MEM;
    }
    strcpy(param2, uri);

    if ( postMsg(MSG_LOAD_URI, 0, param2) ) {
        MMLOGE("failed to post msg\n");
        MM_RELEASE_ARRAY(param2);
        return MM_ERROR_OP_FAILED;
    }

    return MM_ERROR_SUCCESS;
}

mm_status_t Loader::load(int fd, int64_t offset, int64_t length)
{
    MMLOGV("fd: %d, offset: %" PRId64 ", length: %" PRId64 "\n", fd, offset, length);
    LoadFDMsg * param2 = new LoadFDMsg(fd, offset, length);
    if ( !param2 ) {
        MMLOGE("no mem\n");
        return MM_ERROR_NO_MEM;
    }

    if ( postMsg(MSG_LOAD_FD, 0, param2) ) {
        MMLOGE("failed to post msg\n");
        MM_RELEASE(param2);
        return MM_ERROR_OP_FAILED;
    }

    return MM_ERROR_SUCCESS;
}

mm_status_t Loader::load(const MMIDBufferSP & buffer)
{
    MMLOGI("+\n");

    if ( postMsg(MSG_LOAD_BUF, 0, 0, 0, buffer) ) {
        MMLOGE("failed to post msg\n");
        return MM_ERROR_OP_FAILED;
    }

    return MM_ERROR_SUCCESS;
}

void Loader::onLoadURI(param1_type param1, param2_type param2, uint32_t rspId)
{
    MMLOGD(">>>param1: %u, param2: %p, rspId: %u\n", param1, param2, rspId);
    MMASSERT(rspId == 0);
    char * uri = static_cast<char*>(param2);
    MMASSERT(uri != NULL);
    MMLOGD("uri: %s\n", uri);

    int status;
    Sample * sample = NULL;
    audio_sample_spec sampleSpec;
    uint8_t * data = new uint8_t[MAX_SAMEPLE_SIZE];
    size_t size = MAX_SAMEPLE_SIZE;
    if ( !data ) {
        MMLOGE("failed to new\n");
        status = MM_ERROR_NO_MEM;
        goto exit;
    }
    // decode
    MMLOGD("call decode: %p, size: %u\n", data, size);
    status = audio_decode(uri, &sampleSpec, data, &size);
    if ( status != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to decode\n");
        status = MM_ERROR_OP_FAILED;
        goto exit;
    }

    MMLOGD("decode over, uri: %s, status: %d, data: %p, size: %u\n", uri, status, data, size);
    sample = allocSample(&sampleSpec, data, size);
    if ( !sample ) {
        MMLOGE("failed to alloc sample\n");
        status = MM_ERROR_NO_MEM;
        goto exit;
    }

exit:
    mURILoadedCB(uri, status, sample, mLoadedUserData);
    MM_RELEASE_ARRAY(uri);
    MM_RELEASE_ARRAY(data);
    MMLOGD("<<<\n");
}

void Loader::onLoadFD(param1_type param1, param2_type param2, uint32_t rspId)
{
    MMLOGD(">>>param1: %u, param2: %p, rspId: %u\n", param1, param2, rspId);
    MMASSERT(rspId == 0);
    LoadFDMsg * msg = static_cast<LoadFDMsg*>(param2);
    MMASSERT(msg != NULL);
    MMLOGV("fd: %d, offset: %" PRId64 ", len: %" PRId64 "\n", msg->mFd, msg->mOffset, msg->mLength);

    int status;
    Sample * sample = NULL;
    audio_sample_spec sampleSpec;
    uint8_t * data = new uint8_t[MAX_SAMEPLE_SIZE];
    size_t size = MAX_SAMEPLE_SIZE;
    if ( !data ) {
        MMLOGE("failed to new\n");
        status = MM_ERROR_NO_MEM;
        goto exit;
    }
    // decode
    MMLOGD("call decode: %p, size: %u\n", data, size);
    status = audio_decode_f(msg->mFd, msg->mOffset, msg->mLength, &sampleSpec, data, &size);
    if ( status != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to decode\n");
        status = MM_ERROR_OP_FAILED;
        goto exit;
    }

    MMLOGD("decode over, fd: %d, status: %d, data: %p, size: %u\n", msg->mFd, status, data, size);
    if ( status == MM_ERROR_SUCCESS ) {
        sample = allocSample(&sampleSpec, data, size);
        if ( !sample ) {
            MMLOGE("failed to alloc sample\n");
            status = MM_ERROR_NO_MEM;
            goto exit;
        }
    }

exit:
    mFDLoadedCB(msg->mFd, status, sample, mLoadedUserData);
    MM_RELEASE(msg);
    MM_RELEASE_ARRAY(data);
    MMLOGD("<<<\n");
}

void Loader::onLoadBuffer(param1_type param1, param2_type param2, param3_type param3, uint32_t rspId)
{
    MMLOGD(">>>param1: %u, param2: %p, rspId: %u\n", param1, param2, rspId);
    MMASSERT(rspId == 0);
    MMIDBufferSP buffer = std::tr1::dynamic_pointer_cast<MMIDBuffer>(param3);
    MMASSERT(buffer != NULL);
    MMLOGD("buf size: %d\n", buffer->getActualSize());

    int status;
    Sample * sample = NULL;
    audio_sample_spec sampleSpec;
    uint8_t * data = new uint8_t[MAX_SAMEPLE_SIZE];
    size_t size = MAX_SAMEPLE_SIZE;
    if ( !data ) {
        MMLOGE("failed to new\n");
        status = MM_ERROR_NO_MEM;
        goto exit;
    }
    // decode
    MMLOGD("call decode: %p, size: %u\n", data, size);
    status = audio_decode_b((uint8_t *)buffer->getBuffer(), buffer->getActualSize(), &sampleSpec, data, &size);
    if ( status != MM_ERROR_SUCCESS ) {
        MMLOGE("failed to decode\n");
        status = MM_ERROR_OP_FAILED;
        goto exit;
    }

#ifdef DUMP_LOADED
    char fname[1024];
    sprintf(fname, "/data/buf_%d.pcm", buffer->id());
    dumpLoaded(fname, data, size);
    sprintf(fname, "/data/buf_%d_src.pcm", buffer->id());
    dumpLoaded(fname, buffer->getBuffer(), buffer->getActualSize());
#endif

    MMLOGD("decode over, buffer: %p, status: %d, data: %p, size: %u\n", buffer->getBuffer(), status, data, size);
    sample = allocSample(&sampleSpec, data, size);
    if ( !sample ) {
        MMLOGE("failed to alloc sample\n");
        status = MM_ERROR_NO_MEM;
        goto exit;
    }

exit:
    mBufferLoadedCB(buffer->id(), status, sample, mLoadedUserData);
    MM_RELEASE_ARRAY(data);
    MMLOGD("<<<\n");
}

#ifdef DUMP_LOADED
void Loader::dumpLoaded(const char * fname, const uint8_t * buf, size_t size)
{
    MMLOGV("dump: fname: %s, size: %u\n", fname, buf, size);
    FILE * f = fopen(fname, "w");
    if ( !f ) {
        MMLOGE("failed to open dumpfile\n");
        return;
    }

    fwrite(buf, 1, size, f);
    fclose(f);
}
#endif

Sample * Loader::allocSample(const audio_sample_spec * sampleSpec, const uint8_t * data, size_t size)
{
    Sample * sample = new Sample(++mNextSampleId);
    if ( !sample ) {
        MMLOGE("no mem\n");
        return NULL;
    }

    if ( !sample->setSample(sampleSpec, data, size) ) {
        MM_RELEASE(sample);
        MMLOGE("no mem\n");
        return NULL;
    }

    return sample;
}

}

