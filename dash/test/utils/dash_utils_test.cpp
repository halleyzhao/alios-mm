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

#include <stdio.h>
#include <unistd.h>

#include <libdash.h>
#include <IMPD.h>
#include <IChunk.h>

#include <AdaptationSetStream.h>

#include <DashStream.h>
#include <DashUtils.h>

using namespace dash;
using namespace dash::mpd;
using namespace dash::network;

//using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

using namespace YUNOS_MM::YUNOS_DASH;

#define PRINTF(format, ...)  fprintf(stdout, format "\n", ##__VA_ARGS__)

char *mpdUrl = NULL;
static int seekMs = 0;

static void
usage(int error_code)
{
    fprintf(stderr, "Usage: libdash-test [OPTIONS]\n\n"
        "  -i\tmpd url\n"
        "  -s\tseek pts\n"
        "  -h\tThis help text\n\n");

    exit(error_code);
}

void parseCommandLine(int argc, char * const argv[])
{
    int res;

    while ((res = getopt(argc, argv, "hi:s:")) >= 0) {
        switch (res) {
            case 'i':
                mpdUrl= optarg;
                PRINTF("MPD URL:%s", mpdUrl);
                break;
            case 's':
                sscanf(optarg, "%d", &seekMs);
                PRINTF("seek to:%dms", seekMs);
                break;
            case 'h':
                usage(-1);
                break;
            default:
                usage(-1);
                break;
        }
    }
}

#define CHECK_NULL(val)                                            \
    if (!val) {                                                    \
        PRINTF("[%s %d] check null fail", __func__, __LINE__);     \
        exit(-1);                                                  \
    }

int main(int argc, char* const argv[]) {


    int periodIdx = 0;
    int adaptionSetIdx = 0;
    int representationIdx = 0;
    DashStreamSP dashStream;

    parseCommandLine(argc, argv);

    IDASHManager *manager = CreateDashManager();
    CHECK_NULL(manager)

    if (!mpdUrl) {
        mpdUrl = "http://dash.edgesuite.net/akamai/bbb_30fps/bbb_30fps.mpd";
        PRINTF("mpd url is not provided, use default: %s", mpdUrl);
    }

    PRINTF("open mpd\n");
    IMPD *mpd = manager->Open(mpdUrl);
    CHECK_NULL(mpd);
    PRINTF("open mpd done\n");

    IPeriod *period;
    IAdaptationSet *adaptationSet;
    IRepresentation *representation;

    period = mpd->GetPeriods().at(periodIdx);
    CHECK_NULL(period);

    adaptationSet = period->GetAdaptationSets().at(adaptionSetIdx);
    CHECK_NULL(adaptationSet);

    representation = adaptationSet->GetRepresentation().at(representationIdx);
    CHECK_NULL(representation);

    //dashStream.reset(DashStream::createStreamByMpd(mpd, DashStream::kStreamTypeVideo));
    //CHECK_NULL(dashStream);

    DashUtils::RepresentationType type = DashUtils::getRepresentationType(period,
                                                                          adaptationSet,
                                                                          representation);
    PRINTF("RepresentationType is %d", type);

#define CHECK_STATUS(s)                                 \
    if (s != MM_ERROR_SUCCESS) {                        \
        PRINTF("[%d]status is %d", __LINE__, s);        \
        exit(-1);                                       \
    }

    int p, start;
    mm_status_t status = DashUtils::getPeroidFromTimeMs(mpd, p, start, seekMs);
    CHECK_STATUS(status);

    period = mpd->GetPeriods().at(p);
    CHECK_NULL(period);

    IAdaptationSet *adaptationSet1;
    IRepresentation *representation1;

    status = DashUtils::selectRepresentationFromPeroid(period,
                                            adaptationSet,
                                            representation,
                                            adaptationSet1,
                                            representation1);
    CHECK_STATUS(status);
    PRINTF("current adaptation Set/Representation:%p %p Seek to:%p %p",
           adaptationSet, representation, adaptationSet1, representation1);

    int index;
    status = DashUtils::getSegmentWithTimeMs(mpd,
                                             period,
                                             adaptationSet1,
                                             representation1,
                                             start,
                                             seekMs,
                                             index);
    CHECK_STATUS(status);
    size_t size = DashUtils::getSegmentNumber(mpd, p, adaptationSet1, representation1);
    PRINTF("segment number is %d, segment index is %d", size, index);

    if (mpd)
        delete mpd;

    if (manager)
        manager->Delete();


    return 0;
}
