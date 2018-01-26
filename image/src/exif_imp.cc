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

#ifdef USE_RESMANAGER
#include <data/ResManager.h>
#include <file/FilePath.h>
#else
#include <ResourceLocator.h>
#endif
#include <uri/Uri.h>

#include "exif_imp.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

ExifImp::TagInfo::TagInfo(ExifContent * content, ExifEntry * entry) : mContent(content), mEntry(entry)
{
}


DEFINE_LOGTAG(ExifImp)

#define SET_STATE(_state) do {\
    MMLOGE("state change to %s\n", #_state);\
    mState = _state;\
}while(0)

ExifImp::ExifImp() : mState(kStateIdle), mExifData(NULL), mCurContent(NULL)
{
    MMLOGI("+\n");
}

ExifImp::~ExifImp()
{
    MMLOGI("+\n");
    if (kStateIdle != mState) {
        doClose();
    }
}

mm_status_t ExifImp::open(const char * uri)
{
    MMLOGI("uri: %s\n", uri);

    if (uri && !strncasecmp(uri, "page://", 7)) {
#ifdef USE_RESMANAGER
        yunos::FilePath filePath = yunos::ResManager::searchFile(yunos::Uri(uri));
        yunos::String path = filePath.toString();
#else
        yunos::String path = yunos::ResourceLocator::getInstance()->search(yunos::Uri(uri));
#endif
        if (path.size() == 0) {
            MMLOGE("failed to locate uri: %s\n", uri);
            return MM_ERROR_INVALID_PARAM;
        }
        MMLOGI("page converted uri: %s\n", path.c_str());
        mFile = path.c_str();
    } else {
        mFile = uri;
    }

    mExifData = exif_data_new_from_file(mFile.c_str());
    if ( !mExifData ) {
        MMLOGV("failed to parse file: %s\n", mFile.c_str());
        return MM_ERROR_OP_FAILED;
    }

    exif_data_foreach_content(mExifData, exifDataForEachFunc, this);

    mCurContent = NULL;
    SET_STATE(kStateOpened);
    return MM_ERROR_SUCCESS;
}

mm_status_t ExifImp::open(int fd, int64_t offset, int64_t length)
{
    MMLOGI("fd: %d, offset: %" PRId64 ", length: %" PRId64 "\n", fd, offset, length);
    if (fd < 0 || offset < 0 || length <= 0) {
        MMLOGE("invalid param: fd: %d, offset: %" PRId64 ", length: %" PRId64 "\n", fd, offset, length);
        return MM_ERROR_INVALID_PARAM;
    }

    uint8_t * buf;
    try {
        buf = new uint8_t[length];
    } catch (...) {
        MMLOGE("no mem, need: %" PRId64 "\n", length);
        return MM_ERROR_NO_MEM;
    }

    FILE * f = fdopen(fd, "rb");
    if (!f) {
        MMLOGE("failed to open file\n");
        delete []buf;
        return MM_ERROR_NO_MEM;
    }

    int seekRet = fseek(f, offset, SEEK_SET);
    if (seekRet < 0) {
        MMLOGE("failed to seek file\n");
        delete []buf;
        fclose(f);
        return MM_ERROR_NO_MEM;
    }

    size_t ret = fread(buf, 1, (size_t)length, f);
    if (ret < (size_t)length) {
        MMLOGE("read ret: %z, too samll(need %" PRId64 "\n", ret, length);
        delete []buf;
        fclose(f);
        return MM_ERROR_INVALID_PARAM;
    }

    fclose(f);

    mExifData = exif_data_new_from_data(buf, (unsigned int)length);
    if ( !mExifData ) {
        MMLOGV("failed to parse file: %s\n", mFile.c_str());
        return MM_ERROR_OP_FAILED;
    }

    exif_data_foreach_content(mExifData, exifDataForEachFunc, this);

    delete []buf;
    mCurContent = NULL;
    SET_STATE(kStateOpened);
    return MM_ERROR_SUCCESS;
}

