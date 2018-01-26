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

#include <multimedia/mediacollector.h>
#ifdef MEDIACOLLECT_SINGLE_THREAD
#include <mediacollector_imp.h>
#else
#include <mediacollector_main.h>
#endif
#include "mediacollector_char_converter.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

namespace YUNOS_MM {

DEFINE_LOGTAG(MediaCollectorWatcher::MediaCollectorWatcher)
DEFINE_LOGTAG(MediaCollector)

MediaCollectorWatcher::MediaCollectorWatcher(const char *locale) : mLocale(locale)
{
    MMLOGI("locale: %s\n", locale);
    if (mLocale.empty()) {
        mLocalCharsetStr = "";
        mLocaleVal = 0;
        return;
    }

    mLocalCharsetStr = getCharSetByCountry(locale);
    if (mLocalCharsetStr == "shift-jis") {
        mLocaleVal = CharConverter::kCharSetShiftJIS;
        return;
    }

    if (mLocalCharsetStr == "EUC-KR") {
        mLocaleVal = CharConverter::kCharSetEUCKR;
        return;
    }

    if (mLocalCharsetStr == "gbk") {
        mLocaleVal = CharConverter::kCharSetGBK;
        return;
    }

    if (mLocalCharsetStr == "Big5") {
        mLocaleVal = CharConverter::kCharSetBig5;
        return;
    }

    if (mLocalCharsetStr == "cp1251") {
        mLocaleVal = CharConverter::kCharSetCP1251;
        return;
    }

    if (mLocalCharsetStr == "ISO-8859-1") {
        mLocaleVal = CharConverter::kCharSetISO8859;
        return;
    }

    mLocaleVal = 0;
    MMLOGI("local: %s, %u\n", mLocalCharsetStr.c_str(), mLocaleVal);
}

mm_status_t MediaCollectorWatcher::addStringAttrib(const char * name, const char * tag)
{
    char * dst = NULL;
    mm_status_t ret = CharConverter::convert(mLocaleVal, tag, dst);
    switch (ret) {
        case MM_ERROR_SUCCESS:
            ret = addAttrib(name, dst);
            MM_RELEASE_ARRAY(dst);
            break;
        case MM_ERROR_OP_FAILED:
        case MM_ERROR_INVALID_PARAM:
            ret = MM_ERROR_SUCCESS;
            break;
        case MM_ERROR_SKIPPED:
        default:
            ret = addAttrib(name, tag);
            break;
    }

    return ret;
}

MediaCollector::MediaCollector()
{
    MMLOGV("+\n");
}

MediaCollector::~MediaCollector()
{
    MMLOGV("+\n");
}

/*static */MediaCollector * MediaCollector::create(MediaCollectorWatcher * watcher,
                    int thumbMaxSizeSqrt,
                    int microThumbMaxSizeSqrt,
                    const char * thumbPath)
{
    MMLOGI("+\n");
    if ( !watcher ) {
        MMLOGE("invalid param\n");
        return NULL;
    }

#ifdef MEDIACOLLECT_SINGLE_THREAD
    MMLOGI("use single thread\n");
    MediaCollectorImp * m = new MediaCollectorImp(watcher,
                                                        thumbMaxSizeSqrt,
                                                        microThumbMaxSizeSqrt,
                                                        thumbPath);
#else
    MMLOGI("use multi thread\n");
    MediaCollectorMain* m = new MediaCollectorMain(watcher,
                                                    thumbMaxSizeSqrt,
                                                    microThumbMaxSizeSqrt,
                                                    thumbPath);
#endif
    if ( !m ) {
        MMLOGE("no mem\n");
        return NULL;
    }

    if ( !m->initCheck() ) {
        MMLOGE("initcheck failed\n");
        delete m;
        return NULL;
    }

    MMLOGI("-\n");
    return m;
}

/*static */void MediaCollector::destroy(MediaCollector * p)
{
    MMLOGI("+\n");
    MM_RELEASE(p);
    MMLOGI("-\n");
}

}
