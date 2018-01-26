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

#include "media_key_utils.h"
#include "media_key_type.h"

#include <string.h>
#include <stdlib.h>

namespace YUNOS_DRM {

/*
#define DRM_DISALLOW_COPY(type) \
private: \
    type(const type &); \
    type & operator=(const type &);
*/

DrmLock::DrmLock() {
    pthread_mutex_init(&mLock, NULL);
}

DrmLock::~DrmLock() {
   pthread_mutex_destroy(&mLock);
}

void DrmLock::acquire() {
    pthread_mutex_lock(&mLock);
}

void DrmLock::release() {
    pthread_mutex_unlock(&mLock);
}

void DrmLock::tryLock() {
   pthread_mutex_trylock(&mLock);
}

DrmAutoLock::DrmAutoLock(DrmLock& lock)
    : mLock(lock) {
    mLock.acquire();
}

DrmAutoLock::~DrmAutoLock() {
    mLock.release();
}

DrmCondition::DrmCondition(DrmLock& lock)
    :mLock(lock) {
    pthread_cond_init(&m_cond, NULL);
}

DrmCondition::~DrmCondition() {
    pthread_cond_destroy(&m_cond);
}

void DrmCondition::wait() {
    pthread_cond_wait(&m_cond, &mLock.mLock);
}

void DrmCondition::signal() {
    pthread_cond_signal(&m_cond);
}

void DrmCondition::broadcast() {
    pthread_cond_broadcast(&m_cond);
}

void DrmCondition::timedWait(int64_t delayUs) {
    timeval t;
    timespec ts;
    gettimeofday(&t, NULL);
    ts.tv_sec = t.tv_sec;
    ts.tv_nsec = t.tv_usec * 1000;
    ts.tv_sec += delayUs / 1000000;
    ts.tv_nsec +=  (delayUs % 1000000) * 1000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    pthread_cond_timedwait(&m_cond, &mLock.mLock, &ts);
}

DrmLooper::DrmLooper()
    : mCondition(mLock),
      mContinue(false),
      mThreadCreated(false),
      mJoinable(true) {
    memset(&mThreadId, 0, sizeof(mThreadId));
    mName = "DRM Looper";
}

DrmLooper::~DrmLooper() {

}

void DrmLooper::setName(const char *name) {
    mName = name;
}

bool DrmLooper::start() {
    DEBUG(">>>\n");

    DrmAutoLock lock(mLock);
    if (mThreadCreated)
        return true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, mJoinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED);

    mContinue = true;

    if ( pthread_create(&mThreadId, &attr, threadfunc, this) ) {
        ERROR("failed to start thread\n");
        pthread_attr_destroy(&attr);
        return false;
    }

    if (pthread_setname_np(mThreadId, mName.c_str()))
        WARNING("fail to set thread name, use default name");

    mThreadCreated = true;
    pthread_attr_destroy(&attr);

    INFO("start thread -%s- success\n", mName.c_str());

    DEBUG("<<<\n");
    return true;
}

void DrmLooper::stop() {
    DEBUG(">>>\n");
    if ( !mThreadCreated ) {
        DEBUG("<<<not started\n");
        return;
    }

    mThreadCreated = false;
    mContinue = false;
    void * tmp = NULL;
    INFO("joining\n");
    int ret = 0;

    mCondition.broadcast();

    if (mJoinable)
        ret = pthread_join(mThreadId, &tmp);

    DEBUG("<<< ret %d\n", ret);
}

/*static*/void* DrmLooper::threadfunc(void *user) {
    if (user == NULL) {
        WARNING("not have user data, threadfunc return");
        return NULL;
    }
    DEBUG(">>>\n");
    DrmLooper* looper = static_cast<DrmLooper*>(user);
    looper->threadLoop();
    DEBUG("<<<\n");
    return NULL;
}

void MessageToString(std::vector<uint8_t> &message, std::string &string) {
    //string.clear();

    size_t len = message.size();
    char *data = new char[len + 1];

    memcpy(data, &message[0], len);
    data[message.size()] = '\0';

    string.append(data);
    delete [] data;

#if 0
    for (size_t i = 0; i < message.size(); i++)
        string.append(1, message[i]);
#endif
}

void StringToMessage(std::string &string, std::vector<uint8_t> &message) {
    message.clear();

    message.assign((uint8_t *)string.data(), (uint8_t *)string.data() + string.size());

}

DrmParam::DrmParam() {
    reset();
}

DrmParam::~DrmParam() {
    delete mData;
    reset();
}

void DrmParam::reset() {
    mData = NULL;
    mCapacity = 0;
    mSize = 0;
    mReadPos = 0;
}

size_t DrmParam::roundSize(size_t len) const {
    return ((len + 3) & ~3);
}

status_t DrmParam::growData(size_t len) {

    if (mCapacity + len < mCapacity) {
        WARNING("DrmParam grow data fail");
        return BAD_VALUE;
    }

    mCapacity += roundSize((mSize + len)*3/2);
    void *p;

    if (mData)
        p = realloc(mData, mCapacity);
    else
        p = malloc(mCapacity);

    if (p)
        mData = static_cast<uint8_t*>(p);
    else
        WARNING("DrmParam grow data fail");

    return p == NULL ? NO_MEMORY : OK;
}

template<class T>
status_t DrmParam::writeAligned(T val) {
    if (sizeof(T) != roundSize(sizeof(T)))
        WARNING("writeAligned: type is not aligned");

    status_t status = OK;
    size_t len = roundSize(sizeof(T));

    if (len + mSize > mCapacity)
        status = growData(len);

    if (status != OK)
        return status;

    *reinterpret_cast<T*>(mData+mSize) = val;
    mSize += len;
    return OK;
}

