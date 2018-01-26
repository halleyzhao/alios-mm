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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <glib.h>
#include <gtest/gtest.h>

typedef int status_t;
#include "multimedia/media_attr_str.h"

#include "multimedia/mm_debug.h"
#include "multimedia/media_meta.h"
#include "multimedia/component.h"
#include "multimedia/mm_cpp_utils.h"
#include "multimedia/mediaplayer.h"

#include "VrVideoView.h"
#include "VrViewController.h"

#include "native_surface_help.h"
//#include <WindowSurface.h>

#include <media_surface_texture.h>

#define DEBUG_IDX(format, ...) DEBUG("idx: %d, " format, mDraw, ##__VA_ARGS__);
#define INFO_IDX(format, ...) INFO("idx: %d, " format, mDraw, ##__VA_ARGS__);
#define WARN_IDX(format, ...) WARNING("idx: %d, " format, mDraw, ##__VA_ARGS__);
#define ERROR_IDX(format, ...) ERROR("idx: %d, " format, mDraw, ##__VA_ARGS__);

using namespace YUNOS_MM;
using namespace yunos::yvr;

MM_LOG_DEFINE_MODULE_NAME("VRPlayer-Test")
typedef long	    TTime;

#undef PRE_PLAY_VIDEO
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
#include "SimpleSubWindow.h"
using namespace YunOSMediaCodec;
std::vector<MediaSurfaceTexture*> mst;
static Lock g_draw_anb_lock;

void initMediaTexture(int n) {
    if (!mst[n]) {
        mst[n] = new MediaSurfaceTexture();
    }
}
#endif
int g_player_c = 1;

uint32_t g_surface_width = 1280;
uint32_t g_surface_height = 720;
bool g_use_media_texture = false;
bool g_use_window_surface = false;
bool g_show_surface = false;
bool g_auto_test = false;
bool g_auto_orientation = false;
bool g_loop_player = false;
bool g_loop_player_recreate_player = false;
bool g_loop_player_recreate_surface = false;
uint32_t g_play_time = 0;
uint32_t g_wait_time = 2;

bool g_setDataSource_fd = false;
int32_t loopedCount = 0;
int32_t g_loop_count = 100;
float g_play_rate = 1.0f;
float g_decreased_delta = 0.1f;
float g_increased_delta = 1.0f;
//float g_play_rate_arr[] = {0.1f, 0.2f, 0.4f, 0.5f, 0.8f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
float g_play_rate_arr[] = {0.1f, 0.5f, 1.0f, 1.5f, 1.9f, 2.0f, 3.0f, 3.5f, 4.0f, 8.0f, 16.0f};

int32_t g_play_rate_arr_size = sizeof(g_play_rate_arr)/sizeof(float);
float g_cur_play_rate = 1.0f;
int32_t g_cur_play_rate_index = 2;
static bool g_quit = false;
static bool g_user_quit = false;
static bool g_eos = false; // FIXME, use one for each player

bool g_vr_active = true;

// simple synchronization between main thread and player listener; assumed no lock is required
typedef struct OperationSync {
    bool done;
    int64_t timeout;
} OperationSync;
std::vector<OperationSync> g_opSync;

std::vector<WindowSurface*>ws;
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
std::vector<yunos::libgui::SurfaceController*>sc;
#endif

class TestListener : public MediaPlayer::Listener {
public:
    TestListener(int draw):mDraw(draw),mFrameCount(0), mSurface(NULL) {}
    ~TestListener()
     {
        MMLOGV("+\n");
     }
     int mDraw;
     int mFrameCount;
     WindowSurface* mSurface;

#define TIME_T  int32_t
#define GET_REFERENCE(x)    (&(x))
#define TEST_MediaTypeUnknown   MediaPlayer::TRACK_TYPE_UNKNOWN
#define TEST_MediaTypeVideo     MediaPlayer::TRACK_TYPE_VIDEO
#define TEST_MediaTypeAudio     MediaPlayer::TRACK_TYPE_AUDIO
#define TEST_MediaTypeTimedText MediaPlayer::TRACK_TYPE_TIMEDTEXT
#define TEST_MediaTypeSubtitle  MediaPlayer::TRACK_TYPE_SUBTITLE
#define TEST_MediaTypeCount     (MediaPlayer::TRACK_TYPE_SUBTITLE+1)

//#define TEST_RETURN_ACQUIRE

