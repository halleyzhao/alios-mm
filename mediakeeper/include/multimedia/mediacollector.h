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

#ifndef __mediacollector_H
#define __mediacollector_H

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>

namespace YUNOS_MM {

class MediaCollectorWatcher {
public:
    MediaCollectorWatcher(const char *locale);
    virtual ~MediaCollectorWatcher(){}

public:
    mm_status_t addStringAttrib(const char * name, const char * tag);

public:
    // MM_ERROR_SUCCESS
    // MM_ERROR_SKIPPED
    virtual mm_status_t fileFound(const char * path,
                                                            int64_t lastModified,
                                                            int64_t fileSize,
                                                            bool isDirectory) = 0;
    virtual mm_status_t addAttrib(const char * name, const char * tag) = 0;
    virtual mm_status_t setMimeType(const char* mimeType) = 0;
    virtual mm_status_t fileDone(mm_status_t status) = 0;

protected:
    std::string mLocale;
    uint32_t mLocaleVal;
    std::string mLocalCharsetStr;

    MM_DISALLOW_COPY(MediaCollectorWatcher)
    DECLARE_LOGTAG()
};

class MediaCollector {
public:
    static MediaCollector * create(MediaCollectorWatcher * watcher,
                    int thumbMaxSizeSqrt,
                    int microThumbMaxSizeSqrt,
                    const char * thumbPath);
    static void destroy(MediaCollector * scanner);

public:
    virtual ~MediaCollector();

public:
    virtual bool initCheck() const = 0;
    virtual mm_status_t collect(const char *path) = 0;
    virtual void stopCollect() = 0;
    virtual void reset() = 0;

protected:
    MediaCollector();

    MM_DISALLOW_COPY(MediaCollector)
    DECLARE_LOGTAG()
};

}

#endif

