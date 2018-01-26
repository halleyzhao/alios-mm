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

#ifndef MEDIACOLLECT_SINGLE_THREAD
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <multimedia/mmthread.h>
#include <multimedia/media_attr_str.h>
#include "mediacollector_main.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

#define NO_MEDIA_FILE_NAME ".nomedia"
#define FUNC_ENTER() do { MMLOGI("+\n"); }while(0)
#define FUNC_LEAVE() do { MMLOGD("-\n"); }while(0)

DEFINE_LOGTAG(MediaCollectorMain);

void MediaCollectorMain::processAllFileList()
{
    while (mContinue) {
        mm_status_t ret = MM_ERROR_SUCCESS;
        PathAndStatSP item;
       {
            MMAutoLock locker(mLock);
            if (!mAllFileList.empty()) {
                item = mAllFileList.front();
                mAllFileList.pop();
            } else
                break; // done & exit
        }

        if (!item) {
            MMLOGD("empty list/file");
            continue;
       }

        MMLOGD("process: %s", item->mPath.c_str());
        if ( !S_ISREG(item->mStat.st_mode) && !S_ISDIR(item->mStat.st_mode) ) {
            continue;
        }
        ret = mWathcer->fileFound( item->mPath.c_str(), item->mStat.st_mtime, item->mStat.st_size, S_ISDIR(item->mStat.st_mode));

        if ( ret != MM_ERROR_SUCCESS ) {
            MMLOGD("collect ret: %d\n", ret);
            continue;
        }
        MMLOGD("add %s to collect list", item->mPath.c_str());
        MMAutoLock locker(mLock);
        mNewFileList.push(item);
    }
    MMLOGD("process mAllFileList to mNewFileList done");
}

// all database operations run in this thread
void MediaCollectorMain::updateFileInfo2DB()
{
    while (mContinue) {
        MediaCollectorImpSP oneFile;
        {
            MMAutoLock locker(mLock);
            MMLOGD("run one cycle, size: %zu\n", mCollectorFiles.size());
            if (!mCollectorFiles.empty()) {
                oneFile = mCollectorFiles.front();
                mCollectorFiles.pop();
            }
        }
        if (oneFile) {
            oneFile->updateFileInfoToDB();
        } else {
            MMLOGD("mCollectFileListDone:%d, mAllFileList: %zu, mNewFileList: %zu, mCollectorFiles: %zu, mRunningThreadCount: %d",
                mCollectFileListDone, mAllFileList.size(), mNewFileList.size(), mCollectorFiles.size(), mRunningThreadCount);
            if (mCollectFileListDone &&
                 mAllFileList.empty() && mNewFileList.empty()
                 && mCollectorFiles.empty() && mRunningThreadCount == 0) {
                    break; // all files has been processed & exit
            }
            usleep(50000); // use sleep for now, possible optimization
        }
    }

    MMLOGD("update file info to database done");
}

class CollectFileThread : public MMThread {
  public:
    explicit CollectFileThread(MediaCollectorMain* collectorMain, const char* threadName)
        : MMThread(threadName)
        , mContinue(true)
        , mCollectorMain(collectorMain)
        {}
    ~CollectFileThread() {};

  protected:
    virtual void main();

  private:
    bool mContinue;
    MediaCollectorMain* mCollectorMain;
    DECLARE_LOGTAG()
};
DEFINE_LOGTAG(CollectFileThread);

void CollectFileThread::main()
{
#ifdef COLLECTOR_IMP_TIME_PROFILING
    MMAutoCount count (mCollectorMain->mLock, mCollectorMain->mRunningThreadCount);
#endif
    while (mCollectorMain->mContinue) {
        PathAndStatSP item;
        {
            MMAutoLock locker(mCollectorMain->mLock);
            if (!mCollectorMain->mNewFileList.empty()) {
                item = mCollectorMain->mNewFileList.front();
                mCollectorMain->mNewFileList.pop();
            }
        }

        if (!item)  {
            MMLOGD("mCollectFileListDone: %d, mAllFileList: %d, mNewFileList: %d",
                mCollectorMain->mCollectFileListDone, mCollectorMain->mAllFileList.size(), mCollectorMain->mNewFileList.size());
            if (mCollectorMain->mCollectFileListDone &&
                 mCollectorMain->mAllFileList.empty() && mCollectorMain->mNewFileList.empty()) {
                    break;
            }
            usleep(50000);  // sleep for now, possible optimization
            continue;
        }
        MMLOGD("collect file: %s", item->mPath.c_str());

        MediaCollectorImp * m;
        try {
            m = new MediaCollectorImp(mCollectorMain->mWathcer,
                                                            mCollectorMain->mThumbMaxSizeSqrt,
                                                            mCollectorMain->mMicroThumbMaxSizeSqrt,
                                                            mCollectorMain->mThumbPath.c_str());
        } catch (...) {
            MMLOGE("no mem\n");
            continue;
        }

        if ( !m->initCheck() ) {
            MMLOGE("initcheck failed\n");
            delete m;
            continue;
        }

        MediaCollectorImpSP collector(m);
        mm_status_t collectRet = collector->collect(item);
        if (collectRet != MM_ERROR_SUCCESS && collectRet != MM_ERROR_SKIPPED) {
            MMLOGE("failed to collect: %d\n", collectRet);
            continue;
        }
        MMAutoLock locker(mCollectorMain->mLock);
        MMLOGV("collect over, insert\n");
        mCollectorMain->mCollectorFiles.push(collector);
    }

    MMLOGD("thread_exit: CollectFileThread");
}