    virtual void onMessage(int msg, int param1, int param2, const MMParam *meta)
    {
        switch ( msg ) {
            case MSG_PREPARED:
                INFO_IDX("MSG_PREPARED: param1: %d\n", param1);
                if (param1 == MM_ERROR_SUCCESS) {
                    g_opSync[mDraw].done = true;
                }
                break;
            case MSG_STARTED:
                INFO_IDX("MSG_STARTED: param1: %d\n", param1);
                if (param1 == MM_ERROR_SUCCESS) {
                    g_opSync[mDraw].done = true;
                }
                break;
            case MSG_SET_VIDEO_SIZE:
                INFO_IDX("MSG_SET_VIDEO_SIZE: param1: %d, param2: %d\n", param1, param2);
                if ((g_surface_width != (uint32_t)param1) ||
                    (g_surface_height != (uint32_t)param2)) {
                    INFO_IDX("width %d-->%d, height %d-->%d", g_surface_width, param1, g_surface_height, param2);
                    //g_surface_width = param1;
                    //g_surface_height = param2;
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
/*
                    if (ws[mDraw])
                        ((Surface*)ws[mDraw])->setSurfaceResize(
                                               g_surface_width,
                                               g_surface_height);
*/
#endif
                }

                break;
            case MSG_STOPPED:
                INFO_IDX("MSG_STOPPED: param1: %d\n", param1);
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
                g_opSync[mDraw].done = true;
#endif
                break;
             case MSG_SEEK_COMPLETE:
                INFO_IDX("MSG_SEEK_COMPLETE\n");
                break;
             case MSG_PLAYBACK_COMPLETE:
                INFO_IDX("MSG_PLAYBACK_COMPLETE, EOS\n");
                g_eos = true;
                break;
             case MSG_BUFFERING_UPDATE:
                VERBOSE("MSG_BUFFERING_UPDATE: param1: %d\n", param1);
                break;
            case MSG_ERROR:
                INFO_IDX("MSG_ERROR, param1: %d\n", param1);
                break;
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
            case MSG_UPDATE_TEXTURE_IMAGE:
                {
                    MMAutoLock locker(g_draw_anb_lock);
                    INFO_IDX("MSG_UPDATE_TEXTURE_IMAGE %d\n", mFrameCount);
                    mFrameCount++;
                }
                break;
#endif
            default:
                INFO_IDX("msg: %d, ignore\n", msg);
                break;
        }
    }

};

#define CHECK_PLAYER_OP_RET(OPERATION, RET) do {               \
    if(RET != MM_ERROR_SUCCESS && RET != MM_ERROR_ASYNC) {      \
        ERROR_IDX("%s fail, ret=%d\n", OPERATION, RET);             \
        return RET;                                             \
    }                                                           \
} while(0)

#define ENSURE_PLAYER_OP_SUCCESS_PREPARE() do { \
    g_opSync[mDraw].done = false;                         \
    g_opSync[mDraw].timeout = 0;                             \
} while(0)

#define ENSURE_PLAYER_OP_SUCCESS(OPERATION, RET, MY_TIMEOUT) do {  \
    if(RET != MM_ERROR_SUCCESS && RET != MM_ERROR_ASYNC){           \
        ERROR_IDX("%s failed, ret=%d\n", OPERATION, RET);               \
        return RET;                                                 \
    }                                                               \
    while (!g_opSync[mDraw].done) {                                           \
        usleep(10000);                                              \
        g_opSync[mDraw].timeout += 10000;                                        \
        if (g_opSync[mDraw].timeout > (MY_TIMEOUT)*1000*1000) {                  \
            ERROR_IDX("%s timeout", OPERATION);                               \
            return MM_ERROR_IVALID_OPERATION;                       \
        }                                                           \
    }                                                               \
}while (0)

extern gboolean use_localmediaplayer;

class MyPlayer
{
public:
    MyPlayer(int draw)
      : mpUrl(NULL)
      , mFd(-1)
      , mDraw(draw)
      , mIsReleased(false)
    {
        if (use_localmediaplayer)
            mpPlayer = MediaPlayer::create(MediaPlayer::PlayerType_COW);
        else
            mpPlayer = MediaPlayer::create(MediaPlayer::PlayerType_PROXY);
        mpListener= new TestListener(draw);
        mMeta = MediaMeta::create();

        memset(&mThreadId, 0, sizeof(mThreadId));
    }
    virtual ~MyPlayer(){
        DEBUG_IDX();
        {
            if (mThreadId) {
                mIsReleased = true;
                void * aaa = NULL;
                pthread_join(mThreadId, &aaa);
            }
        }

        if (mFd >= 0) {
            close(mFd);
            mFd = -1;
        }
        MediaPlayer::destroy(mpPlayer);
        delete mpListener;
        DEBUG_IDX("~MyPlayer done");
    }

    mm_status_t  setSurface(void* sur) {
        DEBUG_IDX();
        void *p = NULL;

        if (!use_localmediaplayer)
            return mpPlayer->setVideoDisplay(sur);
        else
            return mpPlayer->setVideoDisplay(sur);
    }

    mm_status_t setSurfaceTexture(void* sur){
        DEBUG_IDX();
        return mpPlayer->setVideoSurfaceTexture(sur);
    }

    mm_status_t quit()
    {
        mm_status_t ret = MM_ERROR_SUCCESS;
        DEBUG_IDX();
        // stop and deinit (clean up egl when there is msg of StopResult
        ENSURE_PLAYER_OP_SUCCESS_PREPARE();
        ret = mpPlayer->stop();
        ENSURE_PLAYER_OP_SUCCESS("stop", ret, 10);

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
        if (g_use_media_texture) {
            ASSERT(mst[mDraw]);
            mst[mDraw]->returnAcquiredBuffers();
        }
#endif
        DEBUG_IDX();
        // usleep(100000);
        mpPlayer->reset();
        DEBUG_IDX();
        // mpPlayer->release();
        return ret;
    }

