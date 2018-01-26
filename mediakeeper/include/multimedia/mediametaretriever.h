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

#ifndef __mediametaretriever_H
#define __mediametaretriever_H

#include <map>
#include <string>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mm_buffer.h>
#include <multimedia/media_meta.h>
#include <multimedia/video_frame.h>
#include <multimedia/rgb.h>

namespace YUNOS_MM {
/*
MEDIA_ATTR_TRACK_NUMBER
MEDIA_ATTR_ALBUM "album" -- add
MEDIA_ATTR_ARTIST "artist"
MEDIA_ATTR_AUTHOR
MEDIA_ATTR_COMPOSER
MEDIA_ATTR_DATE
MEDIA_ATTR_GENRE
MEDIA_ATTR_TITLE
MEDIA_ATTR_YEAR
MEDIA_ATTR_DURATION
MEDIA_ATTR_WRITER
MEDIA_ATTR_MIME
MEDIA_ATTR_ALBUM_ARTIST -- to "album_artist"
MEDIA_ATTR_HAS_AUDIO -- add, self add, "y" or "n"
MEDIA_ATTR_HAS_VIDEO -- add, self add, "y" or "n"
MEDIA_ATTR_WIDTH -- self add
MEDIA_ATTR_HEIGHT -- self add
MEDIA_ATTR_BIT_RATE
MEDIA_ATTR_LANGUAGE
MEDIA_ATTR_LOCATION
MEDIA_ATTR_ROTATION
 */

class MediaMetaRetriever {
public:
    static MediaMetaRetriever * create();
    static void destroy(MediaMetaRetriever * retriever);

public:
    virtual ~MediaMetaRetriever(){}

public:
    virtual bool initCheck() = 0;
    virtual mm_status_t setDataSource(
            const char * dataSourceUrl,
            const std::map<std::string, std::string> * headers = NULL) = 0;
    virtual mm_status_t setDataSource(int fd, int64_t offset, int64_t length) = 0;
    virtual mm_status_t extractMetadata(MediaMetaSP & fileMeta, MediaMetaSP & audioMeta, MediaMetaSP & videoMeta) = 0;
    virtual VideoFrameSP extractVideoFrame(int64_t timeUs, int32_t * memCosts = NULL) = 0;
    virtual VideoFrameSP extractVideoFrame(int64_t timeUs,
                RGB::Format format,
                uint32_t scaleNum = 1,
                uint32_t scaleDenom = 1) = 0;
    virtual CoverSP extractCover() = 0;

protected:
    MediaMetaRetriever(){}

    MM_DISALLOW_COPY(MediaMetaRetriever)
    DECLARE_LOGTAG()
};

}

#endif // __mediametaretriever_H


