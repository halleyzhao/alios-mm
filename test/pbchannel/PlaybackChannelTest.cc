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



MM_LOG_DEFINE_MODULE_NAME("PlaybackChannel-Test");

using namespace YUNOS_MM;

class PlaybackChannelTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};


bool mUsingPlaybackChannelManager = false;
bool mPlaybackRemoteIpc = false;
bool mChannelCreateTest = false;
bool mControllerCreateTest = false;

PlaybackChannelSP mChannel1;
ChannelListenerSP mChannelListener1;

static PlaybackChannelTestHelperSP helper;

#if 0
TEST_F(PlaybackChannelTest, ChannelCreateAndDestroy) {
    if (mPlaybackRemoteIpc)
        return;
    if (mChannelCreateTest)
        return;
    if (mControllerCreateTest)
        return;

    mManager = PlaybackChannelManager::getInstance();
    if (!mManager) {
        ERROR("failed to create PlaybackChannelManager\n");
        return;
    }
    mm_status_t status = mManager->getEnabledChannels(mRemote);
    ASSERT(status == MM_ERROR_SUCCESS);
    mManagerListener.reset(new PlaybackChannelManagerListener());
    mManager->addListener(mManagerListener.get());


    INFO("ChannelCreateAndDestroy start\n");

    PlaybackChannelSP channel1 = PlaybackChannel::create("MusicPlayer", "page://musicplayer.yunos.com/musicplayer");
    if (!channel1) {
        ERROR("failed to create PlaybackChannel\n");
        return;
    }
    std::string remoteId = channel1->getPlaybackRemoteId();
    PlaybackRemoteSP controller1 = PlaybackRemote::create(remoteId.c_str());
    if (!controller1) {
        ERROR("failed to create PlaybackRemote\n");
        return;
    }
    DEBUG("getPlaybackRemoteId, remoteId: %s", remoteId.c_str());

    // second time
    PlaybackChannelSP channel2 = PlaybackChannel::create("MusicPlayer1", "page://musicplayer.yunos.com/musicplayer1");
    if (!channel2) {
        ERROR("failed to create PlaybackChannel\n");
        return;
    }
    remoteId = channel2->getPlaybackRemoteId();
    PlaybackRemoteSP controller2 = PlaybackRemote::create(remoteId.c_str());
    if (!controller2) {
        ERROR("failed to create PlaybackRemote\n");
        return;
    }
    DEBUG("getPlaybackRemoteId, remoteId: %s", remoteId.c_str());
    controller1.reset();
    controller2.reset();

    status = mManager->getEnabledChannels(mRemote);
    ASSERT(status == MM_ERROR_SUCCESS);
    fprintf(stderr, "mRemote.size %d\n", mRemotes.size());


    controller1 = mRemote[0];
    controller2 = mRemote[1];
    ControllerListenerSP listener1;
    ControllerListenerSP listener2;
    listener1.reset(new PlaybackRemoteListener());
    controller1->addListener(listener1.get());
    listener2.reset(new PlaybackRemoteListener());
    controller2->addListener(listener2.get());

    channel1.reset(); //invoke PlaybackChannel::destroy() and notify channelDestroy to PlaybackRemote and PlaybackChannelManager
    mRemote.clear();
    status = mManager->getEnabledChannels(mRemote);
    ASSERT(status == MM_ERROR_SUCCESS);
    fprintf(stderr, "mRemote.size %d\n", mRemotes.size());
    mRemote.clear();

    channel2.reset();

    mManager->removeListener(mManagerListener.get());
    controller1->removeListener(listener1.get()); //channel is destroyed, so removeListener will be failed
    controller2->removeListener(listener2.get());
    INFO("ChannelCreateAndDestroy end\n");
}
#endif

// test adaptor count in server progress
#if 0
TEST_F(PlaybackChannelTest, createChannelRepeatedly) {
    for (int i =0 ; i < 500; i++){
        INFO("ChannelConfigure start, i = %d\n", i);
        helper->channelCreate();
        helper->channelRelease(0);
    }
}
#endif

#if 1
TEST_F(PlaybackChannelTest, ChannelCreateAndConfigure) {
    if (mChannelCreateTest) {
        INFO("ChannelConfigure start\n");
        helper->channelCreate();
        helper->channelConfigure(false /*isNeedMediaButton*/);


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

        helper->channelRelease();
    }

}
#endif

#if 1
TEST_F(PlaybackChannelTest, InSameProcess) {
    if (!mPlaybackRemoteIpc) {
        INFO("ChannelConfigure start\n");
        helper->channelCreate();
        if (!mUsingPlaybackChannelManager) {
            std::string remoteId = helper->getChannel()->getPlaybackRemoteId();
            helper->controllerCreate(mUsingPlaybackChannelManager, remoteId.c_str());
        } else {
            helper->controllerCreate(mUsingPlaybackChannelManager);
        }

        helper->channelConfigure(false);

        helper->controllerGet();
        helper->controllerTransportControl();
        helper->remoteSendPlaybackEvent();

        helper->channelRelease();
        helper->controllerRelease();
        helper->managerRelease();
        usleep(5*1000);

        INFO("ChannelConfigure end\n");
    }
}

#endif

#if 1
TEST_F(PlaybackChannelTest, ipcTest) {
    if (mPlaybackRemoteIpc) {
        INFO("ChannelConfigure start\n");

        helper->channelCreate();
        // fprintf(stderr, "\tparent pid = %d\n",getpid());

        std::string remoteId = helper->getChannel()->getPlaybackRemoteId();
        DEBUG("getPlaybackRemoteId, remoteId: %s", remoteId.c_str());

        pid_t child;
        if((child = fork()) == -1) {
            fprintf(stderr, "fork error");
            return;
        } else if(child == 0) {
            char tmp[16]  ={0};
            sprintf(tmp, "%d", mUsingPlaybackChannelManager);
            int i = execl("/usr/bin/pbremoteipc-test", tmp, remoteId.c_str(), (char *)0);
            fprintf(stderr, "i: %d, errno: %d(%s)\n", i, errno, strerror(errno));
            if (i < 0) {
                exit(EXIT_FAILURE);
            }
            return;
        } else {

            fprintf(stderr, "[PlaybackChannelTest] sleep 3s to wait PlaybackRemote create success\n");
            usleep(3*1000*1000); //to wait PlaybackRemote create success

            helper->channelConfigure(false);

            fprintf(stderr, "[PlaybackChannelTest] sleep 15s to wait PlaybackRemote cap success\n");
            usleep(15*1000*1000);
            helper->channelRelease();
        }

        INFO("ChannelConfigure end\n");
    }
}
#endif


const static GOptionEntry entries[] = {
    {"PlaybackRemoteCreatedType", 'm', 0, G_OPTION_ARG_NONE, &mUsingPlaybackChannelManager, "PlaybackRemote created from PlaybackChannelManager", NULL},
    {"PlaybackRemoteCreatedTypeIpc", 'p', 0, G_OPTION_ARG_NONE, &mPlaybackRemoteIpc, "PlaybackRemote created from PlaybackChannelManager ipc", NULL},
    {"PlaybackChannelCreateIpc", 's', 0, G_OPTION_ARG_NONE, &mChannelCreateTest, "mChannelCreateTest repeatedly", NULL},
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