/*static */void ExifImp::exifDataForEachFunc(ExifContent *content, void *callback_data)
{
    MMLOGV("+\n");
    ExifImp * me = static_cast<ExifImp*>(callback_data);
    MMASSERT(me != NULL);
    me->exifDataForEachFunc(content);
}

void ExifImp::exifDataForEachFunc(ExifContent *content)
{
    MMLOGV("+\n");
    mCurContent = content;
    exif_content_foreach_entry(content, exifContentForEachFunc, this);
}

/*static */void ExifImp::exifContentForEachFunc(ExifEntry *entry, void *callback_data)
{
    ExifImp * me = static_cast<ExifImp*>(callback_data);
    MMASSERT(me != NULL);

    me->exifContentForEachFunc(entry);
}

#define SAVE_TAG_START() if (0) {}

#define SAVE_TAG_ITEM(_tag) else if (entry->tag == (ExifTag)_tag && entry->format != EXIF_FORMAT_UNDEFINED) {\
    if (mTags.find(_tag) == mTags.end()) {\
        MMLOGV("save tag: (%d, %s, %d, %d)\n", mCurContent->count, #_tag, entry->format, entry->size);\
        TagInfoSP tag(new TagInfo(mCurContent, entry));\
        mTags[_tag] = tag;\
    } else {\
        MMLOGV("save tag exists: (%d, %s, %d, %d)\n", mCurContent->count, #_tag, entry->format, entry->size);\
    }\
}

#define SAV_TAG_END() else {\
    MMLOGV("ignore tag: (%d, 0x%x, %d)\n", mCurContent->count, entry->tag, entry->format);\
}

void ExifImp::exifContentForEachFunc(ExifEntry *entry)
{
    try {
        SAVE_TAG_START()
            SAVE_TAG_ITEM(kTagType_GPS_LATITUDE_REF)
            SAVE_TAG_ITEM(kTagType_GPS_LATITUDE)
            SAVE_TAG_ITEM(kTagType_GPS_LONGITUDE_REF)
            SAVE_TAG_ITEM(kTagType_GPS_LONGITUDE)
            SAVE_TAG_ITEM(kTagType_GPS_ALTITUDE_REF)
            SAVE_TAG_ITEM(kTagType_GPS_ALTITUDE)
            SAVE_TAG_ITEM(kTagType_GPS_TIMESTAMP)
            SAVE_TAG_ITEM(kTagType_GPS_PROCESSING_METHOD)
            SAVE_TAG_ITEM(kTagType_GPS_DATESTAMP)
            SAVE_TAG_ITEM(kTagType_MODEL)
            SAVE_TAG_ITEM(kTagType_ORIENTATION)
            SAVE_TAG_ITEM(kTagType_DATETIME)
            SAVE_TAG_ITEM(kTagType_MAKE)
            SAVE_TAG_ITEM(kTagType_IMAGE_WIDTH)
            SAVE_TAG_ITEM(kTagType_IMAGE_LENGTH)
            SAVE_TAG_ITEM(kTagType_ISO)
            SAVE_TAG_ITEM(kTagType_EXPOSURE_TIME)
            SAVE_TAG_ITEM(kTagType_APERTURE)
            SAVE_TAG_ITEM(kTagType_FLASH)
            SAVE_TAG_ITEM(kTagType_FOCAL_LENGTH)
            SAVE_TAG_ITEM(kTagType_WHITE_BALANCE)
        SAV_TAG_END()
    } catch (...) {
        MMLOGE("no mem\n");
    }
}


