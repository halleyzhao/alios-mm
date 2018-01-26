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

#ifndef __yunos_mm_exif_imp_H
#define __yunos_mm_exif_imp_H

#include <map>

#include <libexif/exif-data.h>
#include <libexif/exif-system.h>
#include <libexif/exif-content.h>

#include "multimedia/exif.h"

namespace YUNOS_MM {

class ExifImp : public Exif {
public:
    ExifImp();
    ~ExifImp();

public:
    virtual mm_status_t open(const char * uri);
    virtual mm_status_t open(int fd, int64_t offset, int64_t length);
    virtual mm_status_t save();
    virtual mm_status_t saveAs(const char * path);
    virtual mm_status_t close(bool save = false);
    virtual bool isDirty() const;

    virtual mm_status_t getTag(TagType tag, std::string & value);
    virtual mm_status_t getIntTag(TagType tag, int & value);
    virtual mm_status_t getDoubleTag(TagType tag, double & value);
    virtual mm_status_t getRationalTag(TagType tag, int & numerator, int & denominator);

    virtual mm_status_t setTag(TagType tag, const char * value);
    virtual mm_status_t setIntTag(TagType tag, int value);
    virtual mm_status_t setDoubleTag(TagType tag, double value);
    virtual mm_status_t setRationalTag(TagType tag, int numerator, int denominator);

    virtual mm_status_t removeTag(TagType tag);

private:
    static void exifDataForEachFunc(ExifContent *content, void *callback_data);
    void exifDataForEachFunc(ExifContent *content);
    static void exifContentForEachFunc(ExifEntry *entry, void *callback_data);
    void exifContentForEachFunc(ExifEntry *entry);
    void doRemoveTag(TagType tag);

    void doClose();


    enum State {
        kStateIdle,
        kStateOpened,
        kStateDirty
    };

    struct TagInfo {
        TagInfo(ExifContent * content, ExifEntry * entry);
        const ExifContent * mContent;
        const ExifEntry * mEntry;

    private:
        TagInfo();
    };
    typedef MMSharedPtr<TagInfo> TagInfoSP;


private:
    State mState;
    ExifData * mExifData;
    ExifContent * mCurContent;
    std::map<int, TagInfoSP> mTags;
    std::string mFile;

private:
    MM_DISALLOW_COPY(ExifImp)
    DECLARE_LOGTAG()
};


}

#endif