    mm_status_t pause()
    {
        DEBUG_IDX();
        return mpPlayer->pause();
    }

    mm_status_t resume(){
        DEBUG_IDX();
        return mpPlayer->start();
    }

    mm_status_t  seek(TTime tar){
        DEBUG_IDX();
        return mpPlayer->seek(tar);
    }

    mm_status_t  setLoop(){
        static bool loop = true;

        int ret = mpPlayer->setLoop(loop);
        loop = !loop;
        return ret;
    }

    void getPositionTest() {
        mm_status_t status = MM_ERROR_UNKNOWN;
        TIME_T msec = -2;
        int64_t tmp = -2;

        if (!mThreadId) {
            if ( pthread_create(&mThreadId, NULL, getCurrentPositionThread, this) ) {
                ERROR("failed to start thread\n");
                return NULL;
            }
        }

        status = mpPlayer->getCurrentPosition(GET_REFERENCE(msec));
        tmp = msec;
        DEBUG_IDX("getCurrentPosition msec=%" PRId64 "\n", tmp);

        status = mpPlayer->getDuration(GET_REFERENCE(msec));
        tmp = msec;
        DEBUG_IDX("getDuration msec=%" PRId64 "\n", tmp);

        bool playing = mpPlayer->isPlaying();
        DEBUG_IDX("status: %d, playing %d\n", status, playing);
    }

    void selectTrackTest() {
        mm_status_t status = MM_ERROR_SUCCESS;
        MMParam* request = new MMParam();
        MMParam* param = new MMParam();

        request->writeInt32(MediaPlayer::INVOKE_ID_GET_TRACK_INFO);
        status = mpPlayer->invoke(request, param);
        ASSERT(status == MM_ERROR_SUCCESS);
        DEBUG("use mediaplayer test");

        // stream count
        int32_t streamCount = param->readInt32();
        DEBUG("streamCount: %d", streamCount);

        int32_t videoTrackCount = 0;
        int32_t i=0, j=0;
        for (i=0; i<streamCount; i++) {
            // media type & track count
            int32_t mediaType = TEST_MediaTypeUnknown;
            int32_t trackCount = 0;
            mediaType = param->readInt32();
            trackCount = param->readInt32();
            DEBUG("mediaType: %d, trackCount: %d", mediaType, trackCount);

            if (mediaType == TEST_MediaTypeVideo)
                videoTrackCount = trackCount;

            for (j=0; j<trackCount; j++) {
                int32_t index = param->readInt32();
                int32_t codecId = param->readInt32();
                const char* codecName = param->readCString();
                const char* mime = param->readCString();
                const char* title = param->readCString();
                const char* lang = param->readCString();
                int32_t width = 0, height = 0;
                if (mediaType == TEST_MediaTypeVideo) {
                    width = param->readInt32();
                    height = param->readInt32();
                }

                DEBUG("index: %d, size: %dx%d, codecId: %d, codecName: %s, mime: %s, title: %s, lang: %s",
                    index, width, height, codecId, PRINTABLE_STR(codecName), PRINTABLE_STR(mime), PRINTABLE_STR(title), PRINTABLE_STR(lang));
            }
        }

        while (videoTrackCount) {
            static int32_t selectTrack = 0;
            selectTrack++;
            selectTrack %= videoTrackCount;
            MMParam* request2 = new MMParam();
            request2->writeInt32(MediaPlayer::INVOKE_ID_SELECT_TRACK);
            request2->writeInt32(TEST_MediaTypeVideo);
            request2->writeInt32(selectTrack);
            status = mpPlayer->invoke(request2, param);
            DEBUG("status: %d", status);
            ASSERT(status == MM_ERROR_SUCCESS);
            delete request2;
            break; // always break;
        }

        delete request;
        delete param;
    }

    void test(){
        DEBUG_IDX();
        // getPositionTest();
        selectTrackTest();
    }
    void setPlayRate(bool increased){
        if (!increased) {
            g_cur_play_rate_index--;
            if (g_cur_play_rate_index < 0)
                g_cur_play_rate_index = 0;
        }

        if (increased) {
            g_cur_play_rate_index++;
            if (g_cur_play_rate_index >= g_play_rate_arr_size)
                g_cur_play_rate_index = g_play_rate_arr_size - 1;
        }


        g_cur_play_rate = g_play_rate_arr[g_cur_play_rate_index];


        DEBUG_IDX();
        mMeta->setFloat(MEDIA_ATTR_PALY_RATE, g_cur_play_rate);
        mpPlayer->setParameter(mMeta);

        DEBUG_IDX("setPlayRate %.2f\n", g_cur_play_rate);
    }