mm_status_t ExifImp::save()
{
    MMLOGI("+\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t ExifImp::saveAs(const char * path)
{
    MMLOGI("+\n");
    return MM_ERROR_SUCCESS;
}

mm_status_t ExifImp::close(bool save/* = false*/)
{
    MMLOGI("+\n");
    doClose();
    return MM_ERROR_SUCCESS;
}

void ExifImp::doClose()
{
    MMLOGI("+\n");
    mTags.clear();
    if (mExifData) {
        exif_data_unref(mExifData);
    }

    mFile = "";
    SET_STATE(kStateIdle);
}

bool ExifImp::isDirty() const
{
    return mState == kStateDirty;
}

mm_status_t ExifImp::getTag(TagType tag, std::string & value)
{
    try {
        TagInfoSP info = mTags.at(tag);
        char * buf = new char[info->mEntry->size + 1];

        exif_entry_get_value((ExifEntry*)info->mEntry, buf, info->mEntry->size);
        buf[info->mEntry->size] = '\0';
        MMLOGV("tag: (%d, %d, %d, %s)\n", tag, info->mEntry->format, info->mEntry->size, buf);
        value = buf;
        delete []buf;
        return MM_ERROR_SUCCESS;
    } catch (...) {
        MMLOGE("tag %d not exists\n", tag);
        return MM_ERROR_NOTEXISTS;
    }
}


#define GET_INT_TAG_ITEM(_tag) do {\
    if (tag == _tag) {\
        TagInfoSP tagInfo = mTags.at(_tag);\
        ExifEntry * entry = (ExifEntry*)tagInfo->mEntry;\
        switch (entry->format) {\
            case EXIF_FORMAT_BYTE:\
            case EXIF_FORMAT_SBYTE:\
                value = entry->data[0];\
                MMLOGV("tag: (0x%x, %d, %d)\n", tag, entry->format, value);\
                return MM_ERROR_SUCCESS;\
            case EXIF_FORMAT_SHORT:\
                value = exif_get_short(entry->data, exif_data_get_byte_order(entry->parent->parent));\
                MMLOGV("tag: (0x%x, %d, %d)\n", tag, entry->format, value);\
                return MM_ERROR_SUCCESS;\
            case EXIF_FORMAT_SSHORT:\
                value = exif_get_sshort(entry->data, exif_data_get_byte_order(entry->parent->parent));\
                MMLOGV("tag: (0x%x, %d, %d)\n", tag, entry->format, value);\
                return MM_ERROR_SUCCESS;\
            case EXIF_FORMAT_LONG:\
                value = exif_get_long(entry->data, exif_data_get_byte_order(entry->parent->parent));\
                MMLOGV("tag: (0x%x, %d, %d)\n", tag, entry->format, value);\
                return MM_ERROR_SUCCESS;\
            case EXIF_FORMAT_SLONG:\
                value = exif_get_slong(entry->data, exif_data_get_byte_order(entry->parent->parent));\
                MMLOGV("tag: (0x%x, %d, %d)\n", tag, entry->format, value);\
                return MM_ERROR_SUCCESS;\
            default:\
                MMLOGV("tag: (0x%x, %d), unrecognized format\n", tag, entry->format);\
                return MM_ERROR_UNSUPPORTED;\
        }\
    }\
} while(0)

mm_status_t ExifImp::getIntTag(TagType tag, int & value)
{
    try {
        GET_INT_TAG_ITEM(kTagType_GPS_ALTITUDE_REF);
        GET_INT_TAG_ITEM(kTagType_ORIENTATION);
        GET_INT_TAG_ITEM(kTagType_IMAGE_WIDTH);
        GET_INT_TAG_ITEM(kTagType_IMAGE_LENGTH);
        GET_INT_TAG_ITEM(kTagType_ISO);
        GET_INT_TAG_ITEM(kTagType_FLASH);
        GET_INT_TAG_ITEM(kTagType_WHITE_BALANCE);

        MMLOGI("tag %d unsupported\n", tag);
        return MM_ERROR_UNSUPPORTED;
    } catch (...) {
        MMLOGE("tag %d not exists\n", tag);
        return MM_ERROR_NOTEXISTS;
    }
}

mm_status_t ExifImp::getDoubleTag(TagType tag, double & value)
{
    MMLOGE("no entry need\n");
    return MM_ERROR_UNSUPPORTED;
}

#define GET_RATIONAL_TAG_ITEM(_tag) do {\
    if (tag == _tag) {\
        TagInfoSP tagInfo = mTags.at(_tag);\
        ExifEntry * entry = (ExifEntry*)tagInfo->mEntry;\
        switch (entry->format) {\
            case EXIF_FORMAT_RATIONAL:\
                {\
                    ExifRational r = exif_get_rational(entry->data, exif_data_get_byte_order(entry->parent->parent));\
                    numerator = r.numerator;\
                    denominator = r.denominator;\
                    MMLOGV("tag: (0x%x, %d, %d/%d)\n", tag, entry->format, numerator, denominator);\
                    return MM_ERROR_SUCCESS;\
                }\
            case EXIF_FORMAT_SRATIONAL:\
                {\
                    ExifSRational r = exif_get_srational(entry->data, exif_data_get_byte_order(entry->parent->parent));\
                    numerator = r.numerator;\
                    denominator = r.denominator;\
                    MMLOGV("tag: (0x%x, %d, %d/%d)\n", tag, entry->format, numerator, denominator);\
                    return MM_ERROR_SUCCESS;\
                }\
            default:\
                MMLOGV("tag: (0x%x, %d), unrecognized\n", tag, entry->format);\
                return MM_ERROR_UNSUPPORTED;\
        }\
    }\
} while(0)

