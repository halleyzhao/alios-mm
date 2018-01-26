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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "jpeglib.h"
#include "turbojpeg.h"
#include <setjmp.h>
#include <png.h>
#include <multimedia/bmp.h>
#include <multimedia/wbmp.h>
#include <gif_lib.h>
#include <webp/decode.h>
extern "C" {
#include <libswscale/swscale.h>
}

#include <multimedia/media_attr_str.h>
#include "mediacollector_imp.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

#define FUNC_ENTER() do { MMLOGI("+\n"); }while(0)
#define FUNC_LEAVE() do { MMLOGV("-\n"); }while(0)

#define NO_MEDIA_FILE_NAME ".nomedia"

#define MICRO_THUMB_FILE_PRFIX "._"
#define THUMB_FILE_PRFIX "."

#ifdef MEDIACOLLECT_SINGLE_THREAD
    #define RELEASE_RETRIEVER()
#else
    #define RELEASE_RETRIEVER() do {\
        MediaMetaRetriever::destroy(mRetriever);\
        mRetriever = NULL;\
    }while (0)
#endif


#ifdef MEDIACOLLECT_SINGLE_THREAD
#define ADD_ATTRIB mWathcer->addAttrib
#define ADD_STRING_ATTRIB mWathcer->addStringAttrib
#else
#define ADD_ATTRIB(_tag, _val) mTags.push(FileTagType(_tag, _val))
#define ADD_STRING_ATTRIB(_tag, _val) mTags.push(FileTagType(_tag, _val))
#endif

#define REPORT_BASE(_w, _h, _mime) do {\
    char s[32];\
    snprintf(s, 32, "%d", _w);\
    ADD_ATTRIB(MEDIA_ATTR_WIDTH, s);\
    snprintf(s, 32, "%d", _h);\
    ADD_ATTRIB(MEDIA_ATTR_HEIGHT, s);\
    ADD_ATTRIB(MEDIA_ATTR_MIME, _mime);\
}while(0)


enum FileType {
    kFileTypeUnknown,
    kFileTypeAV,
    kFileTypeJPEG,
    kFileTypeBMP,
    kFileTypePNG,
    kFileTypeGIF,
    kFileTypeWEBP,
    kFileTypeWBMP
};

#define TRY_FILE_TYPE(_exts, _count, _type) do {\
    for (size_t i = 0; i < _count; ++i) {\
        if (!strcasecmp(extension, _exts[i])) {\
            return _type;\
        }\
    }\
}while(0)


struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */

    jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

void my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


static FileType checkFileType(const char *extension) {
    static const char * AV_EXTENTIONS[] = {
        ".mp3", ".mp4", ".m4a", ".3gp", ".3gpp", ".3g2", ".3gpp2",
        ".mpeg", ".ogg", ".mid", ".smf", ".imy", ".wma", ".aac",
        ".wav", ".amr", ".midi", ".xmf", ".rtttl", ".rtx", ".ota",
        ".mkv", ".mka", ".webm", ".ts", /*".fl",*/ ".flac", ".mxmf",
        ".wma", ".avi", ".mpeg", ".mpg", ".awb", ".mpga", ".mov", ".m4v"
    };
    static const size_t AV_EXTENTIONS_COUNT =
        sizeof(AV_EXTENTIONS) / sizeof(AV_EXTENTIONS[0]);

    static const char * JPEG_EXTENSIONS[] = {
        ".JPG", ".JPEG"
    };
    static const size_t JPEG_EXTENSIONS_COUNT =
        sizeof(JPEG_EXTENSIONS) / sizeof(JPEG_EXTENSIONS[0]);

    static const char * BMP_EXTENSIONS[] = {
        ".BMP"
    };
    static const size_t BMP_EXTENSIONS_COUNT =
        sizeof(BMP_EXTENSIONS) / sizeof(BMP_EXTENSIONS[0]);

    static const char * PNG_EXTENSIONS[] = {
        ".PNG"
    };
    static const size_t PNG_EXTENSIONS_COUNT =
        sizeof(PNG_EXTENSIONS) / sizeof(PNG_EXTENSIONS[0]);

    static const char * GIF_EXTENSIONS[] = {
        ".GIF"
    };
    static const size_t GIF_EXTENSIONS_COUNT =
        sizeof(GIF_EXTENSIONS) / sizeof(GIF_EXTENSIONS[0]);

    static const char * WEBP_EXTENSIONS[] = {
        ".WEBP"
    };
    static const size_t WEBP_EXTENSIONS_COUNT =
        sizeof(WEBP_EXTENSIONS) / sizeof(WEBP_EXTENSIONS[0]);

    static const char * WBMP_EXTENSIONS[] = {
        ".WBMP"
    };
    static const size_t WBMP_EXTENSIONS_COUNT =
        sizeof(WBMP_EXTENSIONS) / sizeof(WBMP_EXTENSIONS[0]);


    TRY_FILE_TYPE(AV_EXTENTIONS, AV_EXTENTIONS_COUNT, kFileTypeAV);
    TRY_FILE_TYPE(JPEG_EXTENSIONS, JPEG_EXTENSIONS_COUNT, kFileTypeJPEG);
    TRY_FILE_TYPE(BMP_EXTENSIONS, BMP_EXTENSIONS_COUNT, kFileTypeBMP);
    TRY_FILE_TYPE(PNG_EXTENSIONS, PNG_EXTENSIONS_COUNT, kFileTypePNG);
    TRY_FILE_TYPE(GIF_EXTENSIONS, GIF_EXTENSIONS_COUNT, kFileTypeGIF);
    TRY_FILE_TYPE(WEBP_EXTENSIONS, WEBP_EXTENSIONS_COUNT, kFileTypeWEBP);
    TRY_FILE_TYPE(WBMP_EXTENSIONS, WBMP_EXTENSIONS_COUNT, kFileTypeWBMP);

    return kFileTypeUnknown;
}

DEFINE_LOGTAG(MediaCollectorImp)

bool MediaCollectorImp::shouldSkip(const char * path) const
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

MediaCollectorImp::MediaCollectorImp(MediaCollectorWatcher * watcher,
                                    int thumbMaxSizeSqrt,
                                    int microThumbMaxSizeSqrt,
                                    const char * thumbPath)
                                : mWathcer(watcher),
                                mThumbMaxSizeSqrt(thumbMaxSizeSqrt),
                                mMicroThumbMaxSizeSqrt(microThumbMaxSizeSqrt),
                                mThumbPath(thumbPath),
                                mRetriever(NULL),
                                mCurRotation(0),
                                mThumbFilePath(NULL),
                                mMicroThumbFilePath(NULL),
                                mCollectResult(MM_ERROR_SUCCESS),
                                mContinue(true)
{
    FUNC_ENTER();
#ifdef COLLECTOR_IMP_TIME_PROFILING
    mFileTimeCost.reset(new TimeCostStatics("MSTC_File"));
    mJpegFileTimeCost.reset(new TimeCostStatics("MSTC_JpegFile"));
    mDecodeTimeCost.reset(new TimeCostStatics("MSTC_Decode"));
    mGenThumbTimeCost.reset(new TimeCostStatics("MSTC_GenThumb"));
    mThumb1TimeCost.reset(new TimeCostStatics("MSTC_Thumb1"));
    mThumb2TimeCost.reset(new TimeCostStatics("MSTC_Thumb2"));
    mFileDBOperation.reset(new TimeCostStatics("MSTC_DBOp"));
#endif
    mThumbFilePath = new char[PATH_MAX + 1];
    mMicroThumbFilePath = new char[PATH_MAX + 1];
    FUNC_LEAVE();
}

MediaCollectorImp::~MediaCollectorImp()
{
    FUNC_ENTER();
    if ( mRetriever ) {
        MediaMetaRetriever::destroy(mRetriever);
        mRetriever = NULL;
    }

    MM_RELEASE_ARRAY(mThumbFilePath);
    MM_RELEASE_ARRAY(mMicroThumbFilePath);
    FUNC_LEAVE();
}

bool MediaCollectorImp::initCheck() const
{
    return (mThumbFilePath && mMicroThumbFilePath);
}

mm_status_t MediaCollectorImp::collect(PathAndStatSP item)
{
    FUNC_ENTER();
    const char* path = NULL;
    CALCULATE_TIME_COST(mFileTimeCost);

    if ( !initCheck() ) {
        MMLOGE("not inited\n");
        mCollectResult = MM_ERROR_INVALID_STATE;
        return MM_ERROR_INVALID_STATE;
    }
    MMASSERT(item != NULL);
    path = item->mPath.c_str();

    MMLOGD("collect path: %s\n", path);
    mm_status_t ret;
    FileType ftype;

    mFileStat = item;
    ftype = kFileTypeUnknown;
    if ( S_ISREG(item->mStat.st_mode) ) {
        const char *extension = strrchr(path, '.');
        if (extension) {
            ftype = checkFileType(extension);
        }
    }

    mCurRotation = 0;
    switch ( ftype ) {
        case kFileTypeAV:
            ret = collectAV(path);
            break;
        case kFileTypeJPEG:
            ret = collectJPEG(path);
            break;
        case kFileTypePNG:
            ret = collectPNG(path);
            break;
        case kFileTypeBMP:
            ret = collectBMP(path);
            break;
        case kFileTypeGIF:
            ret = collectGIF(path);
            break;
        case kFileTypeWEBP:
            ret = collectWEBP(path);
            break;
        case kFileTypeWBMP:
            ret = collectWBMP(path);
            break;
        default:
            ret = MM_ERROR_SKIPPED;
            MMLOGV("not supported now\n");
            break;
    }

    if (ret != MM_ERROR_SUCCESS) {
        if (mThumbFilePath[0] != '\0') {
            remove(mThumbFilePath);
            remove(mMicroThumbFilePath);
            mThumbFilePath[0] = '\0';
            mMicroThumbFilePath[0] = '\0';
        }
    }

    mCollectResult = ret;

    FUNC_LEAVE();
    return ret;
}