    mm_status_t play(const char* url){
        DEBUG_IDX();
        mpUrl=url;
        int  ret ;
        std::map<std::string,std::string> header;

        g_eos = false;
        if (g_setDataSource_fd) {
            mFd = open(url, O_LARGEFILE | O_RDONLY);
            if (mFd < 0) {
                DEBUG_IDX("open file (%s) fail", url);
                return MM_ERROR_IO;
            }
            int64_t fileLength = lseek64(mFd, 0, SEEK_END);
            DEBUG_IDX("file length %" PRId64 "\n", fileLength);
            if (fileLength == -1) {
                ERROR_IDX("seek file header failed");
                return MM_ERROR_IO;
            }
            //NOTE: seek to begin of the file
            lseek64(mFd, 0, SEEK_SET);
            ret = mpPlayer->setDataSource(mFd, 0, fileLength);
        } else {
            ret = mpPlayer->setDataSource(url,&header);
        }
        CHECK_PLAYER_OP_RET("setDataSource", ret);

        ret = mpPlayer->setListener(mpListener);
        CHECK_PLAYER_OP_RET("setListener", ret);

        if (g_loop_player && !g_loop_player_recreate_player) {
            mpPlayer->setLoop(true);
        }

        DEBUG_IDX("use prepareAsync\n");
        ENSURE_PLAYER_OP_SUCCESS_PREPARE();
        ret = mpPlayer->prepareAsync();
        ENSURE_PLAYER_OP_SUCCESS("prepare", ret, 60);

        DEBUG_IDX("set playRate %.2f\n", g_cur_play_rate);
        mMeta->setFloat(MEDIA_ATTR_PALY_RATE, g_cur_play_rate);

        DEBUG_IDX();
        ENSURE_PLAYER_OP_SUCCESS_PREPARE();
        ret = mpPlayer->start();
        ENSURE_PLAYER_OP_SUCCESS("start", ret, 60);

        DEBUG_IDX();
        return MM_ERROR_SUCCESS;
    }


    static void* getCurrentPositionThread(void *context) {
        DEBUG("currentTimeTestThread enter\n");
        MyPlayer *player = static_cast<MyPlayer *>(context);
        if (!player) {
            return NULL;
        }

        int ret = 0;
        TIME_T msec = -1;
        while (1) {
            if (player->mIsReleased) {
                return NULL;
            }
            ret = player->getPlayer()->getCurrentPosition(GET_REFERENCE(msec));
            if (ret != MM_ERROR_SUCCESS) {
                ERROR("getCurrentPosition failed\n");
                return NULL;
            }

            DEBUG("current position %0.3f\n", msec/1000.0f);
            usleep(500*1000);//sleep 200ms for next getCurrentPosition
        }

        return NULL;
    }

    MediaPlayer* getPlayer(){
        return mpPlayer;
    }

private:
    const char* mpUrl;
    MediaPlayer* mpPlayer;
    TestListener* mpListener;
    MediaMetaSP mMeta;
    int mFd;
    int mDraw;
    pthread_t mThreadId;
    bool mIsReleased;
};

std::vector<MyPlayer* > gPlayer;
std::vector<VrVideoView* > gVrView;
std::vector<VrViewController* > gVrViewCtrl;
int curPlayer = 0;
#define MAX_SOURCE_NAME_LEN 256
#define MAX_SOURCE_NUM 16

char g_source[MAX_SOURCE_NAME_LEN];
int get_source(const char* par)
{
    if(strlen(par) >= MAX_SOURCE_NAME_LEN){
        DEBUG("get_source err: too long source name\n");
        return -1;
    }

    int rval = 0;
    const char* ptr = par;
    int filename_len = 0;

    for(; *ptr != '\0'; ptr++){
        if(*ptr == ' ')
            break;
    }
    filename_len = ptr - par;

    DEBUG("filename_len: %d\n", filename_len);

    strncpy(g_source, par, filename_len);
    DEBUG("get_source %s, filename_len:%d, parsed:%s.\n", par, filename_len, g_source);
    return rval;
}

void show_usage()
{
    printf("\n");
    printf("-a <name> : Name of file to play.\n");
    printf("-t        : Automation test.\n");
    printf("-o        : Auto orientation tracking.\n");
    printf("-l <N>    : loop option.\n"
           "            -l 0  : seek 0 to loop play.\n"
           "            -l 1  : recreate player to loop play.\n"
           "            -l 2  : recreate player and surface to loop play.\n");
    printf("-w <N>    : surface option. \n"
           "            -w 0  : normal surface.\n"
           "            -w 2  : media_texture, not draw.\n"
           "            -w 3  : media_texture and surface, not draw.\n"
           "            -w 4  : media_texture, egle to draw.\n"
           "            -w 5  : media_texture and surface, egle to draw.\n");
    printf("-p <N>    : Set play time.\n");
    printf("-g <N>    : Set wait time to start the next play.\n");
    printf("\n");
    printf("runtime command:\n"
           "            q     : quit \n"
           "            p     : pause \n"
           "            s <N> : seek to N \n"
           "            l     : change loop type \n"
           "            t     : some test \n"
           "            i     : increase play rate \n"
           "            d     : decrease play rate \n"
           "            w     : switch textureview/surfaceview \n"
           "            m     : only use in -w 1,switch video size \n"
           "            v     : toggle to activate or deactivate vr view, direction ctrl likes vim\n");
    printf("\n");
    printf("e.g.\n");
    printf("    vrlayer-test -a /data/H264.mp4 \n");
    printf("    vrplayer-test -a /data/H264.mp4 -l 2 -w 5 -p 10 -g 2\n");
}