bool shouldSkip(const char * path)
{
    static const char * SKIP_LIST[] = {
        "/usr/bin",
        "/usr/lib",
//        "/etc",
        "/bin",
        "/lib",
//        "/cache",
        "/dev",
//        "/proc",
//        "/run",
        "/sbin",
    };
    static const size_t SKIP_LIST_SIZE = sizeof(SKIP_LIST)/sizeof(char*);

    for ( size_t i = 0; i < SKIP_LIST_SIZE; ++i ) {
        const char * p = path;
        const char * q = SKIP_LIST[i];
        while ( *p != '\0' && *q != '\0' && *p == *q ) {
            ++p;
            ++q;
        }
        if ( *q == '\0' ) {
            return true;
        }
    }

    return false;
}

MediaCollectorMain::MediaCollectorMain(MediaCollectorWatcher * watcher,
                                    int thumbMaxSizeSqrt,
                                    int microThumbMaxSizeSqrt,
                                    const char * thumbPath)
                                : mWathcer(watcher),
                                mThumbMaxSizeSqrt(thumbMaxSizeSqrt),
                                mMicroThumbMaxSizeSqrt(microThumbMaxSizeSqrt),
                                mThumbPath(thumbPath),
                                mCollectFileListDone(false),
                                mRunningThreadCount(0),
                                mContinue(true)
{
    FUNC_ENTER();
    int i = 0;
#ifdef COLLECTOR_MAIN_TIME_PROFILING
    std::string envStr = mm_get_env_str("mm.collector.thumb.size", "MM_COLLECTOR_THUMB_SIZE");
    int size = 0;
    if (!envStr.empty()) {
        size = atoi(envStr.c_str());
        if (size > 0)
            mThumbMaxSizeSqrt = size;
    }

    size = 0;
    envStr = mm_get_env_str("mm.collector.micro.thumb.size", "MM_COLLECTOR_MICRO_THUMB_SIZE");
    if (!envStr.empty()) {
        size = atoi(envStr.c_str());
        if (size > 0)
            mMicroThumbMaxSizeSqrt = atoi(envStr.c_str());
    }
    MMLOGD(" thumbMaxSizeSqrt: %d, microThumbMaxSizeSqrt: %d, mThumbMaxSizeSqrt: %d, mMicroThumbMaxSizeSqrt: %d",
        thumbMaxSizeSqrt, microThumbMaxSizeSqrt, mThumbMaxSizeSqrt, mMicroThumbMaxSizeSqrt);

    MMLOGD("MSDBG: thumbPath: %s, mThumbPath: %s", thumbPath, mThumbPath.c_str());
    try {
        mTotalTimeCost.reset(new TimeCostStatics("MSTC_Totoal"));
    } catch (...) {
        MMLOGE("no mem\n");
    }
#endif

    for (i=0; i<COLLECT_THREAD_COUNT; i++) {
        mCollectThreads[i] =  NULL;
    }

    FUNC_LEAVE();
}

MediaCollectorMain::~MediaCollectorMain()
{
    FUNC_ENTER();
    FUNC_LEAVE();
}

bool MediaCollectorMain::initCheck() const
{
#ifdef COLLECTOR_MAIN_TIME_PROFILING
    return mTotalTimeCost != NULL;
#else
    return true;
#endif
}

