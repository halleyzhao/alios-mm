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



MM_LOG_DEFINE_MODULE_NAME("PlaybackRemote-Test");

using namespace YUNOS_MM;

// This PlaybackRemote will interact with js PlaybackChannel test
class PlaybackRemoteTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

static PlaybackChannelTestHelperSP helper;
static bool mUsingPlaybackChannelManager = false;

const static GOptionEntry entries[] = {
    {"PlaybackRemoteCreatedType", 'm', 0, G_OPTION_ARG_NONE, &mUsingPlaybackChannelManager, "PlaybackRemote created from PlaybackChannelManager", NULL},
    {NULL}
};


#if 1
TEST_F(PlaybackRemoteTest, controllerTest) {
    fprintf(stderr, "[PlaybackRemote-Test] PlaybackRemoteTest start\n");

    fprintf(stderr, "please input 'c' to create PlaybackRemote\n");
    ssize_t count = 0;
    char buffer[128] = {0};
    if ((count = read(STDIN_FILENO, buffer, sizeof(buffer)-1) < 0)){
        PRINTF("read from stdin err, count is %d\n", count);
        return;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    count = strlen(buffer);
    fprintf(stderr, "read return %zu bytes, %s\n", count, buffer);
    char ch = buffer[0];
    if (ch != 'c') {
        fprintf(stderr, "invalid %c\n", ch);
        return;
    }

    helper->controllerCreate(true);

    fprintf(stderr, "[PlaybackRemote-Test] PlaybackRemote create success\n");
}

TEST_F(PlaybackRemoteTest, PlaybackRemoteGetProperty) {

    fprintf(stderr, "please input 'g' to get MediaMeta and PlaybackState\n");
    ssize_t count = 0;
    char buffer[128] = {0};
    if ((count = read(STDIN_FILENO, buffer, sizeof(buffer)-1) < 0)){
        PRINTF("read from stdin err, count is %d\n", count);
        return;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    count = strlen(buffer);
    fprintf(stderr, "read return %zu bytes, %s\n", count, buffer);
    char ch = buffer[0];
    if (ch != 'g') {
        fprintf(stderr, "invalid %c\n", ch);
        return;
    }


    fprintf(stderr, "[PlaybackRemote-Test] PlaybackRemoteGetProperty start\n");
    helper->controllerGet();

    fprintf(stderr, "[PlaybackRemote-Test] PlaybackRemoteGetProperty end\n");
}


TEST_F(PlaybackRemoteTest, PlaybackRemoteMediaAction) {
    fprintf(stderr, "[PlaybackRemote-Test] PlaybackRemoteMediaAction start\n");
    helper->controllerTransportControl();
    helper->remoteSendPlaybackEvent();

    helper->managerRelease();
    helper->channelRelease();

    fprintf(stderr,"[PlaybackRemote-Test] remove mControllerListener and mManagerListener\n");
    fprintf(stderr,"[PlaybackRemote-Test] test over\n");


    usleep(5*1000);
}

#endif

int main(int argc, char** argv)
{
    int ret;
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new(MM_LOG_TAG);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, TRUE);

    if (!g_option_context_parse(context, &argc, (char ***)&argv, &error)) {
            PRINTF("option parsing failed: %s\n", error->message);
            return -1;
    }
    g_option_context_free(context);
    helper.reset(new PlaybackChannelTestHelper());

    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
    } catch (...) {
        MMLOGE("InitGoogleTest failed!");
        ret = -1;
    }

    return ret;
}