int get_size(const char* par)
{
    char sizeOption[MAX_SOURCE_NAME_LEN];

    if(strlen(par) >= MAX_SOURCE_NAME_LEN){
        ERROR("get_source err: too long source name\n");
        return -1;
    }

    int rval = 0;
    const char* ptr = par;
    int param_len = 0;

    for(; *ptr != '\0'; ptr++){
        if(*ptr == ' ')
            break;
    }
    param_len = ptr - par;

    DEBUG("get_size: param len is %d\n", param_len);

    strncpy(sizeOption, par, param_len);
    sizeOption[param_len] = '\0';
    DEBUG("get_size: sizeOption %s\n", sizeOption);

    sscanf(sizeOption, "%dx%d", &g_surface_width, &g_surface_height);

    DEBUG("parse window size %dx%d\n", g_surface_width, g_surface_height);
    return rval;
}

char* trim(char *str)
{
    char *ptr = str;
    int i;

    // skip leading spaces
    for (; *ptr != '\0'; ptr++)
        if (*ptr != ' ')
            break;

    //skip (-a    xxx) to xxx
    for(; *ptr != '\0'; ptr++){
        if(*ptr != ' ')
            continue;
        for(; *ptr != '\0'; ptr++){
            if(*ptr == ' ')
                continue;
            break;
        }
        break;
    }

    // remove trailing blanks, tabs, newlines
    for (i = strlen(str) - 1; i >= 0; i--)
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
            break;

    str[i + 1] = '\0';
    return ptr;
}


uint32_t g_testCount = 10;
char getRandomOp()
{
    const int randomOpMin = 0;
    const int randomOpMax = 4;
    char randomOps[]={'p','r','s','l','t'};
    uint32_t i = rand()%(randomOpMax-randomOpMin+1)+randomOpMin;
    sleep(2);
    return randomOps[i];
}

TIME_T getRandomSeekTo()
{
    MediaPlayer* mp = gPlayer[curPlayer]->getPlayer();
    TIME_T randomSeekMin = 0;
    TIME_T randomSeekMax = 0;
    mp->getDuration(GET_REFERENCE(randomSeekMax));
    int64_t tmp = randomSeekMax;
    DEBUG("max auto seek to =%" PRId64,tmp);
    TIME_T i = rand()%(randomSeekMax-randomSeekMin+1)+randomSeekMin;
    return i;
}