mm_status_t MediaCollectorImp::collect(const char *path)
{
    FUNC_ENTER();
    CALCULATE_TIME_COST(mTotalTimeCost);
    if ( !path ) {
        MMLOGE("path is null\n");
        return MM_ERROR_INVALID_PARAM;
    }
    if ( !initCheck() ) {
        MMLOGE("not inited\n");
        return MM_ERROR_INVALID_STATE;
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

    struct stat st;
    stat(path,&st);
    if ( S_ISDIR(st.st_mode) ) {
        return collectDir(path);
    }

    FUNC_LEAVE();
    return collectFile(path, &st);
}

mm_status_t MediaCollectorImp::collectFile(const char * path, struct stat * st)
{
    CALCULATE_TIME_COST(mFileTimeCost);
    FUNC_ENTER();
    MMLOGV("path: %s\n", path);
    mm_status_t ret;
    FileType ftype;

    uint64_t modifyTime = st->st_mtim.tv_sec * 1000LL + st->st_mtim.tv_nsec / 1000000LL;
    MMLOGV("modifyTime: %" PRId64 "\n", modifyTime);
    { // for JS/DataBase operation 1
    CALCULATE_TIME_COST(mFileDBOperation1);
    if ( S_ISREG(st->st_mode) ) {
        const char *extension = strrchr(path, '.');
        if (extension) {
            ftype = checkFileType(extension);
        } else {
            ftype = kFileTypeUnknown;
        }

        ret = mWathcer->fileFound(path, modifyTime, st->st_size, false);
    } else if ( S_ISDIR(st->st_mode) ) {
        ftype = kFileTypeUnknown;
        ret = mWathcer->fileFound(path, modifyTime, st->st_size, true);
    } else {
        MMLOGE("stat() failed for %s: %s", path, strerror(errno) );
        return MM_ERROR_SKIPPED;
    }

    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGV("collect ret: %d\n", ret);
        return ret;
    }
    } // end of JS/DataBase operation 1

    mCurRotation = 0;
    switch ( ftype ) {
        case kFileTypeAV:
            ret = collectAV(path);
            break;
        case kFileTypeJPEG:
            ret = collectJPEG(path);
            break;
        case kFileTypePNG:
            ret = collectPNG(path);
            break;
        case kFileTypeBMP:
            ret = collectBMP(path);
            break;
        case kFileTypeGIF:
            ret = collectGIF(path);
            break;
        case kFileTypeWEBP:
            ret = collectWEBP(path);
            break;
        case kFileTypeWBMP:
            ret = collectWBMP(path);
            break;
        default:
            ret = MM_ERROR_SKIPPED;
            MMLOGV("not supported now\n");
            break;
    }

    if (ret != MM_ERROR_SUCCESS) {
        if (mThumbFilePath[0] != '\0') {
            remove(mThumbFilePath);
            remove(mMicroThumbFilePath);
            mThumbFilePath[0] = '\0';
            mMicroThumbFilePath[0] = '\0';
        }
    }

    { // for JS/DataBase operation 2
    CALCULATE_TIME_COST(mFileDBOperation2);
    ret = mWathcer->fileDone(ret);
    if (ret != MM_ERROR_SUCCESS) {
        MMLOGE("fileend failed, rm thum\n");
        if (mThumbFilePath[0] != '\0') {
            remove(mThumbFilePath);
            remove(mMicroThumbFilePath);
            mThumbFilePath[0] = '\0';
            mMicroThumbFilePath[0] = '\0';
        }
    } else {
        mThumbFilePath[0] = '\0';
        mMicroThumbFilePath[0] = '\0';
    }
    } // for JS/DataBase operation 2

    if (ret != MM_ERROR_SUCCESS) {
        ret = MM_ERROR_SKIPPED;
    }

    FUNC_LEAVE();
    return ret;
}

mm_status_t MediaCollectorImp::collectDir(const char * path)
{
    CALCULATE_TIME_COST(mDirTimeCost);
    MMASSERT(path != NULL);
    if ( shouldSkip(path) ) {
        MMLOGD("skipping dir: %s\n", path);
        return MM_ERROR_SKIPPED;
    }

    MMLOGV("collectning dir: %s\n", path);
    DIR * dir = opendir(path);
    if ( !dir ) {
        MMLOGE("failed to open dir: %s\n", path);
        return MM_ERROR_IO;
    }

    struct dirent * dr;
    while ( (dr = readdir(dir)) != NULL && mContinue) {
        if ( !strcmp(dr->d_name, ".") || !strcmp(dr->d_name, "..") ) {
            continue;
        }
        if (dr->d_name[0] == '.') {
            MMLOGV("skip hidden file\n");
            continue;
        }

        char file[PATH_MAX];
        snprintf(file, PATH_MAX, "%s/%s", path, NO_MEDIA_FILE_NAME);
        if ( access(file, F_OK) == 0 ) {
            MMLOGI("%s no media\n", path);
            closedir(dir);
            return MM_ERROR_SUCCESS;
        }

        snprintf(file, PATH_MAX, "%s/%s", path, dr->d_name);
        MMLOGV("got file path: %s\n", file);
        struct stat st;
        stat(file,&st);
        if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) {
            MMLOGI("not dir and not reg, ignore\n");
            continue;
        }

        collectFile(file, &st);

        if ( S_ISDIR(st.st_mode) )
            collectDir(file);
    }

    MMLOGV("ok");
    closedir(dir);
    return MM_ERROR_SUCCESS;
}

#define META_SEARCH_BEGIN(_meta)\
    if ( _meta ) {\
        for ( MediaMeta::iterator i = _meta->begin(); i != _meta->end(); ++i ) {\
            const MediaMeta::MetaItem & item = *i;\
            const char * key = item.mName;

#define META_SEARCH_STR(_name)\
        if ( !strcasecmp(_name, key) ) {\
            MMLOGV("name: %s, val: %s\n", key, item.mValue.str);\
            ADD_STRING_ATTRIB(_name, item.mValue.str);\
            continue;\
        }

#define META_SEARCH_I32(_name)\
        if ( !strcasecmp(_name, key) ) {\
            MMLOGV("name: %s, val: %d\n", key, item.mValue.ii);\
            char s[32];\
            snprintf(s, 32, "%d", item.mValue.ii);\
            ADD_ATTRIB(_name, s);\
            continue;\
        }

#define META_SEARCH_I32_1(_name, _save)\
        if ( !strcasecmp(_name, key) ) {\
            MMLOGV("name: %s, val: %d\n", key, item.mValue.ii);\
            char s[32];\
            snprintf(s, 32, "%d", item.mValue.ii);\
            ADD_ATTRIB(_name, s);\
            _save = item.mValue.ii;\
            continue;\
        }

#define META_SEARCH_I64(_name)\
        if ( !strcasecmp(_name, key) ) {\
            MMLOGV("name: %s, val: %" PRId64 "\n", key, item.mValue.ld);\
            char s[32];\
            snprintf(s, 32, "%" PRId64, item.mValue.ld);\
            ADD_ATTRIB(_name, s);\
            continue;\
        }

#define META_SEARCH_I64_1(_name, _save)\
        if ( !strcasecmp(_name, key) ) {\
            MMLOGV("name: %s, val: %" PRId64 "\n", key, item.mValue.ld);\
            char s[32];\
            snprintf(s, 32, "%" PRId64, item.mValue.ld);\
            ADD_ATTRIB(_name, s);\
            _save = item.mValue.ld;\
            continue;\
        }

#define META_SEARCH_END()\
        }\
    }

#define REPORT_THUMB(_path, _w, _h) do {\
        ADD_ATTRIB(MEDIA_ATTR_THUMB_PATH, _path);\
        char s[32];\
        snprintf(s, 32, "%u", _w);\
        ADD_ATTRIB(MEDIA_ATTR_THUMB_WIDTH, s);\
        snprintf(s, 32, "%u", _h);\
        ADD_ATTRIB(MEDIA_ATTR_THUMB_HEIGHT, s);\
}while(0)

#define REPORT_MICROTHUMB(_path, _w, _h) do {\
        ADD_ATTRIB(MEDIA_ATTR_MICROTHUMB_PATH, _path);\
        char s[32];\
        snprintf(s, 32, "%u", _w);\
        ADD_ATTRIB(MEDIA_ATTR_MICROTHUMB_WIDTH, s);\
        snprintf(s, 32, "%u", _h);\
        ADD_ATTRIB(MEDIA_ATTR_MICROTHUMB_HEIGHT, s);\
}while(0)

