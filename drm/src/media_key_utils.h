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

#ifndef __media_key_utils_h
#define __media_key_utils_h

#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <log/Log.h>
#include <string.h>

namespace YUNOS_DRM {

#define DRM_DISALLOW_COPY(type) \
private: \
    type(const type &); \
    type & operator=(const type &);

class DrmLock {
public:
    DrmLock();
    ~DrmLock();

    void acquire();
    void release();
    void tryLock();

friend class DrmCondition;

private:
    pthread_mutex_t mLock;
    DRM_DISALLOW_COPY(DrmLock);
};

class DrmAutoLock {
public:
    explicit DrmAutoLock(DrmLock& lock);
    ~DrmAutoLock();

private:
    DrmLock &mLock;
    DRM_DISALLOW_COPY(DrmAutoLock);
};

class DrmCondition {
public:
    explicit DrmCondition(DrmLock& lock);
    ~DrmCondition();

    void wait();
    void signal();
    void broadcast();

    void timedWait(int64_t delayUs);
private:
    DrmLock &mLock;
    pthread_cond_t m_cond;
    DRM_DISALLOW_COPY(DrmCondition);
};

/**
 * DrmLooper is used to send client callback
 */
class DrmLooper {
public:
    DrmLooper();
    ~DrmLooper();

    void setName(const char *name);

    bool start();
    void stop();

protected:
    virtual void threadLoop() = 0;
    DrmLock mLock;
    DrmCondition mCondition;
    bool mContinue;

    std::string mName;

private:
    static void * threadfunc(void *user);

    pthread_t mThreadId;
    bool mThreadCreated;
    bool mJoinable;

    DRM_DISALLOW_COPY(DrmLooper);
};

void MessageToString(std::vector<uint8_t> &message, std::string &string);
void StringToMessage(std::string &string, std::vector<uint8_t> &message);
typedef int32_t status_t;

class DrmParam {

public:
    DrmParam();
    ~DrmParam();

    int32_t readInt32();
    status_t readInt32(int32_t *arg);

    int64_t readInt64();
    float readFloat();
    double readDouble();
    status_t read(void* outData, size_t len);
    const void* readInplace(size_t len);

    status_t writeInt32(int32_t);
    status_t writeInt64(int64_t);
    status_t writeFloat(float);
    status_t writeDouble(double);
    status_t write(const void* data, size_t len);

    status_t writeByteArray(size_t len, const uint8_t *val);

    status_t setData(const uint8_t* buffer, size_t len);
    void setDataPosition(size_t);

    const uint8_t* data() const;
    size_t dataSize() const;

private:
    uint8_t *mData;
    size_t mCapacity;
    size_t mSize;
    size_t mReadPos;

    status_t growData(size_t len);
    void reset();
    size_t roundSize(size_t len) const;

    template<class T>
    status_t writeAligned(T val);

    template<class T> T readAligned();

    template<class T> status_t readAligned(T *arg);

    DRM_DISALLOW_COPY(DrmParam);
};

} // end of namespace YUNOS_DRM

#if 0
//#define INFO(format, ...)  printf("[I][MM][%s]%s(L: %d): " format"\n", MM_LOG_TAG, __func__, __LINE__, ##__VA_ARGS__)
#define ERROR(format, ...)  printf("[E][DRM]%s(L: %d): " format"\n", __func__, __LINE__, ##__VA_ARGS__)
#define WARNING(format, ...)  printf("[W][DRM]%s(L: %d): " format"\n", __func__, __LINE__, ##__VA_ARGS__)
#define INFO(format, ...)  printf("[I][DRM]%s(L: %d): " format"\n", __func__, __LINE__, ##__VA_ARGS__)
#define DEBUG(format, ...)  printf("[D][DRM]%s(L: %d): " format"\n", __func__, __LINE__, ##__VA_ARGS__)
#else
int drm_log(int level, const char *fmt,...);

#define DRM_LOG_(level, format, ...) do {                                                       \
    const char* _log_filename = strrchr(__FILE__, '/');                                                  \
    _log_filename = (_log_filename ? (_log_filename + 1) : __FILE__);                                              \
    drm_log(level, "[MK] (%s, %s, %d)" format,  _log_filename, __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
} while(0)

#define ERROR(format, ...)  DRM_LOG_(kLogPriorityError, format, ##__VA_ARGS__);
#define WARNING(format, ...) DRM_LOG_(kLogPriorityWarn, format, ##__VA_ARGS__);
#define INFO(format, ...)    DRM_LOG_(kLogPriorityInfo, format, ##__VA_ARGS__);
#define DEBUG(format, ...)   DRM_LOG_(kLogPriorityDebug, format, ##__VA_ARGS__);
#endif

#endif //__media_key_utils_h