void RunMainLoop()
{
    char ch;
    char buffer[128] = {0};
    int flag_stdin = 0;
    int par;
    int cur = 0;
    TIME_T target;
    //
    char buffer_back[128] = {0};
    ssize_t back_count = 0;

    //playback zoom    int video_width=0;
    // int video_height=0;
    ssize_t count = 0;

    flag_stdin = fcntl(STDIN_FILENO, F_GETFL);
    if(fcntl(STDIN_FILENO, F_SETFL, flag_stdin | O_NONBLOCK)== -1)
        ERROR("stdin_fileno set error");

    uint32_t  testcount =0;
    uint32_t  playtime = 0;
    if(g_auto_test){
        uint64_t seed = (unsigned)time(0);
        INFO("auto test seed =%" PRId64, seed);
        srand(seed);
    }
    while (1)
    {
        if(g_quit)
            break;
        if(!g_auto_test){
            memset(buffer, 0, sizeof(buffer));
            if ((count = read(STDIN_FILENO, buffer, sizeof(buffer)-1) < 0)) {
                if ((g_eos && !g_loop_player) || (g_play_time && g_play_time*1000*1000 <= playtime)) {
                    buffer[0] = 'q';
                } else {
                    usleep(100000);
                    playtime += 100000;
                    continue;
                }
            } else if (buffer[0] == 'q') {
                g_user_quit = true;
            }

            buffer[sizeof(buffer) - 1] = '\0';
            count = strlen(buffer);
            DEBUG("read return %zu bytes, %s\n", count, buffer);
            ch = buffer[0];
            if(ch == '-'){
                if (count < 2)
                    continue;
                 else
                    ch = buffer[1];
            }
           if(ch == '\n' && buffer_back[0] != 0){
                DEBUG("Repeat Cmd:%s", buffer_back);//buffer_back has a line change
                memcpy(buffer, buffer_back, sizeof(buffer));
                ch = buffer[0];
                if(ch == '-'){
                    if (back_count < 2)
                        continue;
                    else
                        ch = buffer[1];
                }
           }else{
                memset(buffer_back, 0, sizeof(buffer_back));
                //DEBUG("deb%sg\n", buffer_back);
                memcpy(buffer_back, buffer, sizeof(buffer));
                back_count = count;
           }
        }
        else{ //auto test get rand op
           if(testcount <= g_testCount && !g_quit){
                ch = getRandomOp();
                DEBUG("auto ch = %c\n",ch);
                testcount ++;
            }
            else{ //auto test exit
                 ch = 'q';
                 DEBUG("auto quit\n ");
            }
        }
        //g_last_cmd = ch;
        switch (ch) {
        case 'q':    // exit
        case 'Q':
            INFO("receive 'q', quit ...");
            g_quit = true;
            for (int n = 0; n < g_player_c; n++)
            {
                gPlayer[n]->quit();
            }
            break;

        case 'p':
            gPlayer[curPlayer]->pause();
            break;

        case 'r':
            gPlayer[curPlayer]->resume();
            break;

        case 's':
            if(!g_auto_test){
                if(sscanf(trim(buffer), "%d", &par) != 1){
                    ERROR("Please set the seek target in 90k unit\n");
                    break;
                }
                target = par;
            }
            else {
                target = getRandomSeekTo();
                int64_t temp = target;
                DEBUG("auto seek to %" PRId64 "\n",temp);
            }
            gPlayer[curPlayer]->seek(target);
            break;

        case 'c':
                if(sscanf(trim(buffer), "%d", &cur) != 1){
                    ERROR("Please set curplayer in 1 - %d\n",g_player_c);
                    break;
                }
                if (cur < 1 || cur > g_player_c)
                {
                    ERROR("Please set curplayer in 1 - %d\n",g_player_c);
                    break;
                }
                DEBUG("set %d as current player", cur);
                curPlayer = cur - 1;
                break;

        case 'l':
            if (g_vr_active) {
                float pitch, roll, yaw;
                gVrViewCtrl[curPlayer]->getHeadPoseInStartSpace(pitch, roll, yaw);
                yaw -= 4*(3.1415926*2/360);
                gVrViewCtrl[curPlayer]->setHeadPoseInStartSpace(pitch, roll, yaw);
                break;
            }

            gPlayer[curPlayer]->setLoop();
            break;

        case 't':
            gPlayer[curPlayer]->test();
            break;

        case 'i':
            gPlayer[curPlayer]->setPlayRate(true);
            break;

        case 'd':
            gPlayer[curPlayer]->setPlayRate(false);
            break;

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
        case 'w':
            g_show_surface = !g_show_surface;
            mst[curPlayer]->setShowFlag(g_show_surface);
            break;
#endif
        case '-':
            //some misc cmdline
            break;
        case 'v':
            if (!gVrViewCtrl[curPlayer]) {
                printf("cannot control vr view\n");
                break;
            }

            g_vr_active = !g_vr_active;
            printf("vr view active %d\n", g_vr_active);
            if (g_vr_active)
                printf("use 'h' 'j' 'k' 'l' keys to move video view\n");

            break;

        case 'h':
            if (g_vr_active) {
                float pitch, roll, yaw;
                gVrViewCtrl[curPlayer]->getHeadPoseInStartSpace(pitch, roll, yaw);
                yaw += 4*(3.1415926*2/360);
                gVrViewCtrl[curPlayer]->setHeadPoseInStartSpace(pitch, roll, yaw);
            }
            break;
        case 'j':
            if (g_vr_active) {
                float pitch, roll, yaw;
                gVrViewCtrl[curPlayer]->getHeadPoseInStartSpace(pitch, roll, yaw);
                pitch -= 4*(3.1415926*2/360);
                gVrViewCtrl[curPlayer]->setHeadPoseInStartSpace(pitch, roll, yaw);
            }
            break;
        case 'k':
            if (g_vr_active) {
                float pitch, roll, yaw;
                gVrViewCtrl[curPlayer]->getHeadPoseInStartSpace(pitch, roll, yaw);
                pitch += 4*(3.1415926*2/360);
                gVrViewCtrl[curPlayer]->setHeadPoseInStartSpace(pitch, roll, yaw);
            }
            break;


        default:
            ERROR("Unkonw cmd line:%c\n", ch);
            break;
        }
    }

    if(fcntl(STDIN_FILENO,F_SETFL,flag_stdin) == -1)
        ERROR("stdin_fileno set error");
    DEBUG("\nExit RunMainLoop on yunostest.\n");
}

const static char *file_name = NULL;
static char *sizestr = NULL;
static gboolean autotest = FALSE;
static gboolean autoorientation = FALSE;
static gboolean preplay = FALSE;
static int loopplay = -1;
static int loopcount = -1;
static int surfacetype = 0;
static int playtime = 0;
static int waittime = 2;
static int player_c = 1;
static gboolean set_dataSource_with_fd = FALSE;
static int rotationWinDegree = 0;
gboolean use_localmediaplayer = false;

static GOptionEntry entries[] = {
    {"add", 'a', 0, G_OPTION_ARG_STRING, &file_name, " set the file name to play", NULL},
    {"winsize", 's', 0, G_OPTION_ARG_STRING, &sizestr, "set the video size, e.g. \"1280x720\"", NULL},
    {"autotest", 't', 0, G_OPTION_ARG_NONE, &autotest, "use auto test ", NULL},
    {"autoorientation", 'o', 0, G_OPTION_ARG_NONE, &autoorientation, "use auto orientation ", NULL},
    {"preplay", 'd', 0, G_OPTION_ARG_NONE, &preplay, "play video before vr", NULL},
    {"loopPlayer", 'l', 0, G_OPTION_ARG_INT, &loopplay, "set loop play", NULL},
    {"loopCount", 'c', 0, G_OPTION_ARG_INT, &loopcount, "set loop play count", NULL},
    {"mediaTexture", 'w', 0, G_OPTION_ARG_INT, &surfacetype, "set surface type", NULL},
    {"playtime", 'p', 0, G_OPTION_ARG_INT, &playtime, "each play time when loop play", NULL},
    {"waittime", 'g', 0, G_OPTION_ARG_INT, &waittime, "the interval of two play when loop play", NULL},
    {"setDataSourceWithFd", 'f', 0, G_OPTION_ARG_NONE, &set_dataSource_with_fd, "set datasource with fd", NULL},
    {"forceWinRotation", 'r', 0, G_OPTION_ARG_INT, &rotationWinDegree, "force window rotation", NULL},
    {"mediaplayerservice", 'x', 0, G_OPTION_ARG_NONE, &use_localmediaplayer, "use media player service", NULL},
    {NULL}
};