mm_status_t MediaCollectorImp::collectAV(const char * path)
{
    FUNC_ENTER();
    if (!mRetriever)
        mRetriever = MediaMetaRetriever::create();
    if (!mRetriever) {
        MMLOGE("fail to create MediaMetaRetriever");
        return MM_ERROR_SKIPPED;
    }
    MMASSERT(mWathcer != NULL);
    mm_status_t ret = mRetriever->setDataSource(path);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGV("failed to setdatasource\n");
        RELEASE_RETRIEVER();
        return MM_ERROR_SKIPPED;
    }

    MediaMetaSP fileMeta;
    MediaMetaSP audioMeta;
    MediaMetaSP videoMeta;
    ret = mRetriever->extractMetadata(fileMeta, audioMeta, videoMeta);
    if ( ret != MM_ERROR_SUCCESS ) {
        MMLOGV("failed to extractMetadata\n");
        RELEASE_RETRIEVER();
        return MM_ERROR_SKIPPED;
    }

    if ( !fileMeta ) {
        MMLOGV("no file meta\n");
        RELEASE_RETRIEVER();
        return MM_ERROR_SKIPPED;
    }

    bool hasAudio = false;
    bool hasVideo = false;
    int64_t duration = 0;

    META_SEARCH_BEGIN(fileMeta)
        META_SEARCH_STR(MEDIA_ATTR_ALBUM)
        META_SEARCH_STR(MEDIA_ATTR_HAS_COVER)
        META_SEARCH_STR(MEDIA_ATTR_AUTHOR)
        META_SEARCH_STR(MEDIA_ATTR_COMPOSER)
        META_SEARCH_STR(MEDIA_ATTR_GENRE)
        META_SEARCH_STR(MEDIA_ATTR_TITLE)
        META_SEARCH_STR(MEDIA_ATTR_YEAR)
        META_SEARCH_STR(MEDIA_ATTR_DATE)
        META_SEARCH_I64_1(MEDIA_ATTR_DURATION, duration)
        META_SEARCH_STR(MEDIA_ATTR_WRITER)
        META_SEARCH_STR(MEDIA_ATTR_ALBUM_ARTIST)
        META_SEARCH_STR("artist")
        if ( !strcmp(MEDIA_ATTR_HAS_AUDIO, key) ) {
            MMLOGV("name: %s, val: %s\n", key, item.mValue.str);
            if (!strcmp(item.mValue.str, "y")) {
                hasAudio = true;
            }
            ADD_ATTRIB(key, item.mValue.str);
            continue;
        }
        if ( !strcmp(MEDIA_ATTR_HAS_VIDEO, key) ) {
            MMLOGV("name: %s, val: %s\n", key, item.mValue.str);
            if (!strcmp(item.mValue.str, "y")) {
                hasVideo = true;
            }
            ADD_ATTRIB(key, item.mValue.str);
            continue;
        }
        META_SEARCH_STR(MEDIA_ATTR_LOCATION)
        META_SEARCH_STR(MEDIA_ATTR_FILE_CREATION_TIME)
        META_SEARCH_STR(MEDIA_ATTR_CATEGORY)
        META_SEARCH_STR(MEDIA_ATTR_MIME)
    META_SEARCH_END()

    META_SEARCH_BEGIN(audioMeta)
        META_SEARCH_I32(MEDIA_ATTR_BIT_RATE)
    META_SEARCH_END()

    META_SEARCH_BEGIN(videoMeta)
        META_SEARCH_I32(MEDIA_ATTR_WIDTH)
        META_SEARCH_I32(MEDIA_ATTR_HEIGHT)
        if (!strcmp(MEDIA_ATTR_ROTATION, key)) {
            MMLOGV("name: %s, val: %d\n", key, item.mValue.ii);
            char s[32];
            snprintf(s, 32, "%d", item.mValue.ii);
            ADD_ATTRIB(key, s);
            mCurRotation = item.mValue.ii;
            continue;
        }
        META_SEARCH_STR(MEDIA_ATTR_LANGUAGE)
    META_SEARCH_END()

    if (hasVideo) {
        genVideoThumb(duration * 1000LL);
    } else if (hasAudio) {
        genAudioCover(fileMeta);
    }

    FUNC_LEAVE();
    RELEASE_RETRIEVER();
    return MM_ERROR_SUCCESS;
}

#define OPEN_WRITE_FILE_MAX_RETRY 5
#define OPEN_WRITE_FILE_RETRY_INT 200000

#define OPEN_WRITE_FILE(_file, _fname) do {\
    int retryTimes = OPEN_WRITE_FILE_MAX_RETRY;\
    while (retryTimes-- > 0) {\
        if ((_file = fopen(_fname, "wb")) == NULL) {\
            MMLOGE("failed to open %s, err: %d(%s), retry\n", _fname, errno, strerror(errno));\
            usleep(OPEN_WRITE_FILE_RETRY_INT);\
            continue;\
        }\
        MMLOGV("success open: %s\n", _fname);\
        break;\
    }\
}while(0)

bool MediaCollectorImp::genAudioCover(const MediaMetaSP & fileMeta)
{
    const char * coverMime;
    if (!fileMeta->getString(MEDIA_ATTR_ATTACHEDPIC_MIME, coverMime)) {
        MMLOGV("no cover mime\n");
        return false;
    }

    if (!strstr(coverMime, "jpeg")) {
        MMLOGV("unsupported cover mime: %s\n", coverMime);
        return false;
    }

    if (!mRetriever)
        mRetriever = MediaMetaRetriever::create();
    if (!mRetriever) {
         MMLOGE("fail to create MediaMetaRetriever");
         return MM_ERROR_SKIPPED;
     }

    CoverSP cover = mRetriever->extractCover();
    if (!cover) {
        MMLOGV("no cover\n");
        RELEASE_RETRIEVER();
        return false;
    }

    genThumbFileName();
    FILE * thumbFile;
    OPEN_WRITE_FILE(thumbFile, mThumbFilePath);
    if (!thumbFile) {
        MMLOGE("can't open thumb file\n");
        RELEASE_RETRIEVER();
        return false;
    }

    if (fwrite(cover->mData->getBuffer(), 1, cover->mData->getActualSize(), thumbFile) != cover->mData->getActualSize()) {
        MMLOGE("failed to write thumb\n");
        fclose(thumbFile);
        remove(mThumbFilePath);
        RELEASE_RETRIEVER();
        return false;
    }
    fclose(thumbFile);

    ADD_ATTRIB(MEDIA_ATTR_COVER_PATH, mThumbFilePath);
    RELEASE_RETRIEVER();
    return true;
}

void MediaCollectorImp::calcThumbSize(bool micro, uint32_t srcWidth, uint32_t srcHeight, uint32_t & thumbWidth, uint32_t & thumbHeight)
{
    int sz = micro ? mMicroThumbMaxSizeSqrt : mThumbMaxSizeSqrt;
    if ((int)srcWidth <= sz && (int)srcHeight <= sz) {
        thumbWidth = srcWidth;
        thumbHeight = srcHeight;
        return;
    }

    uint32_t scaleNum = sz;
    uint32_t scaleDemon;
    if (srcWidth > srcHeight) {
        scaleDemon = srcWidth;
    } else {
        scaleDemon = srcHeight;
    }

    double f = (double)scaleNum / scaleDemon;
    thumbWidth = srcWidth * f;
    thumbHeight = srcHeight * f;
    thumbWidth = (thumbWidth + 7) / 8 * 8;
    thumbHeight = (thumbHeight + 7) / 8 * 8;
    MMLOGV("f: %lf, thumb w: %u, h: %u\n", f, thumbWidth, thumbHeight);
    if (thumbWidth & 1) {
        ++thumbWidth;
        MMLOGV("f: %lf, thumb w: %u, h: %u\n", f, thumbWidth, thumbHeight);
    }
}

void MediaCollectorImp::calcFactor(bool micro, uint32_t srcWidth, uint32_t srcHeight, uint32_t & num, uint32_t & den)
{
    int sz = micro ? mMicroThumbMaxSizeSqrt : mThumbMaxSizeSqrt;
    if ((int)srcWidth <= sz && (int)srcHeight <= sz) {
        num = 1;
        den = 1;
        return;
    }

    num = sz;
    if (srcWidth > srcHeight) {
        den = srcWidth;
    } else {
        den = srcHeight;
    }
    MMLOGV("num: %u, den: %u\n", num, den);
}

bool MediaCollectorImp::genVideoThumb(int64_t duration)
{
    MMLOGV("dur: %" PRId64 "\n", duration);

    if (!mRetriever)
        mRetriever = MediaMetaRetriever::create();
    if (!mRetriever) {
        MMLOGE("failed to create retriever\n");
        return false;
    }

    int32_t memCosts = -1;
    VideoFrameSP videoFrame = mRetriever->extractVideoFrame(duration > (int64_t)5000000 ? duration / 3 : 0, &memCosts);
    if (!videoFrame) {
        MMLOGE("failed to extract video frame\n");
        RELEASE_RETRIEVER();
        return false;
    }

    MMLOGV("video frame: size: %zu, width: %d, height: %d, colorformat: %d, RotationAngle: %d, stride: %d\n",
        videoFrame->mData->getActualSize(),
        videoFrame->mWidth,
        videoFrame->mHeight,
        videoFrame->mColorFormat,
        videoFrame->mRotationAngle,
        videoFrame->mStride);

    if (videoFrame->mStride == 0) {
        MMLOGW("no stride provided, try set to width\n");
        videoFrame->mStride = videoFrame->mWidth;
    }
    if (videoFrame->mColorFormat == -1) {
        MMLOGW("no colorformat provided, try set to YV12\n");
        videoFrame->mColorFormat = 'YV12';
    }
    if (videoFrame->mRotationAngle == -1) {
        MMLOGW("no rotation provided, try set to source provided: %d\n", mCurRotation);
        videoFrame->mRotationAngle = mCurRotation;
    }

#if 0
    {
        char name[64];
        sprintf(name, "/data/b.yuv");
        DataDump dumper(name);
        dumper.dump(videoFrame->mData->getWritableBuffer(), videoFrame->mData->getActualSize());
    }
#endif

    if (memCosts > 0) {
        MMLOGV("adding codec-memory-cost: %d\n", memCosts);
        char s[64];
        sprintf(s, "%d", memCosts);
        ADD_ATTRIB("codec-memory-cost", s);
    } else {
        MMLOGV("no codec-memory-cost");
    }
    genThumbFileName();
    genVideoThumb(false, videoFrame);
    genVideoThumb(true, videoFrame);

    MMLOGV("+\n");
    RELEASE_RETRIEVER();
    return true;
}

