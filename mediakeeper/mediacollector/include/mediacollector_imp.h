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

#ifndef __mediacollector_imp_H
#define __mediacollector_imp_H

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <queue>

#include <libexif/exif-data.h>
#include <libexif/exif-system.h>
#include <libexif/exif-content.h>

#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mediacollector.h>
#include <multimedia/mediametaretriever.h>

//#define COLLECTOR_IMP_TIME_PROFILING
#ifdef COLLECTOR_IMP_TIME_PROFILING
#include <multimedia/media_monitor.h>
#endif

namespace YUNOS_MM {

class PathAndStat {
  public:
    explicit PathAndStat(const char* path) : mPath(std::string(path)) {
        if (path)
            stat(path, &mStat);
    }
    std::string mPath;
    struct stat mStat;
};
typedef MMSharedPtr<PathAndStat> PathAndStatSP;

class MediaCollectorImp : public MediaCollector {
public:
    MediaCollectorImp(MediaCollectorWatcher * watcher,
                                    int thumbMaxSizeSqrt,
                                    int microThumbMaxSizeSqrt,
                                    const char * thumbPath);
    ~MediaCollectorImp();

public:
    virtual bool initCheck() const;
    virtual mm_status_t collect(PathAndStatSP item); // accept file only, dir itself is treated as file
    mm_status_t updateFileInfoToDB(); // insert/update file info to JS/database
    virtual mm_status_t collect(const char *path);
    virtual void stopCollect();
    virtual void reset();

private:
    mm_status_t collectFile(const char * path, struct stat * st);
    mm_status_t collectDir(const char * path);
    mm_status_t collectAV(const char * path);
    mm_status_t collectJPEG(const char * path);
    mm_status_t collectBMP(const char * path);
    mm_status_t collectPNG(const char * path);
    mm_status_t collectGIF(const char * path);
    mm_status_t collectWEBP(const char * path);
    mm_status_t collectWBMP(const char * path);

    mm_status_t parseJPEG(const char * path,
                        uint32_t maxSqrt,
                        uint32_t & srcWidth,
                        uint32_t & srcHeight,
                        uint32_t & outWidth,
                        uint32_t & outHeight,
                        MMBufferSP & outRGB);
    void parseExif(const char * path);
    static void exifDataForEachFunc(ExifContent *content, void *callback_data);
    static void exifContentForEachFunc(ExifEntry *entry, void *callback_data);
    void exifContentForEachFunc(ExifEntry *entry);

    bool shouldSkip(const char * path) const;
    const char * genThumbFileName();

    bool genAudioCover(const MediaMetaSP & fileMeta);
    bool genVideoThumb(int64_t duration);
    bool genVideoThumb(bool micro, VideoFrameSP & videoFrame);
    bool genPngThumb(const uint8_t * data, bool hasAlpha, uint32_t width, uint32_t height);
    bool genGifThumb(void * gifFileType, const char * jpegPath, uint32_t & thumbWidth, uint32_t & thumbHeight);
    bool genRGBThumb(const uint8_t * data, bool hasAlpha, uint32_t width, uint32_t height, bool bottomUp);

    bool rotateYV12(VideoFrameSP & videoFrame);
    bool convertYUV(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight);
    bool convertNV12(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight);
    bool convertYV12(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight);
    bool convertYV21(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight);

    void calcThumbSize(bool micro, uint32_t srcWidth, uint32_t srcHeight, uint32_t & thumbWidth, uint32_t & thumbHeight);
    void calcFactor(bool micro, uint32_t srcWidth, uint32_t srcHeight, uint32_t & num, uint32_t & den);
    bool isPicSizeExceeds(uint32_t width, uint32_t height);

private:
    MediaCollectorImp();

private:
    MediaCollectorWatcher * mWathcer;
    int mThumbMaxSizeSqrt;
    int mMicroThumbMaxSizeSqrt;
    std::string mThumbPath;
    MediaMetaRetriever * mRetriever;
    int mCurRotation;

    char * mThumbFilePath;
    char * mMicroThumbFilePath;

    // information of file/dir
    PathAndStatSP mFileStat;
    mm_status_t mCollectResult;
    class FileTagType{
      public:
        FileTagType(const char* name, const char* value) {
            mName = name;
            mValue = value;
        }
        std::string mName;
        std::string mValue;
    };
    std::queue<FileTagType> mTags;

    bool mContinue;

    // time profiling
#ifdef COLLECTOR_IMP_TIME_PROFILING
    TimeCostStaticsSP mFileTimeCost;
    TimeCostStaticsSP mJpegFileTimeCost;
    TimeCostStaticsSP mDecodeTimeCost;
    TimeCostStaticsSP mGenThumbTimeCost;
    TimeCostStaticsSP mThumb1TimeCost;
    TimeCostStaticsSP mThumb2TimeCost;
    TimeCostStaticsSP mFileDBOperation;
    #define CALCULATE_TIME_COST(tcSP) AutoTimeCost atc(*(tcSP.get()))  // not robust, the pointer isn't double checked
#else
    #define CALCULATE_TIME_COST(...)
#endif

    MM_DISALLOW_COPY(MediaCollectorImp)
    DECLARE_LOGTAG()
};
typedef MMSharedPtr<MediaCollectorImp> MediaCollectorImpSP;

}
#endif