class CowplayerTest : public testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

TEST_F(CowplayerTest, cowplaytest) {
    int ret;

    gPlayer.resize(g_player_c);
    gVrView.resize(g_player_c);
    gVrViewCtrl.resize(g_player_c);
    ws.resize(g_player_c);
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    sc.resize(g_player_c);
#endif
    g_opSync.resize(g_player_c);

    for (int n = 0; n < g_player_c; n++) {
        gPlayer[n] = NULL;
        gVrView[n] = NULL;
        gVrViewCtrl[n] = NULL;
        ws[n] = NULL;
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
        sc[n] = NULL;
#endif
    }

#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    mst.resize(g_player_c);
    for (int n = 0; n < g_player_c; n++) {
        mst[n] = NULL;
    }

    if (setenv("HYBRIS_EGLPLATFORM", "wayland", 0)) {
        printf("setenv fail, errno %d", errno);
    } else
        printf("setenv HYBRIS_EGLPLATFORM=wayland");
#endif

BEGIN_OF_LOOP_FROM_CREATE_SURFACE:
    for (int n = 0; n < g_player_c; n++)
    {
        /* set sub surface size */
        g_surface_width = 2560;
        g_surface_height = 1440;

#ifndef PRE_PLAY_VIDEO
        ws[n] = createSimpleSurface(720, 1280, NST_PagewindowSub, 90);
#else
        ws[n] = createSimpleSurface(720, 1280, NST_PagewindowSub | NST_PagewindowRemote, 90);
#endif
        //ws[n] = createSimpleSurface(1280, 720);
        if (ws[n]) {
            //setSubWindowGeometry(ws[n], 10, 10, g_surface_height, g_surface_width);

            //WINDOW_API(set_scaling_mode)(ws[n], SCALING_MODE_PREF(SCALE_TO_WINDOW));

            //WINDOW_API(set_buffers_transform)(ws[n], HAL_TRANSFORM_ROT_90);

            //static_cast<yunos::libgui::Surface *>(ws[n])->setBufferTransformMode(transform_full_fill);

            //((Surface*)ws[n])->setSurfaceResize(720, 1280);
        }
    }

BEGIN_OF_LOOP_FROM_CREATE_PLAYER:

    // play video in surfaceview for a while
#ifdef PRE_PLAY_VIDEO
    if (preplay) {
        int n = 0;
        gPlayer[n] = new MyPlayer(n);

        DEBUG();
        ret = gPlayer[n]->setSurface(getWindowName(ws[n]).c_str());
        EXPECT_TRUE(ret == MM_ERROR_SUCCESS);
        DEBUG();
        ret = gPlayer[n]->play(g_source);
        EXPECT_TRUE(ret == MM_ERROR_SUCCESS);

        usleep(1000*1000*5);
        ret = gPlayer[n]->quit();
        delete gPlayer[n];
        gPlayer.clear();
        usleep(1000*1000*2);
    }
#endif

    for (int n = 0; n < g_player_c; n++)
    {
        gPlayer[n] = new MyPlayer(n);
        gVrView[n] = VrVideoView::createVrView();
        gVrView[n]->setVideoRotation(90);

        //if (!g_auto_orientation) {
            gVrView[n]->getVrViewController(&gVrViewCtrl[n]);
            if (gVrViewCtrl[n])
                gVrViewCtrl[n]->start();
        //}

        DEBUG();
        ret = gPlayer[n]->setSurfaceTexture(gVrView[n]->createSurface());
        EXPECT_TRUE(ret == MM_ERROR_SUCCESS);
        DEBUG();
        bool r;
#ifndef PRE_PLAY_VIDEO
        r = gVrView[n]->setDisplayTarget((void*)ws[n], 0, 0, 1280, 720);
#else
        r = gVrView[n]->setDisplayTarget(getWindowName(ws[n]).c_str(), 0, 0, 1280, 720);
#endif
        EXPECT_TRUE(r);
        r = gVrView[n]->init();
        EXPECT_TRUE(r);
        ret = gPlayer[n]->play(g_source);
        EXPECT_TRUE(ret == MM_ERROR_SUCCESS);
    }
    DEBUG();

    RunMainLoop();
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
    usleep(10000);
#endif
    DEBUG("delete gPlayer...\n");
    for (int n = 0; n < g_player_c; n++)
    {
        delete gPlayer[n];
        delete gVrView[n];
    }
    gPlayer.clear();
    gVrView.clear();
    gVrViewCtrl.clear();
    DEBUG("delete gPlayer done, loopedCount %d\n", loopedCount);
    printf("delete gPlayer done, loopedCount %d\n", loopedCount);

    if (!g_loop_player || g_loop_player_recreate_surface || loopedCount == g_loop_count - 1 || g_user_quit) {
#ifdef __MM_YUNOS_CNTRHAL_BUILD__
        for (int n = 0; n < g_player_c; n++)
        {
            if (ws[n]) {
                DEBUG("delete ws: %p\n", ws[n]);
                destroySimpleSurface(ws[n]);
                ws[n] = NULL;
                DEBUG("delete ws done\n");
            }
        }
        mst.clear();
        ws.clear();
#else
        if (ws[0]) {
            destroySimpleSurface(ws[0]);
            DEBUG("delete ws done\n");
        }
#endif
    }

    DEBUG();
    if (g_loop_player && loopedCount != g_loop_count - 1 && !g_user_quit) {
        usleep(g_wait_time*1000*1000);
        g_quit = false;

        #ifdef __MM_YUNOS_CNTRHAL_BUILD__
        for (int n = 0; n < g_player_c; n++)
        {
            if (!g_loop_player_recreate_surface) {
                if (ws[n]) {
                    DEBUG("call WindowSurface destroyBuffers()\n");
                    ws[n]->destroyBuffers();
                }
                // MediaSurfaceTexture doesn't support destroyBuffers(), delete and create it for next playback
                if (mst[n]) {
                    delete mst[n];
                    mst[n] = NULL;
                }
            }
        }
    #endif

        loopedCount++;
        DEBUG("loopedCount: %d, g_loop_count: %d", loopedCount, g_loop_count);
        if (loopedCount < g_loop_count) {
             if (g_loop_player_recreate_surface)
                goto BEGIN_OF_LOOP_FROM_CREATE_SURFACE;
             else
                goto BEGIN_OF_LOOP_FROM_CREATE_PLAYER;
        }
    }
    DEBUG();
}