bool MediaCollectorImp::genVideoThumb(bool micro, VideoFrameSP & videoFrame)
{
    const char * thumbPath;
    if (micro) {
        thumbPath = mMicroThumbFilePath;
    } else {
        thumbPath = mThumbFilePath;
    }

    uint32_t thumbWidth;
    uint32_t thumbHeight;
    calcThumbSize(micro, videoFrame->mWidth, videoFrame->mHeight, thumbWidth, thumbHeight);

    if (!convertYUV(videoFrame,
        thumbWidth,
        thumbHeight)) {
        MMLOGE("failed to convert color\n");
        return false;
    }

    rotateYV12(videoFrame);

    tjhandle handle = tjInitCompress();
    if (!handle) {
        MMLOGE("failed to init tj\n");
        return false;
    }

    int flags = TJFLAG_BOTTOMUP;

    unsigned char * jpegBuffer = NULL;
    unsigned long jpegSize = 0;

    if (tjCompressFromYUV(handle,
            videoFrame->mData->getWritableBuffer(),
            videoFrame->mWidth,
            (videoFrame->mStride - videoFrame->mWidth + 1) % 4,
            videoFrame->mHeight,
            TJSAMP_420,
            &jpegBuffer,
            &jpegSize,
            100,
            flags) == -1) {
        MMLOGE("failed to compress\n");
        tjDestroy(handle);
        handle = NULL;
        return false;
    }

    MMLOGV("compress success, size: %u\n", jpegSize);
    FILE * thumbFile;
    OPEN_WRITE_FILE(thumbFile, thumbPath);
    if (!thumbFile) {
        MMLOGE("can't open thumb file\n");
        tjDestroy(handle);
        handle = NULL;
        tjFree(jpegBuffer);
        return false;
    }

    if (fwrite(jpegBuffer, 1, jpegSize, thumbFile) != jpegSize) {
        MMLOGE("failed to write file\n");
        fclose(thumbFile);
        thumbFile = NULL;
        tjDestroy(handle);
        handle = NULL;
        remove(thumbPath);
        tjFree(jpegBuffer);
        return false;
    }

    fclose(thumbFile);
    thumbFile = NULL;
    tjDestroy(handle);
    handle = NULL;

    if (micro) {
        REPORT_MICROTHUMB(thumbPath, thumbWidth, thumbHeight);
    } else {
        REPORT_THUMB(thumbPath, thumbWidth, thumbHeight);
    }
    tjFree(jpegBuffer);
    return true;
}

/*
bool MediaCollectorImp::rotateNV12(VideoFrameSP & videoFrame)
{
    const MMBufferSP & src = videoFrame->mData;
    MMBufferSP dst = MMBuffer::create(src->getActualSize());
    if (!dst) {
        MMLOGE("no mem\n");
        return false;
    }
    switch (videoFrame->mRotationAngle) {
        case 0:
            MMLOGV("not need\n");
            return true;
        case 90:
        case -270:
            {
                const uint8_t * src_p = src->getBuffer();
                uint8_t * dst_p = dst->getWritableBuffer();
                for (int32_t i = 0; i < videoFrame->mWidth; ++i) {
                    for (int32_t j = videoFrame->mHeight - 1; j >= 0; --j) {
                        *(dst_p++) = src_p[videoFrame->mWidth * j + i];
                    }
                }

                int32_t wh = videoFrame->mWidth * videoFrame->mHeight;
                src_p += wh;
                int32_t h2 = videoFrame->mHeight >> 1;
                for(int i = 0; i < videoFrame->mWidth; i += 2){
                    for(int j = h2 - 1; j >= 0; --j) {
                        int a = videoFrame->mWidth * j + i;
                        *(dst_p++) = src_p[a];
                        *(dst_p++) = src_p[a + 1];
                    }
                }
            }
            break;
        default:
            MMLOGE("unsupported angle: %d\n", videoFrame->mRotationAngle);
            return false;
    }

    int32_t tmp = videoFrame->mHeight;
    videoFrame->mHeight = videoFrame->mWidth;
    videoFrame->mWidth = tmp;
    videoFrame->mStride = videoFrame->mWidth;
    dst->setActualSize(src->getActualSize());
    MMLOGV("size: %zu\n", dst->getActualSize());
    videoFrame->mData = dst;
    videoFrame->mRotationAngle = 0;

    return true;
}
*/

bool MediaCollectorImp::rotateYV12(VideoFrameSP & videoFrame)
{
    if (0 == videoFrame->mRotationAngle) {
        MMLOGV("not need\n");
        return true;
    }

    const MMBufferSP & src = videoFrame->mData;
    MMBufferSP dst = MMBuffer::create(src->getActualSize());
    if (!dst) {
        MMLOGE("no mem\n");
        return false;
    }

    if (videoFrame->mStride != videoFrame->mWidth) {
        MMLOGW("direct yv12 not tested\n");
    }

    switch (videoFrame->mRotationAngle) {
        case 90:
        case -270:
            {
                MMLOGV("90\n");
                const uint8_t * src_p = src->getBuffer();
                uint8_t * dst_p = dst->getWritableBuffer();
                for (int32_t i = 0; i < videoFrame->mWidth; ++i) {
                    int a = videoFrame->mStride * (videoFrame->mHeight - 1);
                    for (int32_t j = videoFrame->mHeight - 1; j >= 0; --j, a -= videoFrame->mStride) {
                        *(dst_p++) = src_p[a + i];
                    }
                }

                int32_t wh = videoFrame->mStride * videoFrame->mHeight;
                int32_t w2 = videoFrame->mWidth >> 1; //
                int32_t h2 = videoFrame->mHeight >> 1;
                const uint8_t * src_u = src_p + wh;
                const uint8_t * src_v = src_u + (wh >> 2);
                uint8_t * dst_u = dst_p;
                uint8_t * dst_v = dst_u + (wh >> 2);
                for(int i = 0; i < w2; ++i) {
                    int b = (h2 - 1) * w2;
                    for(int j = h2 - 1; j >= 0; --j, b -= w2) {
                        int a = b + i;
                        *(dst_u++) = src_u[a];
                        *(dst_v++) = src_v[a];
                    }
                }

                int32_t tmp = videoFrame->mHeight;
                videoFrame->mHeight = videoFrame->mWidth;
                videoFrame->mWidth = tmp;
                videoFrame->mStride = videoFrame->mWidth;
            }
            break;

        case 180:
        case -180:
            {
                MMLOGV("180\n");
                const uint8_t * src_p = src->getBuffer();
                uint8_t * dst_p = dst->getWritableBuffer();
                int a = videoFrame->mStride * (videoFrame->mHeight - 1);
                for (int32_t i = videoFrame->mHeight - 1; i >= 0; --i, a -= videoFrame->mStride) {
                    for (int32_t j = videoFrame->mWidth - 1; j >= 0; --j) {
                        *(dst_p++) = src_p[a + j];
                    }
                }

                int32_t wh = videoFrame->mStride * videoFrame->mHeight;
                int32_t w2 = videoFrame->mWidth >> 1; //
                int32_t h2 = videoFrame->mHeight >> 1;
                const uint8_t * src_u = src_p + wh;
                const uint8_t * src_v = src_u + (wh >> 2);
                uint8_t * dst_u = dst_p;
                uint8_t * dst_v = dst_u + (wh >> 2);
                a = (h2 - 1) * w2;
                for(int i = h2 - 1; i >= 0; --i, a -= w2) {
                    for(int j = w2 - 1; j >= 0; --j) {
                        int b = a + j;
                        *(dst_u++) = src_u[b];
                        *(dst_v++) = src_v[b];
                    }
                }
            }
            break;

        case 270:
        case -90:
            {
                MMLOGV("270\n");
                const uint8_t * src_p = src->getBuffer();
                uint8_t * dst_p = dst->getWritableBuffer();
                for (int32_t i = videoFrame->mWidth - 1; i >= 0; --i) {
                    int a = 0;
                    for (int32_t j = 0; j < videoFrame->mHeight; ++j, a += videoFrame->mStride) {
                        *(dst_p++) = src_p[a + i];
                    }
                }

                int32_t wh = videoFrame->mStride * videoFrame->mHeight;
                int32_t w2 = videoFrame->mWidth >> 1; //
                int32_t h2 = videoFrame->mHeight >> 1;
                const uint8_t * src_u = src_p + wh;
                const uint8_t * src_v = src_u + (wh >> 2);
                uint8_t * dst_u = dst_p;
                uint8_t * dst_v = dst_u + (wh >> 2);
                for(int i = w2 - 1; i >= 0; --i) {
                    int a = 0;
                    for(int j = 0; j < h2; ++j, a += w2) {
                        int b = a + i;
                        *(dst_u++) = src_u[b];
                        *(dst_v++) = src_v[b];
                    }
                }

                int32_t tmp = videoFrame->mHeight;
                videoFrame->mHeight = videoFrame->mWidth;
                videoFrame->mWidth = tmp;
                videoFrame->mStride = videoFrame->mWidth;
            }
            break;

        default:
            MMLOGE("unsupported angle: %d\n", videoFrame->mRotationAngle);
            return false;
    }

    dst->setActualSize(dst->getSize());
    MMLOGV("size: %zu\n", dst->getActualSize());
    videoFrame->mData = dst;
    videoFrame->mRotationAngle = 0;

    return true;
}

