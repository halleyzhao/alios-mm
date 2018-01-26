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
#include <multimedia/PlaybackChannelManager.h>
#include "PlaybackChannelTestHelper.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>

MM_LOG_DEFINE_MODULE_NAME("PlaybackChannelManager-Test");

using namespace YUNOS_MM;
static PlaybackChannelManagerSP mManager;
static std::vector<PlaybackRemoteSP> mRemotes;
static PlaybackRemoteSP mRemote;
static ManagerListenerSP mManagerListener;
static bool mWaitForChannel = false;

class PlaybackChannelManagerTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

#if 0
TEST_F(PlaybackChannelManagerTest, managerCreateTest) {

    INFO("PlaybackChannelManager test start\n");

    mManager = PlaybackChannelManager::getInstance();
    if (!mManager) {
        ERROR("failed to create PlaybackChannelManager\n");
        return;
    }

    for (int i = 0; i < 300; i++) {
        std::string id = mManager->createChannel("musicplayer", "musicplayer.yunos.com", 0);
        if (id == "InvalidChannel") {
            ERROR("id %s", id.c_str());
            return;
        }
        fprintf(stderr, "createchannel loop %d\n", i);
    }

}
#endif

#if 1
TEST_F(PlaybackChannelManagerTest, managerTest) {

    INFO("PlaybackChannelManager test start\n");

    mManager = PlaybackChannelManager::getInstance();
    if (!mManager) {
        ERROR("failed to create PlaybackChannelManager\n");
        return;
    }
    mManagerListener.reset(new PlaybackChannelManagerListener());
    mManager->addListener(mManagerListener.get());

    mRemotes.clear();
    mm_status_t status = mManager->getEnabledChannels(mRemotes);
    ASSERT(status == MM_ERROR_SUCCESS);
    fprintf(stderr, "mContorller.size %d\n", mRemotes.size());
    INFO("PlaybackChannelManager test end\n");

    if (!mWaitForChannel) {
        return;
    }
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
}

TEST_F(PlaybackChannelManagerTest, longPressTest) {
    if (mWaitForChannel) {
        return;
    }
    if (!mManager) {
        mManager = PlaybackChannelManager::getInstance();
        if (!mManager) {
            ERROR("failed to create PlaybackChannelManager\n");
            return;
        }
    }
    PlaybackEventSP event;
    uint32_t state = 1; // down
    uint32_t keyValue = PlaybackEvent::Keys::kHeadsetHook;
    uint32_t repeatCount = 0;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    mm_status_t status = mManager->dispatchPlaybackEvent(event, true);

    usleep(500*1000);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 3; // longpress
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);

    usleep(500*1000);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 2; // repeat
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);

    usleep(500*1000);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 2; // repeat
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);


    usleep(500*1000);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    repeatCount++;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
}
#endif


TEST_F(PlaybackChannelManagerTest, testMediaKeyNotifyToPlaybackChannelJs) {
    if (mWaitForChannel) {
        return;
    }
    if (!mManager) {
        mManager = PlaybackChannelManager::getInstance();
        if (!mManager) {
            ERROR("failed to create PlaybackChannelManager\n");
            return;
        }
    }
    PlaybackEventSP event;
    uint32_t state = 1; // down
    uint32_t keyValue = PlaybackEvent::Keys::kMediaPlay;
    uint32_t repeatCount = 0;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    mm_status_t status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);


    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaPause;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);


    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaPrevious;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);

    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaNext;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);


    state = 1;
    keyValue = PlaybackEvent::Keys::kMediaStop;
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
    state = 0; // up
    event.reset(new PlaybackEvent(state, keyValue, repeatCount, 0xf));
    status = mManager->dispatchPlaybackEvent(event, true);
    EXPECT_TRUE(status ==  MM_ERROR_SUCCESS);
}


const static GOptionEntry entries[] = {
    {"waitForChannel", 'w', 0, G_OPTION_ARG_NONE, &mWaitForChannel, "wait for channel", NULL},
    {NULL}
};

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

    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
    } catch (...) {
        MMLOGE("InitGoogleTest failed!");
        ret = -1;
    }

    return ret;
}