extern "C" int main(int argc, char* const argv[]) {
    DEBUG("MediaPlayer video test for YunOS!\n");
    if (argc < 2) {
        ERROR("Must specify play file \n");
        show_usage();
        return 0;
    }

    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new(MM_LOG_TAG);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_help_enabled(context, TRUE);

    if (!g_option_context_parse(context, &argc, (gchar***)&argv, &error)) {
            ERROR("option parsing failed: %s\n", error->message);
            show_usage();
            return -1;
    }
    g_option_context_free(context);

    if(file_name){
        DEBUG("set file name  %s\n", file_name);
        if (get_source(file_name) < 0){
                ERROR("invalid file name\n");
                return -1;
        }
    } else {
        ERROR("Must specify play file \n");
        show_usage();
        return 0;
    }

    if(sizestr){
        DEBUG("set size = %s\n",sizestr);
        if (get_size(sizestr) < 0){
                ERROR("invalid size \n");
                return -1;
        }
    }

    if(autotest){
        DEBUG("set auto test\n");
        g_auto_test = true;
    }

    if(autoorientation){
        DEBUG("set auto test\n");
        g_auto_orientation = true;
        //g_vr_active = false;
    }

    if(loopplay != -1) {
/***************************************************************
 *-l 0 : seek 0 to loop play
 *-l 1 : recreate player to loop play
 *-l 2 : recreate player and surface to loop play
 */
        DEBUG("set loop play loopplay = %d\n",loopplay);
        switch (loopplay) {
        case 0:
            g_loop_player = true;
            g_loop_player_recreate_player = false;
            break;
        case 1:
            g_loop_player = true;
            g_loop_player_recreate_player = true;
            break;
        case 2:
            g_loop_player = true;
            g_loop_player_recreate_player = true;
            g_loop_player_recreate_surface = true;
            break;
        default:
            ERROR("\n");
            break;
        }
    }

    if(loopcount >0){
        DEBUG("set loop play count =%d \n",loopcount);
        g_loop_count = loopcount;
    }

/***************************************************************
 *-w 0 : normal surface
 *-w 1 : sub surface
 *-w 2 : media_texture
 *-w 3 : media_texture and egl to draw
 *-w 4 : media_texture , window_surface and egle to draw
 */

    DEBUG("set surface type surfacetype = %d\n",surfacetype);
    switch (surfacetype) {
    case 0:
        break;
    case 2:
        g_use_media_texture = true;
        break;
    case 3:
        g_use_media_texture = true;
        g_use_window_surface = true;
        break;
    case 4:
        g_use_media_texture = true;
        break;
    case 5:
        g_use_media_texture = true;
        g_use_window_surface = true;
        break;
    default:
        ERROR("error surfacetype:%d\n", surfacetype);
        break;
    }

    g_play_time = playtime;
    g_wait_time = waittime;

    if(set_dataSource_with_fd){
        DEBUG("set data source with fd\n");
        g_setDataSource_fd = true;
    }

    g_player_c = player_c;
    int ret;
    try {
        ::testing::InitGoogleTest(&argc, (char **)argv);
        ret = RUN_ALL_TESTS();
     } catch (...) {
        ERROR("InitGoogleTest failed!");
        ret = -1;
    }
    if (file_name)
        g_free((char *)file_name);
    if (sizestr)
        g_free(sizestr);
    DEBUG("cowplayyer-test done");

    return ret;
}