bool MediaCollectorImp::convertYUV(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight)
{
    MMLOGV("outWidth: %u, outHeight: %u, format: %d\n", outWidth, outHeight, videoFrame->mColorFormat);
    MMASSERT(videoFrame != NULL);
    switch (videoFrame->mColorFormat) {
        case 'NV12':
            return convertNV12(videoFrame, outWidth, outHeight);
        case 'YV12':
        case '420p':
            return convertYV12(videoFrame, outWidth, outHeight);
        case 'YV21':
            return convertYV21(videoFrame, outWidth, outHeight);
        case 'NV21':
        default:
            MMLOGE("not implemented colorformat: %d\n", videoFrame->mColorFormat);
            return false;
    }
}

bool MediaCollectorImp::convertNV12(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight)
{
    MMASSERT(videoFrame != NULL);
    int32_t pixCount = outWidth * outHeight;

    struct SwsContext * sws = sws_getContext(videoFrame->mWidth,
                                videoFrame->mHeight,
                                AV_PIX_FMT_NV12,
                                outWidth,
                                outHeight,
                                AV_PIX_FMT_YUV420P,
                                SWS_BICUBIC,
                                NULL,
                                NULL,
                                NULL);

    MMBufferSP dstBuf = MMBuffer::create((pixCount * 3) >> 1);
    if (!dstBuf) {
        MMLOGE("no mem\n");
        return false;
    }

    const uint8_t * input[4];
    input[0] = videoFrame->mData->getBuffer();
    input[1] = videoFrame->mData->getBuffer() + videoFrame->mStride * videoFrame->mHeight;
    input[2] = input[3] = NULL;
    int inputSize[4] = {(int)videoFrame->mStride, (int)videoFrame->mStride};
    input[2] = input[3] = 0;
    uint8_t * output[4];
    output[0] = dstBuf->getWritableBuffer();
    output[1] = dstBuf->getWritableBuffer() + pixCount;
    output[2] = dstBuf->getWritableBuffer() + ((pixCount * 5) >> 2);
    output[3] = NULL;
    int outputSize[4] = {(int)outWidth, (int)(outWidth >> 1), (int)(outWidth >> 1), 0};
    int dh = sws_scale(sws,
        input,
        inputSize,
        0,
        videoFrame->mHeight,
        output,
        outputSize);
    MMLOGV("scal ret: %d, w: %u, h: %u\n", dh, outWidth, outHeight);
    dstBuf->setActualSize(dstBuf->getSize());
    videoFrame->mData = dstBuf;
    sws_freeContext(sws);
    sws = NULL;

    videoFrame->mColorFormat = 'YV12';
    videoFrame->mWidth = outWidth;
    videoFrame->mHeight = outHeight;
    videoFrame->mStride = videoFrame->mWidth;

    return true;
}

bool MediaCollectorImp::convertYV12(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight)
{
    MMASSERT(videoFrame != NULL);
    if (videoFrame->mWidth == outWidth && videoFrame->mHeight == outHeight) {
        return true;
    }

    int32_t pixCount = outWidth * outHeight;

    struct SwsContext * sws = sws_getContext(videoFrame->mWidth,
                                videoFrame->mHeight,
                                AV_PIX_FMT_YUV420P,
                                outWidth,
                                outHeight,
                                AV_PIX_FMT_YUV420P,
                                SWS_BICUBIC,
                                NULL,
                                NULL,
                                NULL);

    MMBufferSP dstBuf = MMBuffer::create(pixCount << 1);
    if (!dstBuf) {
        MMLOGE("no mem\n");
        sws_freeContext(sws);
        return false;
    }

    int32_t pixCountSrc = videoFrame->mStride * videoFrame->mHeight;
    const uint8_t * input[4];
    input[0] = videoFrame->mData->getBuffer();
    input[1] = videoFrame->mData->getBuffer() + pixCountSrc;
    input[2] = videoFrame->mData->getBuffer() + ((pixCountSrc * 5) >> 2);
    input[3] = NULL;
    int inputSize[4] = {(int)videoFrame->mWidth, (int)(videoFrame->mWidth >> 1), (int)(videoFrame->mWidth >> 1), 0};
    uint8_t * output[4];
    output[0] = dstBuf->getWritableBuffer();
    output[1] = dstBuf->getWritableBuffer() + pixCount;
    output[2] = dstBuf->getWritableBuffer() + ((pixCount * 5) >> 2);
    output[3] = NULL;
    int outputSize[4] = {(int)outWidth, (int)(outWidth >> 1), (int)(outWidth >> 1), 0};
    int dh = sws_scale(sws,
        input,
        inputSize,
        0,
        videoFrame->mHeight,
        output,
        outputSize);
    MMLOGV("scal ret: %d, w: %u, h: %u\n", dh, outWidth, outHeight);
    dstBuf->setActualSize(dstBuf->getSize());
    videoFrame->mData = dstBuf;
    sws_freeContext(sws);
    sws = NULL;

    videoFrame->mWidth = outWidth;
    videoFrame->mHeight = outHeight;
    videoFrame->mStride = videoFrame->mWidth;

    return true;
}

bool MediaCollectorImp::convertYV21(VideoFrameSP & videoFrame,
        int32_t outWidth,
        int32_t outHeight)
{
    MMLOGV("+\n");
    int s = videoFrame->mWidth * videoFrame->mHeight;
    uint8_t * p = videoFrame->mData->getWritableBuffer() + s;
    int s2 = s >> 2;
    uint8_t * q = p + s2;
    for (int i = 0; i < s2; ++i) {
        uint8_t r = *p;
        *p = *q;
        *q = r;
        ++p;
        ++q;
    }

    videoFrame->mStride = videoFrame->mWidth;
    videoFrame->mColorFormat = 'YV12';
    return convertYV12(videoFrame, outWidth, outHeight);
}

#ifdef _MEDIACOLLECT_PIC_MAX_SIZE
bool MediaCollectorImp::isPicSizeExceeds(uint32_t width, uint32_t height)
{
    if (width * height > _MEDIACOLLECT_PIC_MAX_SIZE) {
        MMLOGE("pic size exceeds: w: %u, h: %u, max: %u\n", width, height, (uint32_t)(_MEDIACOLLECT_PIC_MAX_SIZE));
        return true;
    }

    return false;
}
#endif

mm_status_t MediaCollectorImp::collectJPEG(const char * path)
{
    CALCULATE_TIME_COST(mJpegFileTimeCost);
    parseExif(path);

    uint32_t width, height, outWidth, outHeight;
    MMBufferSP rgb;
    if (parseJPEG(path,
        mThumbMaxSizeSqrt,
        width,
        height,
        outWidth,
        outHeight,
        rgb) != MM_ERROR_SUCCESS) {
        return MM_ERROR_OP_FAILED;
    }
#ifdef _MEDIACOLLECT_PIC_MAX_SIZE
    if (isPicSizeExceeds(width, height)) {
        return MM_ERROR_SKIPPED;
    }
#endif
    MMLOGD("(%dx%d) decode to out (%dx%d)", width, height, outWidth, outHeight);
    genRGBThumb(rgb->getBuffer(), true, outWidth, outHeight, false);

    REPORT_BASE(width, height, MEDIA_MIMETYPE_IMAGE_JPEG);

    return MM_ERROR_SUCCESS;
}

/*
mm_status_t MediaCollectorImp::parseJPEG(const char * path,
                        const char * thumbFilePath,
                        uint32_t thumbSqt,
                        uint32_t & width,
                        uint32_t & height,
                        uint32_t & thumbWidth,
                        uint32_t & thumbHeight)
{
    MMLOGV("path: %s, thumbSqt: %u\n", path, thumbSqt);
    FILE * srcFile;
    if ((srcFile = fopen(path, "rb")) == NULL) {
        MMLOGE("can't open %s\n", path);
        return MM_ERROR_OP_FAILED;
    }

    FILE * thumbFile;
    if ((thumbFile = fopen(thumbFilePath, "wb")) == NULL) {
        MMLOGE("can't open %s\n", thumbFilePath);
        fclose(srcFile);
        return MM_ERROR_OP_FAILED;
    }

    struct jpeg_decompress_struct srcInfo;
    struct jpeg_compress_struct thumbInfo;

    MMLOGV("creating cinfo\n");
    jpeg_create_decompress(&srcInfo);
    jpeg_create_compress(&thumbInfo);

    MMLOGV("setting error\n");
    struct my_error_mgr jerr;
    srcInfo.err = jpeg_std_error(&jerr.pub);
    thumbInfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        MMLOGE("error occured\n");
        jpeg_destroy_decompress(&srcInfo);
        jpeg_destroy_compress(&thumbInfo);
        fclose(srcFile);
        fclose(thumbFile);
        remove(thumbFilePath);
        return MM_ERROR_OP_FAILED;
    }

    MMLOGV("set src\n");
    jpeg_stdio_src(&srcInfo, srcFile);
    MMLOGV("read header\n");
    (void) jpeg_read_header(&srcInfo, TRUE);
    MMLOGV("starting decompress\n");

    if (srcInfo.image_width * srcInfo.image_height <= thumbSqt * thumbSqt) {
        srcInfo.scale_num = 1;
        srcInfo.scale_denom = 1;
    } else {
        unsigned int useF = srcInfo.image_width > srcInfo.image_height ? srcInfo.image_width : srcInfo.image_height;
        srcInfo.scale_num = thumbSqt;
        srcInfo.scale_denom = useF;
    }

    (void) jpeg_start_decompress(&srcInfo);
    MMLOGV("after calc: scale_num: %d, scale_denom: %d, image_width: %d, image_height: %d, output_width: %d, output_height: %d\n",
        srcInfo.scale_num, srcInfo.scale_denom,
        srcInfo.image_width, srcInfo.image_height,
        srcInfo.output_width, srcInfo.output_height);

    jpeg_stdio_dest(&thumbInfo, thumbFile);
    thumbInfo.input_components = srcInfo.output_components;
    thumbInfo.in_color_space = srcInfo.out_color_space;
    jpeg_set_defaults(&thumbInfo);
    thumbInfo.image_width = srcInfo.output_width;// * mThumbMaxSizeSqrt / useF;
    thumbInfo.image_height = srcInfo.output_height;// * mThumbMaxSizeSqrt / useF;

    thumbWidth = thumbInfo.image_width;
    thumbHeight = thumbInfo.image_height;

    MMLOGV("start compress\n");
    jpeg_start_compress(&thumbInfo, TRUE);
    MMLOGV("after start: thumbInfo.scale_num: %d, scale_denom: %d, image_width: %d, image_height: %d, jpeg_width: %d, jpeg_height: %d\n",
        thumbInfo.scale_num, thumbInfo.scale_denom,
        thumbInfo.image_width, thumbInfo.image_height,
        thumbInfo.jpeg_width, thumbInfo.jpeg_height);

    int row_stride = srcInfo.output_width * srcInfo.output_components;
    JSAMPARRAY buffer = (*srcInfo.mem->alloc_sarray)
                ((j_common_ptr) &srcInfo, JPOOL_IMAGE, row_stride, 1);
    while (srcInfo.output_collectline < srcInfo.output_height) {
        (void) jpeg_read_collectlines(&srcInfo, buffer, 1);
        JSAMPROW row_pointer[1];
        row_pointer[0] = buffer[0];
        (void) jpeg_write_collectlines(&thumbInfo, row_pointer, 1);
    }

    width = srcInfo.image_width;
    height = srcInfo.image_height;
    (void) jpeg_finish_decompress(&srcInfo);
    jpeg_finish_compress(&thumbInfo);

    jpeg_destroy_decompress(&srcInfo);
    jpeg_destroy_compress(&thumbInfo);
    fclose(srcFile);
    fclose(thumbFile);

    return MM_ERROR_SUCCESS;
}
*/

