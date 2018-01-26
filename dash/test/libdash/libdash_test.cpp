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

using namespace dash;
using namespace dash::mpd;
using namespace dash::network;

//using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

#define PRINTF(format, ...)  fprintf(stdout, format "\n", ##__VA_ARGS__)

char *mpdUrl = NULL;

static void
usage(int error_code)
{
    fprintf(stderr, "Usage: libdash-test [OPTIONS]\n\n"
        "  -i\tmpd url\n"
        "  -h\tThis help text\n\n");

    exit(error_code);
}

void parseCommandLine(int argc, char * const argv[])
{
    int res;

    while ((res = getopt(argc, argv, "hi:")) >= 0) {
        switch (res) {
            case 'i':
                mpdUrl= optarg;
                PRINTF("MPD URL:%s", mpdUrl);
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

class DownloadObserver : public IDownloadObserver {
    public:
    DownloadObserver() : mState(NOT_STARTED) {
    }
    virtual void OnDownloadRateChanged  (uint64_t bytesDownloaded)  {
        PRINTF("download rate changed, %lld bytes downloaded", bytesDownloaded);
    }

    virtual void OnDownloadStateChanged (DownloadState state) {
        PRINTF("download state changed %d", state);
        mState = state;
    }

    DownloadState getState() {
        return mState;
    }

    private:
    DownloadState mState;
};

int main(int argc, char* const argv[]) {


    int periodIdx = 0;
    int adaptionSetIdx = 0;
    int representationIdx = 0;

    parseCommandLine(argc, argv);

    IDASHManager *manager = CreateDashManager();
    CHECK_NULL(manager)

    if (!mpdUrl) {
        mpdUrl = "http://dash.edgesuite.net/akamai/bbb_30fps/bbb_30fps.mpd";
        PRINTF("mpd url is not provided, use default: %s", mpdUrl);
    }

    printf("open mpd\n");
    IMPD *mpd = manager->Open(mpdUrl);
    CHECK_NULL(mpd);
    printf("open mpd done\n");

    IPeriod *period;
    IAdaptationSet *adaptationSet;
    IRepresentation *representation;

    AdaptationSetStream *adaptationSetStream;
    IRepresentationStream *representationStream;

    period = mpd->GetPeriods().at(periodIdx);
    CHECK_NULL(period);

    adaptationSet = period->GetAdaptationSets().at(adaptionSetIdx);
    CHECK_NULL(adaptationSet);

    representation = adaptationSet->GetRepresentation().at(representationIdx);
    CHECK_NULL(representation);

    adaptationSetStream   = new AdaptationSetStream(mpd, period, adaptationSet);
    representationStream  = adaptationSetStream->GetRepresentationStream(representation);
    CHECK_NULL(representationStream);
 
    PRINTF("presentation[%d %d %d] has %d segments",
            periodIdx, adaptionSetIdx, representationIdx, representationStream->GetSize());

    ISegment *seg = NULL;
    seg = representationStream->GetInitializationSegment();
    if (seg) {
        IChunk *chunk = dynamic_cast<IChunk*>(seg);
        CHECK_NULL(chunk);
        PRINTF("has init segment, URI: %s", chunk->AbsoluteURI().c_str());
        delete seg;
    } else {
        PRINTF("no init segment url\n");
    }

    PRINTF("representation has %d segments", representationStream->GetSize());
    for (uint32_t i = 0; i < representationStream->GetSize(); i++) {
        DownloadObserver observer;
        seg = representationStream->GetMediaSegment(i);
        if (!seg) {
            PRINTF("seg %u is NULL", i);
            continue;
        }
        
        IChunk *chunk = dynamic_cast<IChunk*>(seg);
        CHECK_NULL(chunk);

        PRINTF("segment %d:\n%s\n", i, chunk->AbsoluteURI().c_str());

        seg->AttachDownloadObserver(&observer);
        seg->StartDownload();
        do {
            if (observer.getState() == COMPLETED) {
                PRINTF("download complete");
                break;
            }
            usleep(1000*1000);
        } while(1);

        delete seg;
    }

    if (mpd)
        delete mpd;

    if (manager)
        manager->Delete();

    if (adaptationSetStream)
        delete adaptationSetStream;

    return 0;
}