mm_status_t MediaCollectorMain::collect(const char *path)
{
    FUNC_ENTER();
    MMLOGD("collect path: %s", path);
    mm_status_t ret = MM_ERROR_SUCCESS;
    uint32_t i=0;

    CALCULATE_TIME_COST(mTotalTimeCost);
    if ( !path ) {
        MMLOGE("path is null\n");
        return MM_ERROR_INVALID_PARAM;
    }

    {
        const char * p = path + strlen(path) - 1;
        while (p > path && *p == '/') {
            --p;
        }
        while (p > path && *p != '/') {
            --p;
        }
        if (*p == '/' && *(p+1) == '.') {
            MMLOGV("hidden file, skip\n");
            return MM_ERROR_SKIPPED;
        }
    }

    mCollectFileListDone = false;

    uint32_t collectorThreadCount = 1;
    try {
        PathAndStatSP item(new PathAndStat(path));
        if ( S_ISDIR(item->mStat.st_mode))
            collectorThreadCount = COLLECT_THREAD_COUNT;
        for (i=0; i<collectorThreadCount; i++) {
            MMASSERT(!mCollectThreads[i]);
            char threadName[32];
            sprintf(threadName, "CollectorThread%02d", i);
            mCollectThreads[i] =  new CollectFileThread(this, threadName);
            if (mCollectThreads[i]->create()) {
                MMLOGE("no mem\n");
                throw 5;
            }
        }

        MMLOGD("create collector threads done");

        // create file list to collect
        if ( S_ISDIR(item->mStat.st_mode) )
            ret = collectDir(item);
        else
            ret = collectFile(item);

        MMLOGD("done for creating file list to collect");

    } catch (...) {
        MMLOGE("no mem\n");
        for (uint32_t i = 0; i < COLLECT_THREAD_COUNT; ++i) {
            if (mCollectThreads[i]) {
                mCollectThreads[i]->destroy();
                delete mCollectThreads[i];
                mCollectThreads[i] = NULL;
            }
        }
        return MM_ERROR_NO_MEM;
    }

    processAllFileList();
    mCollectFileListDone = true;
    updateFileInfo2DB();

    for (i=0; i<collectorThreadCount; i++) {
        mCollectThreads[i]->destroy();
        delete mCollectThreads[i];
        mCollectThreads[i] = NULL;
    }
    MMLOGD("collector threads done");

    {
        MMAutoLock locker(mLock);
        while (mContinue && !mCollectorFiles.empty()) {
            MMLOGD("run one cycle 2, size: %zu\n", mCollectorFiles.size());
            MediaCollectorImpSP oneFile = mCollectorFiles.front();
            mCollectorFiles.pop();
            oneFile->updateFileInfoToDB();
        }
    }

    if (!mContinue) {
        MMLOGI("user stopped\n");
    }
    MMLOGI("clean up to collect: %zu, %zu\n", mAllFileList.size(), mCollectorFiles.size());
    std::queue<PathAndStatSP> nouse;
    std::swap(mAllFileList, nouse);
    std::queue <MediaCollectorImpSP> nouse1;
    std::swap(mCollectorFiles, nouse1);
    MMLOGI("clean up to collect over: %zu, %zu\n", mAllFileList.size(), mCollectorFiles.size());

    FUNC_LEAVE();
    return ret;
}

mm_status_t MediaCollectorMain::collectFile(PathAndStatSP item)
{
    MMLOGD("add %s to the list which is possible to collect", item->mPath.c_str());
    MMAutoLock locker(mLock);
    try {
        mAllFileList.push(item);
    } catch (...) {
        MMLOGE("no mem\n");
        return MM_ERROR_NO_MEM;
    }
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaCollectorMain::collectDir(PathAndStatSP item)
{
    mm_status_t ret = MM_ERROR_SUCCESS;
    const char* path = NULL;
    if (!item)
        return ret;

    path = item->mPath.c_str();
    MMLOGD("collect path: %s", item->mPath.c_str());
    if ( shouldSkip(path) ) {
        MMLOGD("skipping dir: %s\n", path);
        return MM_ERROR_SKIPPED;
    }

    DIR * dir = opendir(path);
    if ( !dir ) {
        MMLOGE("failed to open dir: %s\n", path);
        return MM_ERROR_IO;
    }

    ret = collectFile(item); // add dir as one record as well
    struct dirent * dr;
    while ( (dr = readdir(dir)) != NULL ) {
        if (dr->d_name[0] == '.') {
            MMLOGV("skip hidden file\n");
            continue;
        }

        char file[PATH_MAX];
        snprintf(file, PATH_MAX, "%s/%s", path, NO_MEDIA_FILE_NAME);
        if ( access(file, F_OK) == 0 ) {
            MMLOGI("%s no media\n", path);
            ret = MM_ERROR_SKIPPED;
            break;
        }

        snprintf(file, PATH_MAX, "%s/%s", path, dr->d_name);
        MMLOGD("got file path: %s\n", file);
        PathAndStatSP newItem (new PathAndStat(file));
        if (!S_ISDIR(newItem->mStat.st_mode) && !S_ISREG(newItem->mStat.st_mode)) {
            MMLOGI("not dir and not reg, ignore\n");
            continue;
        }

        if ( S_ISDIR(newItem->mStat.st_mode) )
            collectDir(newItem);
        else
            collectFile(newItem);
    }

    MMLOGD("ok");
    closedir(dir);

    return ret;
}

void MediaCollectorMain::stopCollect()
{
    MMLOGI("+\n");
    mContinue = false;
}

void MediaCollectorMain::reset()
{
    MMLOGI("+\n");
    mContinue = true;
}


}

#endif