status_t DrmParam::writeInt32(int32_t val) {
    return writeAligned(val);
}

status_t DrmParam::writeInt64(int64_t val) {

    return writeAligned(val);
}

status_t DrmParam::writeFloat(float val) {

    return writeAligned(val);
}

status_t DrmParam::writeDouble(double val) {

    return writeAligned(val);
}

template<class T>
T DrmParam::readAligned() {
    if (sizeof(T) != roundSize(sizeof(T)))
        WARNING("writeAligned: type is not aligned");

    T result;
    size_t len = roundSize(sizeof(T));

    if (mReadPos + len > mSize) {
        WARNING("read beyond data size");
        return 0;
    }

    result = *reinterpret_cast<const T*>(mData + mReadPos);
    mReadPos += len;

    return result;
}

template<class T>
status_t DrmParam::readAligned(T *arg) {
    if(roundSize(sizeof(T)) != sizeof(T))
        return BAD_VALUE;

    if ((mReadPos + sizeof(T)) <= mSize) {
        const void* data = mData + mReadPos;
        mReadPos += sizeof(T);
        *arg =  *reinterpret_cast<const T*>(data);
        return NO_ERROR;
    } else {
        return NOT_ENOUGH_DATA;
    }
}

status_t DrmParam::readInt32(int32_t *arg) {
    return readAligned(arg);
}

int32_t DrmParam::readInt32() {
    return readAligned<int32_t>();
}

int64_t DrmParam::readInt64() {
    return readAligned<int64_t>();
}

float DrmParam::readFloat() {
    return readAligned<float>();
}

double DrmParam::readDouble() {
    return readAligned<double>();
}

const uint8_t* DrmParam::data() const {
    return mData;
}

size_t DrmParam::dataSize() const {
    return mSize;
}

void DrmParam::setDataPosition(size_t pos) {
    mReadPos = pos;
}

status_t DrmParam::read(void* outData, size_t len) {
    if (mReadPos + len > mSize || len > roundSize(len) || mReadPos+roundSize(len) < mReadPos) {
        WARNING("DrmParam not enough data");
        return NOT_ENOUGH_DATA;
    }

    void *data = (mData + mReadPos);
    memcpy(outData, data, len);
    mReadPos += roundSize(len);
    return OK;
}

status_t DrmParam::write(const void* data, size_t len) {
    if (mSize + len < mSize) {
        WARNING("interge overflow");
        return BAD_VALUE;
    }

    status_t status = OK;
    if (mSize + roundSize(len) > mCapacity) {
        status = growData(len);
    }

    if (status != OK)
        return status;

    memcpy(mData+mSize, data, len);

    mSize += roundSize(len);
    return OK;
}

status_t DrmParam::writeByteArray(size_t len, const uint8_t *val) {

    status_t status = writeInt32(len);
    if (status != OK) {
        WARNING("write byte array fail");
    }

    return write(val, len);
}

status_t DrmParam::setData(const uint8_t* buffer, size_t len) {

    delete mData;
    reset();

    status_t status = growData(len);
    if (status != OK)
        return status;

    memcpy(mData, buffer, len);

    mSize = roundSize(len);

    return OK;
}

const void* DrmParam::readInplace(size_t len) {
    if ((mReadPos+roundSize(len)) >= mReadPos && (mReadPos+roundSize(len)) <= mSize
            && len <= roundSize(len)) {
        const void* data = mData+mReadPos;
        mReadPos += roundSize(len);
        return data;
    }
    return NULL;
}

const char* keyStatusToString(MediaKeyStatus status) {
    switch (status) {
    case MediaKeyUsable:
        return "MediaKey";
    case MediaKeyExpired:
        return "MediaKeyExpired";
    case MediaKeyReleased:
        return "MediaKeyReleased";
    case MediaKeyOutputRestricted:
        return "MediaKeyOutputRestricted";
    case MediaKeyOutputDownscaled:
        return "MediaKeyOutputDownscaled";
    case MediaKeyStatusPending:
        return "MediaKeyStatusPending";
    case MediaKeyInternalError:
        return "MediaKeyInternalError";
    default:
        return "MediaKeyUnknown";
    }
}

const char* keyEventToString(MediaKeyMessageEvent keyEvent) {
    switch (keyEvent) {
    case EventLicenseRequest:
        return "EventLicenseRequest";
    case EventLicenseRenewal:
        return "EventLicenseRenewal";
    case EventLicenseRelease:
        return "EventLicenseRelease";
    case EventIndividualizationRequest:
        return "EventIndividualizationRequest";
    default:
        return "MediaKeyEventUnknown";
    }
}

const char* eventToString(Event event) {
    switch (event) {
    case EventKeyStatusesChange:
        return "EventKeyStatusesChange";
    case EventError:
        return "EventError";
    default:
        return "EventUnknown";
    }
}

const char* errorToString(ErrorType error) {
    switch (error) {
    case NotSupportedError:
        return "NotSupportedError";
    case InvalidStateError:
        return "InvalidStateError";
    case TypeError:
        return "TypeError";
    case RangeError:
        return "RangeError";
    case QuotaExceedError:
        return "QuotaExceedError";
    default:
        return "ErrorUnknown";
    }
}

} // end of namespace YUNOS_DRM

static YUNOS_DRM::DrmLock lock;
const char *drmTag = "MediaKey";

int drm_log(int level, const char *fmt, ...) {
    int ret = 0;
    #define DRM_LOG_BUF_SIZE 1024

    YUNOS_DRM::DrmAutoLock locker(lock);

    va_list ap;
    char buf[DRM_LOG_BUF_SIZE];
    va_start(ap, fmt);
    vsnprintf(buf, DRM_LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    ret = yunosLogPrint(kLogIdMain, level, drmTag, "%s", buf);

    return ret;
}