mm_status_t MediaCollectorImp::parseJPEG(const char * path,
                        uint32_t maxSqrt,
                        uint32_t & srcWidth,
                        uint32_t & srcHeight,
                        uint32_t & outWidth,
                        uint32_t & outHeight,
                        MMBufferSP & outRGB)
{
    CALCULATE_TIME_COST(mDecodeTimeCost);
    MMLOGV("path: %s, maxSqrt: %u\n", path, maxSqrt);
    FILE * srcFile;
    if ((srcFile = fopen(path, "rb")) == NULL) {
        MMLOGE("can't open %s\n", path);
        return MM_ERROR_OP_FAILED;
    }

    MMLOGV("creating cinfo\n");
    struct jpeg_decompress_struct srcInfo;
    memset(&srcInfo, 0, sizeof(srcInfo));
    jpeg_create_decompress(&srcInfo);

    MMLOGV("setting error\n");
    struct my_error_mgr jerr;
    srcInfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        MMLOGE("error occured\n");
        jpeg_destroy_decompress(&srcInfo);
        fclose(srcFile);
        return MM_ERROR_OP_FAILED;
    }

    MMLOGV("set src\n");
    jpeg_stdio_src(&srcInfo, srcFile);
    MMLOGV("read header\n");
    (void) jpeg_read_header(&srcInfo, TRUE);
    MMLOGV("starting decompress\n");

    if (srcInfo.image_width * srcInfo.image_height <= maxSqrt * maxSqrt) {
        srcInfo.scale_num = 1;
        srcInfo.scale_denom = 1;
    } else {
        unsigned int useF = srcInfo.image_width > srcInfo.image_height ? srcInfo.image_width : srcInfo.image_height;
        srcInfo.scale_num = maxSqrt;
        srcInfo.scale_denom = useF;
    }

    srcInfo.out_color_space = JCS_EXT_RGBX;
    (void) jpeg_start_decompress(&srcInfo);
    MMLOGV("after calc: scale_num: %d, scale_denom: %d, image_width: %d, image_height: %d, output_width: %d, output_height: %d, output_components: %d, out_color_components: %d\n",
        srcInfo.scale_num, srcInfo.scale_denom,
        srcInfo.image_width, srcInfo.image_height,
        srcInfo.output_width, srcInfo.output_height,
        srcInfo.output_components, srcInfo.out_color_components);

    srcWidth = srcInfo.image_width;
    srcHeight = srcInfo.image_height;
    outWidth = srcInfo.output_width;
    outHeight = srcInfo.output_height;

    int row_stride = srcInfo.output_width * srcInfo.output_components;
    outRGB = MMBuffer::create(row_stride * srcInfo.output_height);
    if (!outRGB) {
        MMLOGE("no mem\n");
        (void) jpeg_finish_decompress(&srcInfo);

        jpeg_destroy_decompress(&srcInfo);
        fclose(srcFile);
        return MM_ERROR_NO_MEM;
    }
    outRGB->setActualSize(outRGB->getSize());

    uint8_t * dstBuf = outRGB->getWritableBuffer();

    JSAMPARRAY buffer = (*srcInfo.mem->alloc_sarray)
                ((j_common_ptr) &srcInfo, JPOOL_IMAGE, row_stride, 1);
    while (srcInfo.output_scanline < srcInfo.output_height) {
        (void) jpeg_read_scanlines(&srcInfo, buffer, 1);
        memcpy(dstBuf, buffer[0], row_stride);
        dstBuf += row_stride;
    }

    (void) jpeg_finish_decompress(&srcInfo);

    jpeg_destroy_decompress(&srcInfo);
    fclose(srcFile);

    return MM_ERROR_SUCCESS;
}


void MediaCollectorImp::parseExif(const char * path)
{
    MMLOGV("+\n");
    ExifData *d = exif_data_new_from_file(path);
    if ( !d ) {
        MMLOGV("failed to parse file: %s\n", path);
        return;
    }

    exif_data_foreach_content(d, exifDataForEachFunc, this);
    exif_data_unref(d);
}

/*static */void MediaCollectorImp::exifDataForEachFunc(ExifContent *content, void *callback_data)
{
    MMLOGV("+\n");
    exif_content_foreach_entry(content, exifContentForEachFunc, callback_data);
}

/*static */void MediaCollectorImp::exifContentForEachFunc(ExifEntry *entry, void *callback_data)
{
    MediaCollectorImp * me = static_cast<MediaCollectorImp*>(callback_data);
    if ( !me ) {
        MMLOGE("invalid me\n");
        return;
    }

    me->exifContentForEachFunc(entry);
}

#define EXIF_TO_TAG_BEGIN()\
    if ( 0 ){}\

#define EXIF_TO_TAG_STR(_exif_tag, _format, _name)\
     else if ( entry->tag == _exif_tag && entry->format == _format ) {\
        const char * s = exif_entry_get_value(entry, buf, sizeof(buf));\
        if ( s[0] != '\0' )\
            ADD_STRING_ATTRIB(_name, s);\
    }
#define EXIF_TO_TAG_SHORT(_exif_tag, _format, _name)\
     else if ( entry->tag == _exif_tag && entry->format == _format ) {\
        uint16_t val = exif_get_short(entry->data, exif_data_get_byte_order(entry->parent->parent));\
        snprintf(buf, sizeof(buf), "%u", val);\
        ADD_ATTRIB(_name, buf);\
    }

#define EXIF_TO_TAG_ROTATION(_exif_tag, _format, _name)\
     else if ( entry->tag == _exif_tag && entry->format == _format ) {\
        uint16_t val = exif_get_short(entry->data, exif_data_get_byte_order(entry->parent->parent));\
        MMLOGV("rotation: val %d\n", val);\
        switch (val) {\
            case 6:\
                val = 90;\
                break;\
            case 3:\
                val = 180;\
                break;\
            case 8:\
                val = 270;\
                break;\
            default:\
                val = 0;\
                break;\
        }\
        snprintf(buf, sizeof(buf), "%u", val);\
        mCurRotation = val;\
        MMLOGV("rotation: %d\n", mCurRotation);\
        ADD_ATTRIB(_name, buf);\
    }

#define EXIF_TO_TAG_END()

void MediaCollectorImp::exifContentForEachFunc(ExifEntry *entry)
{
    char buf[256];
    exif_entry_get_value(entry, buf, sizeof(buf));
    MMLOGV("tagname: %s | formatname: %s | "
        "Size, Comps: %d, %d | "
        "Value: %s\n",
        exif_tag_get_name(entry->tag),
        exif_format_get_name(entry->format),
        entry->size,
        (int)(entry->components),
        exif_entry_get_value(entry, buf, sizeof(buf)));

    EXIF_TO_TAG_BEGIN()
        EXIF_TO_TAG_STR(EXIF_TAG_IMAGE_DESCRIPTION, EXIF_FORMAT_ASCII, MEDIA_ATTR_DESCRIPTION)
        EXIF_TO_TAG_STR(EXIF_TAG_INTEROPERABILITY_VERSION, EXIF_FORMAT_RATIONAL, MEDIA_ATTR_LOCATION_LATITUDE)
        EXIF_TO_TAG_STR(EXIF_TAG_GPS_LONGITUDE, EXIF_FORMAT_RATIONAL, MEDIA_ATTR_LOCATION_LONGITUDE)
        EXIF_TO_TAG_ROTATION(EXIF_TAG_ORIENTATION, EXIF_FORMAT_SHORT, MEDIA_ATTR_ORIENTATION)
        else if ( entry->tag == EXIF_TAG_DATE_TIME && entry->format == EXIF_FORMAT_ASCII ) {
            char * p = (char*)exif_entry_get_value(entry, buf, sizeof(buf));
            char * q = p;
            if ( p ) {
                int i = 0;
                while (*p != '\0' && i < 2) {
                    if (*p == ':') {
                        ++i;
                        *p = '-';
                    }
                    ++p;
                }
                ADD_ATTRIB(MEDIA_ATTR_DATETAKEN, q);
            }
        }
    EXIF_TO_TAG_END()
}

