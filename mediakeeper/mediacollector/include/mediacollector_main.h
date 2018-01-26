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

#ifndef __mediacollector_main_H
#define __mediacollector_main_H

#ifndef MEDIACOLLECT_SINGLE_THREAD
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>

#include <multimedia/mediacollector.h>
#include "mediacollector_imp.h"

//#define COLLECTOR_MAIN_TIME_PROFILING
#ifdef COLLECTOR_MAIN_TIME_PROFILING
#include <multimedia/media_monitor.h>
#endif

namespace YUNOS_MM {
class CollectFileThread;
class DBOperationThread;
class MediaCollectorMain : public MediaCollector {
public:
    MediaCollectorMain(MediaCollectorWatcher * watcher,
                                    int thumbMaxSizeSqrt,
                                    int microThumbMaxSizeSqrt,
                                    const char * thumbPath);
    ~MediaCollectorMain();

public:
    virtual bool initCheck() const;
    virtual mm_status_t collect(const char *path);
    virtual void stopCollect();
    virtual void reset();

private:
    MediaCollectorMain();
    mm_status_t collectFile(PathAndStatSP item);
    mm_status_t collectDir(PathAndStatSP item);
    void processAllFileList();
    void updateFileInfo2DB();

private:
    MediaCollectorWatcher * mWathcer;
    int mThumbMaxSizeSqrt;
    int mMicroThumbMaxSizeSqrt;
    std::string mThumbPath;

    Lock mLock;
    bool mCollectFileListDone;
    std::queue <MediaCollectorImpSP> mCollectorFiles;
    std::queue<PathAndStatSP> mAllFileList; // both dir and its files are treated as file
    std::queue<PathAndStatSP> mNewFileList; // files require to collect
    int32_t mRunningThreadCount;
    friend class CollectFileThread;
    #define COLLECT_THREAD_COUNT    4
    CollectFileThread* mCollectThreads[COLLECT_THREAD_COUNT];
    bool mContinue;

    // time profiling
#ifdef COLLECTOR_MAIN_TIME_PROFILING
    TimeCostStaticsSP mTotalTimeCost;
    #define CALCULATE_TIME_COST(tcSP) AutoTimeCost atc(*(tcSP.get()))  // not robust, the pointer isn't double checked
#else
    #define CALCULATE_TIME_COST(...)
#endif

    MM_DISALLOW_COPY(MediaCollectorMain)
    DECLARE_LOGTAG()
};


}

#endif
#endif
