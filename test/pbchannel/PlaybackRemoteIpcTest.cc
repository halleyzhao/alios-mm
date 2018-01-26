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
#include <glib.h>
#include <gtest/gtest.h>
#include <vector>
#include <unistd.h>
#include <multimedia/PlaybackEvent.h>
#include <multimedia/PlaybackChannel.h>
#include <multimedia/PlaybackRemote.h>
#include <multimedia/PlaybackChannelManager.h>
#include "PlaybackChannelTestHelper.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

MM_LOG_DEFINE_MODULE_NAME("PlaybackRemoteIpc-Test");

using namespace YUNOS_MM;

int main(int argc, char** argv)
{
    fprintf(stderr,"[PlaybackRemoteIpc-Test] test start\n");
    DEBUG("usingPlaybackChannelManager %s, remoteId %s", argv[0], argv[1]);
    if (!argv[0] || !argv[1]) {
        return -1;
    }
    PlaybackChannelTestHelperSP helper;
    helper.reset(new PlaybackChannelTestHelper());

    // Using remoteId to create PlaybackRemote
    if (!strcmp(argv[0], "0")) {
        DEBUG("Using remoteId to create PlaybackRemote");
        const char *remoteId = argv[1];
        helper->controllerCreate(false, remoteId);
        fprintf(stderr, "[PlaybackRemoteIpc-Test] create PlaybackRemote success\n");
    } else {
        DEBUG("Using PlaybackChannelManager to create PlaybackRemote");
        helper->controllerCreate(true);
        fprintf(stderr, "[PlaybackRemoteIpc-Test] using PlaybackChannelManager to create PlaybackRemote success\n");
    }
    fprintf(stderr, "[PlaybackRemoteIpc-Test] PlaybackRemote setListener success\n");
    fprintf(stderr, "[PlaybackRemoteIpc-Test] sleep 10s for PlaybackChannel set property\n");

    // sleep for PlaybackChannel set property
    usleep(10*1000*1000);

    fprintf(stderr, "[PlaybackRemoteIpc-Test] PlaybackRemoteGetProperty start\n");
    helper->controllerGet();
    fprintf(stderr, "[PlaybackRemoteIpc-Test] PlaybackRemoteGetProperty end\n");

    ///////////////////////////////////////////////////////////////////
    fprintf(stderr, "[PlaybackRemoteIpc-Test] PlaybackRemoteMediaAction start\n");
    helper->controllerTransportControl();
    helper->remoteSendPlaybackEvent(true);

    helper->controllerRelease();
    helper->managerRelease();
    fprintf(stderr,"[PlaybackRemoteIpc-Test] remove mControllerListener and mManagerListener\n");
    fprintf(stderr,"[PlaybackRemoteIpc-Test] test over\n");

    return 0;
}