mm_status_t MediaCollectorImp::collectBMP(const char * path)
{
#ifdef _MEDIACOLLECT_PIC_MAX_SIZE
    {
        uint32_t w,h;
        if (BMP::parseDimension(path, w, h) != MM_ERROR_SUCCESS) {
            MMLOGE("failed to parseDimension\n");
            return MM_ERROR_INVALID_PARAM;
        }
        if (isPicSizeExceeds(w, h)) {
            return MM_ERROR_SKIPPED;
        }
    }
#endif
    uint32_t w,h;
    MMBufferSP data = BMP::decode(path,
                                        1,
                                        1,
                                        RGB::kFormatRGB32,
                                        w,
                                        h);
    if (!data) {
        MMLOGE("failed to decode\n");
        return MM_ERROR_OP_FAILED;
    }

    REPORT_BASE(w, h, MEDIA_MIMETYPE_IMAGE_BMP);

    genRGBThumb(data->getBuffer(),
        true,
        w,
        h,
        true);
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaCollectorImp::collectPNG(const char * path)
{
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    FILE *fp;

    if ((fp = fopen(path, "rb")) == NULL) {
        MMLOGE("failed to open file %s\n", path);
        return MM_ERROR_OP_FAILED;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
     if (png_ptr == NULL) {
        fclose(fp);
        MMLOGE("failed create struct for %s\n", path);
        return MM_ERROR_OP_FAILED;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        MMLOGE("failed create info struct for %s\n", path);
        return MM_ERROR_OP_FAILED;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        /* Free all of the memory associated with the png_ptr and info_ptr */
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        MMLOGE("read failed for %s\n", path);
        return MM_ERROR_OP_FAILED;
    }

    png_init_io(png_ptr, fp);

    png_set_sig_bytes(png_ptr, sig_read);

    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
            &interlace_type, NULL, NULL);

    MMLOGV("width: %d, height: %d, bit_depth: %d, color_type: %d, interlace_type: %d\n",
            width, height, bit_depth, color_type, interlace_type);
#ifdef _MEDIACOLLECT_PIC_MAX_SIZE
    if (isPicSizeExceeds(width, height)) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return MM_ERROR_SKIPPED;
    }
#endif

    REPORT_BASE(width, height, MEDIA_MIMETYPE_IMAGE_PNG);

    if (bit_depth == 16)
       png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(png_ptr);

    if (bit_depth<8)
        png_set_expand(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_expand(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0)
       png_set_tRNS_to_alpha(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    size_t rowBytes = png_get_rowbytes(png_ptr, info_ptr);
    uint8_t * data = (uint8_t*)png_malloc(png_ptr, rowBytes * height);
    if (!data) {
        MMLOGE("no mem\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return MM_ERROR_OP_FAILED;
    }

    png_bytep row_pointers[height];
    uint8_t * p = data;
    for (png_uint_32 row = 0; row < height; row++) {
        row_pointers[row] = p;
        p += rowBytes;
    }

    png_read_image(png_ptr, row_pointers);

    MMLOGV("rowBytes: %u\n", rowBytes);
    genPngThumb(data, rowBytes/width == 4, width, height);

    png_read_end(png_ptr, info_ptr);
    png_free(png_ptr, data);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);
    data = NULL;

    return MM_ERROR_SUCCESS;
}

bool MediaCollectorImp::genPngThumb(const uint8_t * data, bool hasAlpha, uint32_t width, uint32_t height)
{
    return genRGBThumb(data, hasAlpha, width, height, false);
}

bool MediaCollectorImp::genRGBThumb(const uint8_t * data, bool hasAlpha, uint32_t width, uint32_t height, bool bottomUp)
{
    CALCULATE_TIME_COST(mGenThumbTimeCost);
    MMASSERT(data != NULL);
    RGB::Format format = hasAlpha ? RGB::kFormatRGB32 : RGB::kFormatRGB24;
    MMBufferSP scaled;
    uint32_t den;
    uint32_t num;
    calcFactor(false, width, height, num , den);
    bool needScale = false;
    if (num != 1 || den != 1) {
        MMLOGV("scale it\n");
        needScale = true;
        uint32_t scaledW, scaledH;
        if (RGB::zoom(data,
            format,
            width,
            height,
            num,
            den,
            scaled,
            scaledW,
            scaledH) != MM_ERROR_SUCCESS) {
            MMLOGE("zoom failed\n");
            return MM_ERROR_OP_FAILED;
        }

        width = scaledW;
        height = scaledH;
    }

    MMBufferSP rotated;
    const uint8_t * thumbData;
    if (mCurRotation) {
        MMLOGV("rotate it: %d\n", mCurRotation);
        uint32_t rotatedW, rotatedH;
        if (RGB::rotate(needScale ? scaled->getBuffer() : data,
            format,
            width,
            height,
            mCurRotation,
            rotated,
            &rotatedW,
            &rotatedH) != MM_ERROR_SUCCESS) {
            MMLOGE("failed to rotate\n");
            return MM_ERROR_OP_FAILED;
        }

        thumbData = rotated->getBuffer();
        width = rotatedW;
        height = rotatedH;
    } else {
        thumbData = needScale ? scaled->getBuffer() : data;
    }

    uint32_t thumbWidth;
    uint32_t thumbHeight;
    { // first thumbnail
    CALCULATE_TIME_COST(mThumb1TimeCost);
    genThumbFileName();
    if (RGB::saveAsJpeg(thumbData,
            format,
            width,
            height,
            1,
            1,
            bottomUp,
            mThumbFilePath,
            thumbWidth,
            thumbHeight) == MM_ERROR_SUCCESS) {
        MMLOGD("(%dx%d) save to thumb(%dx%d)", width, height, thumbWidth, thumbHeight);
        REPORT_THUMB(mThumbFilePath, thumbWidth, thumbHeight);
    }
    } // end of first thumbnail


    { // second thumbnail
    CALCULATE_TIME_COST(mThumb2TimeCost);
    calcFactor(true, width, height, num , den);
    if (RGB::saveAsJpeg(thumbData,
            hasAlpha ? RGB::kFormatRGB32 : RGB::kFormatRGB24,
            width,
            height,
            num,
            den,
            bottomUp,
            mMicroThumbFilePath,
            thumbWidth,
            thumbHeight) == MM_ERROR_SUCCESS) {
        MMLOGD("(%dx%d) save to thumb(%dx%d)", width, height, thumbWidth, thumbHeight);
        REPORT_MICROTHUMB(mMicroThumbFilePath, thumbWidth, thumbHeight);
    }
    } // end of second thumbnail

    return true;
}

mm_status_t MediaCollectorImp::collectGIF(const char * path)
{
    int err;
    GifFileType * thegf = DGifOpenFileName(path, &err);
    if ( !thegf ) {
        MMLOGE("failed to open file %s\n", path);
        return MM_ERROR_SKIPPED;
    }
#ifdef _MEDIACOLLECT_PIC_MAX_SIZE
    {
        if (isPicSizeExceeds(thegf->SWidth, thegf->SHeight)) {
            DGifCloseFile(thegf, &err);
            return MM_ERROR_SKIPPED;
        }
    }
#endif

    const char * thumbFile = genThumbFileName();
    uint32_t thumbWidth, thumbHeight;
    genGifThumb(thegf, thumbFile, thumbWidth, thumbHeight);

    REPORT_BASE(thegf->SWidth, thegf->SHeight, MEDIA_MIMETYPE_IMAGE_GIF);

    DGifCloseFile(thegf, &err);

    return MM_ERROR_SUCCESS;
}

#define RELEASE_SCREEN_BUF(_sb, _count) do {\
    if (!_sb) {\
        break;\
    }\
    for (int i = 0; i < _count; ++i) {\
        GifRowType p = _sb[i];\
        if (p) {\
/*            MMLOGV("releasing(%d/%d): %p\n", i, _count, p);*/\
            free(p);\
            _sb[i] = NULL;\
/*            MMLOGV("releasing(%d/%d): %p over\n", i, _count, p);*/\
        }\
    }\
/*    MMLOGV("releasing pp: %p\n", _sb);*/\
    free(_sb);\
/*    MMLOGV("releasing pp: %p OVER\n", _sb);*/\
    (_sb) = NULL;\
}while(0)

bool MediaCollectorImp::genGifThumb(void * gifFileType, const char * jpegPath, uint32_t & thumbWidth, uint32_t & thumbHeight)
{
            int i;
            int j;
            GifRecordType thert;
            int thesz;
            int theR;
            int thec;
            GifRowType *thesb;
            int theec;
            GifByteType *theet;
            int theilo[] = {
                0,
                4,
                2,
                1 };
            ColorMapObject *thecm;
            int theilj[] = {
                8,
                8,
                4,
                2 };
            int thein = 0;
            int width, height;
            GifFileType *thegf = (GifFileType*)gifFileType;
            if ((thesb = (GifRowType *)
                malloc(thegf->SHeight * sizeof(GifRowType))) == NULL) {
                MMLOGE("Failed to allocate thesb.");
                RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                return false;
            }
            memset(thesb, 0, thegf->SHeight * sizeof(GifRowType));
            
            thesz = thegf->SWidth * sizeof(GifPixelType);
            if ((thesb[0] = (GifRowType) malloc(thesz)) == NULL)  {
                MMLOGE("Failed to allocate ScreenBuffer0.");
                RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                return false;
            }
            
            MMLOGV("w: %d, h: %d, bkcolor: 0x%x",
                thegf->SWidth,
                thegf->SHeight,
                thegf->SBackGroundColor);
            
            for (i = 0; i < thegf->SWidth; i++)
                thesb[0][i] = thegf->SBackGroundColor;
            
            for (i = 1; i < thegf->SHeight; i++) {
                if ((thesb[i] = (GifRowType) malloc(thesz)) == NULL) {
                    MMLOGE("failed to allocate row\n");
                    RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                    return false;
                }
            
                memcpy(thesb[i], thesb[0], thesz);
            }
            
            do {
                if (DGifGetRecordType(thegf, &thert) == GIF_ERROR) {
                    MMLOGE("failed to DGifGetRecordType\n");
                    RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                    return false;
                }
                switch (thert) {
                case IMAGE_DESC_RECORD_TYPE:
                    MMLOGV("rectype: image desc\n");
                    if (DGifGetImageDesc(thegf) == GIF_ERROR) {
                        MMLOGE("failed to DGifGetImageDesc: %s\n", GifErrorString(thegf->Error));
                        RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                        return false;
                    }
                    theR = thegf->Image.Top;
                    thec = thegf->Image.Left;
                    width = thegf->Image.Width;
                    height = thegf->Image.Height;
                    MMLOGV("Image %d at (%d, %d) [%dx%d]:     ",
                        ++thein, thec, theR, width, height);
                    if (thegf->Image.Left + thegf->Image.Width > thegf->SWidth ||
                        thegf->Image.Top + thegf->Image.Height > thegf->SHeight) {
                        MMLOGE("Image %d is not confined to screen dimension, aborted.\n",thein);
                        RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                        return false;
                    }
                    if (thegf->Image.Interlace) {
                        MMLOGV("Interlace\n");
                        for (i = 0; i < 4; i++)
                            for (j = theR + theilo[i]; j < theR + (int)height; j += theilj[i]) {
                                if (DGifGetLine(thegf, &thesb[j][thec], (int)width) == GIF_ERROR) {
                                    MMLOGE("failed to get line: %s\n", GifErrorString(thegf->Error));
                                    RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                                    return false;
                                }
                            }
                    } else {
                        MMLOGV("not Interlace\n");
                        for (i = 0; i < (int)height; i++) {
                            if (DGifGetLine(thegf, &thesb[theR++][thec], (int)width) == GIF_ERROR) {
                                MMLOGE("failed to get line: %s\n", GifErrorString(thegf->Error));
                                RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                                return false;
                            }
                        }
                    }
                    break;
                case EXTENSION_RECORD_TYPE:
                    MMLOGV("rectype: extension\n");
                    if (DGifGetExtension(thegf, &theec, &theet) == GIF_ERROR) {
                        MMLOGE("failed to get DGifGetExtension: %s\n", GifErrorString(thegf->Error));
                        RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                        return false;
                    }
                    while (theet != NULL) {
                        if (DGifGetExtensionNext(thegf, &theet) == GIF_ERROR) {
                            MMLOGE("failed to get DGifGetExtensionNext: %s\n", GifErrorString(thegf->Error));
                            RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                            return false;
                        }
                    }
                    break;
                case TERMINATE_RECORD_TYPE:
                    MMLOGV("rectype: TERMINATE\n");
                    break;
                default:
                    MMLOGV("rectype: unrecognized\n");
                    break;
                }
            } while (thert != TERMINATE_RECORD_TYPE);
            
            MMLOGV("setting color map\n");
            thecm = (thegf->Image.ColorMap
                                    ? thegf->Image.ColorMap
                                    : thegf->SColorMap);
            if (thecm == NULL) {
                MMLOGE("Gif Image does not have a colormap\n");
                RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                return false;
            }
            
            MMBufferSP buf = MMBuffer::create(thegf->SWidth * 3 * thegf->SHeight);
            if (!buf) {
                MMLOGE("no mem\n");
                RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
                return false;
            }
            buf->setActualSize(buf->getActualSize());
            
            MMLOGV("to rgb\n");
            for (i = 0; i < thegf->SHeight; i++) {
                uint8_t *thebp;
                GifRowType thegr = thesb[i];
                for (j = 0, thebp = buf->getWritableBuffer() + i * thegf->SWidth * 3; j < thegf->SWidth; j++) {
                    GifColorType * cm = &thecm->Colors[thegr[j]];
                    *thebp++ = cm->Red;
                    *thebp++ = cm->Green;
                    *thebp++ = cm->Blue;
                }
            }

    genRGBThumb(buf->getBuffer(), false, thegf->SWidth, thegf->SHeight, false);
    RELEASE_SCREEN_BUF(thesb, thegf->SHeight);
    return true;
}

const char * MediaCollectorImp::genThumbFileName()
{
    timeval now;
    gettimeofday(&now, NULL);
    int64_t val = (now.tv_sec * 1000000LL + now.tv_usec) / 1000;
    int i = 0;
    while ( 1 ) {
        snprintf(mThumbFilePath, PATH_MAX, "%s/%s%" PRId64 "%d.jpg", mThumbPath.c_str(), THUMB_FILE_PRFIX, val, i);
        if (access(mThumbFilePath, F_OK)) {
            snprintf(mMicroThumbFilePath, PATH_MAX, "%s/%s%" PRId64 "%d.jpg", mThumbPath.c_str(), MICRO_THUMB_FILE_PRFIX, val, i);
            break;
        }

        ++i;
    }

    return mThumbFilePath;
}

mm_status_t MediaCollectorImp::collectWEBP(const char * path)
{
    MMASSERT(path != NULL);
    FILE * srcFile;
    if ((srcFile = fopen(path, "rb")) == NULL) {
        MMLOGE("can't open %s\n", path);
        return MM_ERROR_OP_FAILED;
    }

    fseek(srcFile, 0, SEEK_END);
    long fSize = ftell(srcFile);
    MMLOGV("file size: %ld\n", fSize);
    fseek(srcFile, 0, SEEK_SET);
    uint8_t * fBuf = NULL;
    if (fSize > 0)
        fBuf = new uint8_t[fSize];
    if (!fBuf) {
        MMLOGE("no mem, need: %l\n", fSize);
        fclose(srcFile);
        return MM_ERROR_NO_MEM;
    }

    size_t readed = fread(fBuf, 1, fSize, srcFile);
    if (readed < (size_t)fSize) {
        MMLOGE("failed to read\n");
        fclose(srcFile);
        MM_RELEASE_ARRAY(fBuf);
        return MM_ERROR_OP_FAILED;
    }

    int width, height;
    uint8_t * rgba = WebPDecodeRGBA(fBuf, readed, &width, &height);
    if (!rgba) {
        MMLOGE("failed to decode\n");
        fclose(srcFile);
        MM_RELEASE_ARRAY(fBuf);
        return MM_ERROR_OP_FAILED;
    }

#ifdef _MEDIACOLLECT_PIC_MAX_SIZE
    if (isPicSizeExceeds(width, height)) {
        fclose(srcFile);
        MM_RELEASE_ARRAY(fBuf);
        free(rgba);
        return MM_ERROR_SKIPPED;
    }
#endif

    genRGBThumb(rgba, true, width, height, false);

    REPORT_BASE(width, height, MEDIA_MIMETYPE_IMAGE_WEBP);

    fclose(srcFile);
    MM_RELEASE_ARRAY(fBuf);
    free(rgba);
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaCollectorImp::collectWBMP(const char * path)
{
    uint32_t w,h;
    MMBufferSP data = WBMP::decode(path,
                                        RGB::kFormatRGB32,
                                        w,
                                        h);
    if (!data) {
        MMLOGE("failed to decode\n");
        return MM_ERROR_OP_FAILED;
    }
#ifdef _MEDIACOLLECT_PIC_MAX_SIZE
    if (isPicSizeExceeds(w, h)) {
        return MM_ERROR_SKIPPED;
    }
#endif

    genRGBThumb(data->getBuffer(), true, w, h, true);

    REPORT_BASE(w, h, MEDIA_MIMETYPE_IMAGE_WBMP);
    return MM_ERROR_SUCCESS;
}

mm_status_t MediaCollectorImp::updateFileInfoToDB()
{
    mm_status_t ret = MM_ERROR_SUCCESS;
    CALCULATE_TIME_COST(mFileDBOperation);

    MMLOGD("insert/update info to DB: %s", mFileStat->mPath.c_str());
    ret = mWathcer->fileFound(mFileStat->mPath.c_str(), mFileStat->mStat.st_mtime, mFileStat->mStat.st_size , S_ISDIR(mFileStat->mStat.st_mode));
    MMLOGV("filestart ret: %d\n", ret);
    // MMAutoLock locker(mLock);
    while (!mTags.empty()) {
        FileTagType tag = mTags.front();
        mTags.pop();
        ret = mWathcer->addStringAttrib(tag.mName.c_str(), tag.mValue.c_str());
    }

    ret = mWathcer->fileDone(mCollectResult);
    MMLOGD("insert/update info to DB done: %s, ret: %d", mFileStat->mPath.c_str(), ret);
    if (ret != MM_ERROR_SUCCESS) {
        MMLOGE("fileend failed, rm thum\n");
        if (mThumbFilePath[0] != '\0') {
            remove(mThumbFilePath);
            remove(mMicroThumbFilePath);
            mThumbFilePath[0] = '\0';
            mMicroThumbFilePath[0] = '\0';
        }
    } else {
        mThumbFilePath[0] = '\0';
        mMicroThumbFilePath[0] = '\0';
    }

    return ret;
}

void MediaCollectorImp::stopCollect()
{
    MMLOGI("+\n");
    mContinue = false;
}

void MediaCollectorImp::reset()
{
    MMLOGI("+\n");
    mContinue = true;
}

}