mm_status_t ExifImp::getRationalTag(TagType tag, int & numerator, int & denominator)
{
    try {
        GET_RATIONAL_TAG_ITEM(kTagType_GPS_LATITUDE);
        GET_RATIONAL_TAG_ITEM(kTagType_GPS_LONGITUDE);
        GET_RATIONAL_TAG_ITEM(kTagType_GPS_ALTITUDE);
        GET_RATIONAL_TAG_ITEM(kTagType_GPS_TIMESTAMP);
        GET_RATIONAL_TAG_ITEM(kTagType_EXPOSURE_TIME);
        GET_RATIONAL_TAG_ITEM(kTagType_APERTURE);
        GET_RATIONAL_TAG_ITEM(kTagType_FOCAL_LENGTH);

        MMLOGI("tag %d unsupported\n", tag);
        return MM_ERROR_UNSUPPORTED;
    } catch (...) {
        MMLOGE("tag %d not exists\n", tag);
        return MM_ERROR_NOTEXISTS;
    }
}

mm_status_t ExifImp::setTag(TagType tag, const char * value)
{
    doRemoveTag(tag);
    SET_STATE(kStateDirty);
    return MM_ERROR_UNSUPPORTED;
}

mm_status_t ExifImp::setIntTag(TagType tag, int value)
{
    doRemoveTag(tag);
    SET_STATE(kStateDirty);
    return MM_ERROR_UNSUPPORTED;
}

mm_status_t ExifImp::setDoubleTag(TagType tag, double value)
{
    doRemoveTag(tag);
    SET_STATE(kStateDirty);
    return MM_ERROR_UNSUPPORTED;
}

mm_status_t ExifImp::setRationalTag(TagType tag, int numerator, int denominator)
{
    doRemoveTag(tag);
    SET_STATE(kStateDirty);
    return MM_ERROR_UNSUPPORTED;
}

mm_status_t ExifImp::removeTag(TagType tag)
{
    doRemoveTag(tag);
    SET_STATE(kStateDirty);
    return MM_ERROR_UNSUPPORTED;
}

void ExifImp::doRemoveTag(TagType tag)
{
    std::map<int, TagInfoSP>::iterator i = mTags.find(tag);
    if (i == mTags.end()) {
        MMLOGV("remove tag: %d, not exists\n", tag);
        return;
    }
    TagInfoSP info = i->second;
    MMLOGV("remove tag: %d\n", tag);
    exif_content_remove_entry((ExifContent*)info->mContent, (ExifEntry*)info->mEntry);
    mTags.erase(i);
}

}
