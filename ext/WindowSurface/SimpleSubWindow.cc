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
#include "SimpleSubWindow.h"
#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include <log/Log.h>
#include <looper/Looper.h>
#include <memory/MemoryUtils.h>
#include <Events.h>
#include <Window.h>
#include <WindowManager.h>
#include <WinSurface.h>
#include <WinSubSurface.h>
#include <thread/Thread.h>
// #include <WindowSurface.h>

#include <multimedia/mm_debug.h>
#include <multimedia/mm_cpp_utils.h>

#define LOG_TAG "MM_SimpleSubWindow"
#define MM_LOG_TAG "MM_SimpleSubWindow"

using namespace yunos;
using namespace yunos::events;
// typedef yunos::libgui::BaseNativeSurface WindowSurface;
static Looper *g_mainLooper = NULL;
static bool g_pageWindowMainThreadRunning = false;
static bool g_remote_surface = false;
static bool g_remote_sub_surface = false;
static int g_win_rotation = 0;
static WindowSurface *g_subSurface = NULL;
static WindowSurface *g_Surface = NULL;
#if defined(MM_UT_SURFACE_WIDTH) && defined(MM_UT_SURFACE_HEIGHT)
static const uint32_t g_mainSurface_width = MM_UT_SURFACE_WIDTH;
static const uint32_t g_mainSurface_height = MM_UT_SURFACE_HEIGHT;
#else
static const uint32_t g_mainSurface_width = 640;
static const uint32_t g_mainSurface_height = 480;
#endif
static int32_t g_subSurface_x = 0;
static int32_t g_subSurface_y = 0;
static uint32_t g_subSurface_width = g_mainSurface_width/2;
static uint32_t g_subSurface_height = g_mainSurface_height/2;

extern uint32_t g_surface_width;
extern uint32_t g_surface_height;
static YUNOS_MM::Lock g_sswLock;

#if 0
#define DEBUG(format, ...)  fprintf(stderr, "[D] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define INFO(format, ...)  fprintf(stderr, "[I] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define WARNING(format, ...)  fprintf(stderr, "[W] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define ERROR(format, ...)  fprintf(stderr, "[E] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#endif

enum PageWindowAction {
    SUB_WINDOW_CREATE,
    SUB_WINDOW_PLACE_BELOW,
    SUB_WINDOW_PLACE_ABOVE,
    SUB_WINDOW_DELETE,
    SUB_WINDOW_GEOMETRY,
    SUB_WINDOW_FIT,
    SUB_WINDOW_RELEASE_BUFFER,
    SUB_WINDOW_RESTORE_BUFFER,
    SUB_WINDOW_RENDER,
    SUB_WINDOW_QUIT,
    SURFACE_WINDOW_CREATE,
    SURFACE_WINDOW_SHOW,
    SURFACE_WINDOW_HIDE,
};

namespace {

class Listener: public WindowManagerListener {
public:
    Listener() : mStarted(false) {
    }

    virtual ~Listener() {
    }

#if defined(__TABLET_BOARD_INTEL__)
    virtual void onStarted() {
#else
    virtual void onStarted(const yunos::String& pageToken) {
    DEBUG("window manager is started, pageToken %s", pageToken.c_str());
#endif
        mStarted = true;
    }

    bool isStarted() {
        return mStarted;
    }
private:
    bool mStarted;
};

class MainWindowThread : public Thread {
    typedef SharedPtr<WinSubSurface> WinSubSurfacePtr;
    typedef std::map<WindowSurface*, WinSubSurfacePtr> WinSubSurfaceMap;

public:
    MainWindowThread(WindowConfig& conf) : mConfig(conf), mRunning(true),
            mBufferConfig(BufferConfig::TYPE_NATIVE_BUFFER, 0, 0, conf.width, conf.height),
            mSubBufferConfig(BufferConfig::TYPE_NATIVE_BUFFER, 0, 0, conf.width, conf.height),
            mDrawFrameCount(0) {
    }
    virtual ~MainWindowThread() {
        INFO("Exit MainWindowThread");
    }
    int run() {
        INFO("create main window: width=%d, height=%d, type=%d", mConfig.width,
              mConfig.height, mConfig.type);

        if (g_remote_surface)
            mWindow = WindowManager::getInstance()->createWindow(mConfig, true, String("remote"));
        else
            mWindow = WindowManager::getInstance()->createWindow(mConfig);

        if (!mWindow.pointer()) {
            ERROR("Failed to create window");
            return 0;
        }
        mWindow->setVisibility(true);
    #ifdef __TV_BOARD_MSTAR__
        bool usingHwRender = YUNOS_MM::mm_check_env_str("mm.mc.use.hw.render", NULL, "1", 1);
        if (usingHwRender) {
            DEBUG("begin to setparam");
            mWindow->setParam(12, 1);
        }
    #endif

        if (g_win_rotation)
            mWindow->setRotation(g_win_rotation);

        mWindow->finishDraw();
        mWinSurface = mWindow->getSurface();
        g_pageWindowMainThreadRunning = true;
        draw(mWinSurface);
        // FIXME, there is random crash (double free) to removeWindow, seems PageWindow issue. temp ignore it for now
        DEBUG();
        WindowManager::getInstance()->removeWindow(mWindow);
        DEBUG();
        mWindow = nullptr;
        DEBUG();
        return 1;
    }
    void shutdown() {
        DEBUG();
        // SubSurface is created in loop thread, remove it in loop thread as well
        removeAllSubWindows();
        mRunning = false;
        // wait the exit of run thread
        while (mWindow != nullptr)
            usleep(50);
        DEBUG();
    }
    void createSubWindow() {
        DEBUG("mWindow: %p, mWinSurface: %p, g_remote_sub_surface: %d", mWindow.pointer(), mWinSurface.pointer(),  g_remote_sub_surface);
        if (!mWindow.pointer() || ( !mWinSurface.pointer() && !g_remote_sub_surface)) {
            ERROR("Parent window is null");
            return;
        }

        WinSubSurfacePtr winSubSurface;
        if (!g_remote_sub_surface)
            winSubSurface = WindowManager::getInstance()->createSubSurface(
                mWindow, 0, 0, g_surface_width, g_surface_height);
        else
            winSubSurface = WindowManager::getInstance()->createSubSurface(
                mWindow, 0, 0, g_surface_width, g_surface_height, 0, 0, String("remote"));

        // assumed unique size for each subsurface
        mSubBufferConfig.width = g_surface_width;
        mSubBufferConfig.height = g_surface_height;

        if (!winSubSurface.pointer()) {
            // use -1 to escape caller thread's wait
            ERROR("Failed to create sub window");
            g_subSurface = (WindowSurface*)(-1);
            return;
        }

        winSubSurface->placeAbove();
        // for remote surface, getSurface() is NULL, we use winSubSurface instead
        if (g_remote_sub_surface)
            g_subSurface = reinterpret_cast<WindowSurface*>(winSubSurface.pointer());
        else
            g_subSurface = static_cast<WindowSurface*>( winSubSurface->getSubSurface());
        mSubSurfaceMap[g_subSurface] = winSubSurface;
    }
    void createSurfaceWindow() {
        if (!mWindow.pointer() || !mWinSurface) {
            ERROR("Parent window is null, window %p, winSurface %p", mWindow.pointer(), mWinSurface.pointer());
            return;
        }

        // for remote surface, getSurface() is NULL, we use mWinSurface instead
        if (g_remote_surface)
            g_Surface = reinterpret_cast<WindowSurface*>(mWinSurface.pointer());
        else
            g_Surface = static_cast<WindowSurface*>(mWinSurface->getSurface());
    }
    void pagewindowShow(bool visibility) {
        if (!mWindow.pointer() || !mWinSurface) {
            ERROR("Parent window is null, window %p, winSurface %p", mWindow.pointer(), mWinSurface.pointer());
            return;
        }
        mWindow->setVisibility(visibility);
        if (visibility)
            mWindow->finishDraw();
     }
std::string getWindowName(WindowSurface* window) {
        std::string name;
        if (!window || (!g_remote_surface && !g_remote_sub_surface)) {
            ERROR("name is null");
            return name;
        }

        if (window == (WindowSurface*)mWinSurface.pointer()) {
            return mWinSurface->getClientToken().c_str();
        }

        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(window);
        if (winSubSurface.pointer() ) {
            return winSubSurface->getClientToken().c_str();
        }

        ERROR("name is null");
        return name;
    }

    void placeSubWindowAbove() {
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);

        if (winSubSurface.pointer() )
            winSubSurface->placeAbove();

        g_subSurface = NULL;
    }
    void placeSubWindowBelow() {
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);
        if (winSubSurface.pointer() )
            winSubSurface->placeAbove();

        g_subSurface = NULL;
    }
    void setSubWindowGeometry(/* int32_t x, int32_t y*/) {
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);

        if (winSubSurface.pointer() ) {
            DEBUG("%s: %d+%d+%dx%d", __func__, g_subSurface_x, g_subSurface_y, g_subSurface_width, g_subSurface_height);
            winSubSurface->setPosition(g_subSurface_x, g_subSurface_y);
            //g_subSurface->setSurfaceResize(g_subSurface_width, g_subSurface_height);
            #if defined(__MM_YUNOS_CNTRHAL_BUILD__) || defined(YUNOS_ENABLE_UNIFIED_SURFACE)
            winSubSurface->resize(g_subSurface_width, g_subSurface_height, g_subSurface_width, g_subSurface_height);
            #else
            winSubSurface->resize(g_subSurface_width, g_subSurface_height);
            #endif
        }

        g_subSurface = NULL;
    }
    void setSubWindowFit() {
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);
        static int mode = 0;
        if (winSubSurface.pointer() ) {
            uint32_t target_width, target_height;
            switch (mode%3) {
            case 0:
                target_width= g_surface_width;
                target_height= g_surface_height;
                break;
            case 1:
                target_width= g_surface_width * 2;
                target_height= g_surface_height * 2;
                break;
            case 2:
                if (g_surface_width * g_mainSurface_height > g_surface_height * g_mainSurface_width) {
                    target_width = g_mainSurface_width;
                    target_height = target_width * g_surface_height / g_surface_width;
                } else {
                    target_height = g_mainSurface_height;
                    target_width = target_height * g_surface_width / g_surface_height;
                }
                break;
            default:
                target_width= g_surface_width;
                target_height= g_surface_height;
                break;
            }

            if (g_mainSurface_width > target_width)
                g_subSurface_x = (g_mainSurface_width - target_width)/2;
            else
                g_subSurface_x = 0;

            if (g_mainSurface_height > target_height)
                g_subSurface_y = (g_mainSurface_height - target_height)/2;
            else
                g_subSurface_y = 0;

            winSubSurface->setPosition(g_subSurface_x, g_subSurface_y);
            //g_subSurface->setSurfaceResize(target_width, target_height);
            #if defined(__MM_YUNOS_CNTRHAL_BUILD__) || defined(YUNOS_ENABLE_UNIFIED_SURFACE)
            winSubSurface->resize(target_width, target_height, 0, 0);
            #else
            winSubSurface->resize(target_width, target_height);
            #endif

            DEBUG("main surface size: %dx%d, video size: %dx%d, scale to (%d+%d+%dx%d)",
                g_mainSurface_width, g_mainSurface_height, g_surface_width, g_surface_height,
                g_subSurface_x, g_subSurface_y, target_width, target_height);

            mode++;
        }

        g_subSurface = NULL;
    }
    void removeSubWindow() {
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);
        DEBUG();
        if (winSubSurface.pointer()) {
            mWindow->detachSubSurface(winSubSurface->getId());
            findWinSubSurfacePtr(g_subSurface, true);
            // g_subSurface = NULL;
        }

        g_subSurface = NULL;
    }

    void releaseSubWindowBuffer() {
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);
        if (winSubSurface.pointer() )
            winSubSurface->releaseBuffer();

        g_subSurface = NULL;
    }

    void restoreSubWindowBuffer(){
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);
        if (winSubSurface.pointer() )
            winSubSurface->restoreBuffer();

        g_subSurface = NULL;
    }

    void renderSubWindow(){
        WinSubSurfacePtr winSubSurface = findWinSubSurfacePtr(g_subSurface);
        if (winSubSurface.pointer() )
            drawSubSurface(winSubSurface);

        g_subSurface = NULL;
    }

private:
    void draw(SharedPtr<WinSurface> surface) {
        INFO("Draw in SampleWindowThread");

        // if we don't have sub surface, main surface is supplied to codec
        // so no need to draw main surface
        while(mRunning && mSubSurfaceMap.empty()) {
            usleep(1000*500);
        }

        while(mRunning) {
            uint8_t offset = 0;
            uint32_t* buffer = NULL;

            SharedPtr<WinBuffer> winBuffer = surface->lockBuffer(mBufferConfig);
            if (!winBuffer.pointer()) {
                ERROR("Failed to lock surface buffer");
                break;
            }
            buffer = (uint32_t*)winBuffer->rawData();

            if (buffer) {
                uint8_t color = 0;
                for (uint32_t i = 0; i < mBufferConfig.height; ++i) {
                    color = i + offset;
                    if (i % 10 == 0) {
                        color = 0;
                    }
                    for (uint32_t j = 0; j < mBufferConfig.width; ++j) {
                        if (j % 10 == 0) {
                            *buffer++ = 0xffffffff; // ABGR
                        } else {
                            *buffer++ = 0xff000000 | color;
                        }
                    }
                }
                offset++;
            }
            surface->unLockBuffer();

            if (mDrawFrameCount++ > 100)
                break;
        }

        while(mRunning) {
            usleep(1000*500);
        }

        INFO("Draw in SampleWindowThread End");
    }

    void drawSubSurface(SharedPtr<WinSubSurface> subSurface) {
        const uint32_t boxSize = 32;
        static uint32_t colorIndex = 0;
        uint32_t colors[] = { 0xf0000000, 0x00f00000, 0x0000f000, 0x000000f0
                            , 0xf0f00000, 0xf000f000, 0xf00000f0
                            , 0x00f0f000, 0x00f000f0, 0x0000f0f0};

        if (!subSurface.pointer()) {
            DEBUG("Not a valid surface");
            return;
        }

        SharedPtr<WinBuffer> winBuffer = subSurface->lockBuffer(mSubBufferConfig);
        if (!winBuffer.pointer()) {
            DEBUG("Failed to lock egl buffer");
            return;
        }

        uint32_t offset = 0;
        uint32_t* buffer = (uint32_t*)winBuffer->rawData();
        if (buffer) {
            for (uint32_t i = 0; i < mSubBufferConfig.height; ++i) {
                for (uint32_t j = 0; j < mSubBufferConfig.width; ++j) {
                    if ( (((i+offset)/boxSize)&0x1) == ((j/boxSize)&0x01))
                        *buffer++ = colors[colorIndex];
                    else
                        *buffer++ = ~colors[colorIndex];
                }
                offset++;
            }
        }

        subSurface->unLockBuffer();

        colorIndex++;
        colorIndex %= (sizeof(colors)/sizeof(colors[0]));
    }

    WinSubSurfacePtr findWinSubSurfacePtr(WindowSurface *subSurface, bool eraseIt = false)
    {
        WinSubSurfacePtr winSubSurface;
        YUNOS_MM::MMAutoLock locker(mLock);
        WinSubSurfaceMap::iterator it = mSubSurfaceMap.find(subSurface);

        if (it != mSubSurfaceMap.end()) {
            winSubSurface = it->second;
            if (eraseIt)
                mSubSurfaceMap.erase(it);
        }

        return winSubSurface;
    }

    void removeAllSubWindows() {
        WinSubSurfacePtr winSubSurface;
        YUNOS_MM::MMAutoLock locker(mLock);
        WinSubSurfaceMap::iterator it = mSubSurfaceMap.begin();

        while(it != mSubSurfaceMap.end()) {
            winSubSurface = it->second;
            if (winSubSurface.pointer())
                mWindow->detachSubSurface(winSubSurface->getId());
            it++;
        }
        mSubSurfaceMap.clear();
    }


    WindowConfig mConfig;
    bool mRunning;
    BufferConfig mBufferConfig;
    SharedPtr<Window> mWindow;
    SharedPtr<WinSurface> mWinSurface;

    WinSubSurfaceMap mSubSurfaceMap;
    BufferConfig mSubBufferConfig;
    YUNOS_MM::Lock mLock;
    int mDrawFrameCount;
};


Listener gListener;
SharedPtr<MainWindowThread> gMainWindowThread;

void startMainWindowThread(yunos::Looper* looper) {
    SharedPtr<WindowManager> manager = WindowManager::getInstance();
    manager->setToken(yunos::String("SYSTEM_SERVICE_TOKEN"));
    manager->addWindowManagerListener(&gListener);
    INFO("start window manager");
    static SharedPtr<MessageDelegate> delegate;
    delegate = new DefaultMessageDelegate(looper);
    manager->start(delegate, true);
    while (!gListener.isStarted()) {
        usleep(1000);
    }

    INFO("window manager is started");
    WindowConfig conf(0, 0, g_mainSurface_width, g_mainSurface_height, 2008, true);
    gMainWindowThread = YUNOS_NEW(MainWindowThread, conf);
    if (!gMainWindowThread.pointer()) {
        YUNOS_ABORT_OOM();
    }
    gMainWindowThread->start();
}

void execCommand(char cmd) {
    DEBUG("cmd: %d", cmd);
    if (cmd == SUB_WINDOW_CREATE) {
        gMainWindowThread->createSubWindow();
    } else if (cmd == SUB_WINDOW_PLACE_ABOVE) {
        gMainWindowThread->placeSubWindowAbove();
    } else if (cmd == SUB_WINDOW_PLACE_BELOW) {
        gMainWindowThread->placeSubWindowBelow();
    } else if (cmd == SUB_WINDOW_GEOMETRY) {
        gMainWindowThread->setSubWindowGeometry();
    } else if (cmd == SUB_WINDOW_FIT) {
        gMainWindowThread->setSubWindowFit();
    } else if (cmd == SUB_WINDOW_DELETE) {
        gMainWindowThread->removeSubWindow();
    } else if (cmd == SUB_WINDOW_RELEASE_BUFFER) {
        gMainWindowThread->releaseSubWindowBuffer();
    } else if (cmd == SUB_WINDOW_RESTORE_BUFFER) {
        gMainWindowThread->restoreSubWindowBuffer();
    } else if (cmd == SUB_WINDOW_RENDER) {
        gMainWindowThread->renderSubWindow();
    } else if (cmd == SUB_WINDOW_QUIT) {
        gMainWindowThread->shutdown();
        gMainWindowThread->join();
        SharedPtr<WindowManager> manager = WindowManager::getInstance();
        manager->stop();
        manager->removeWindowManagerListener(&gListener);
        Looper::current()->quit();
    } else if (cmd == SURFACE_WINDOW_CREATE) {
        gMainWindowThread->createSurfaceWindow();
    } else if (cmd == SURFACE_WINDOW_SHOW) {
        gMainWindowThread->pagewindowShow(true);
    } else if (cmd == SURFACE_WINDOW_HIDE) {
        gMainWindowThread->pagewindowShow(false);
    }
}
}  // namespace

void pageWindowOperation(PageWindowAction action)
{
    DEBUG();
    if (g_mainLooper)
        g_mainLooper->sendTask(Task(execCommand, action));
}

static void* mainPageWindow(void *param) {
    Looper looper;
    startMainWindowThread(&looper);
    g_mainLooper = &looper;
    // postpone it to MainWindowThread::run() until mWindow is created; since createSubWindow depends on mWindow
    // g_pageWindowMainThreadRunning = true;
    DEBUG();
    looper.run();
    DEBUG();
    g_pageWindowMainThreadRunning = false;
    INFO("exiting...");
    return NULL;
}

bool initializePageWindow(bool isRemote, bool isSubRemote, int rotation)
{
    DEBUG();
    pthread_attr_t attr;
    pthread_t thread;
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    g_pageWindowMainThreadRunning = false;

    g_remote_surface = isRemote;
    g_remote_sub_surface = isSubRemote;
    g_win_rotation = ((rotation/90) % 3);

    int rc = pthread_create(&thread, &attr, mainPageWindow, NULL);
    pthread_attr_destroy (&attr);
    if (rc){
        ERROR("ERROR; return code from pthread_create() is %d\n", rc);
        return false;
    }
    DEBUG();
    while(!g_pageWindowMainThreadRunning) {
        DEBUG();
        usleep(50000);
    }

    DEBUG();
    return true;
}

void deinitializePageWindow()
{
    DEBUG();
    YUNOS_MM::MMAutoLock locker(g_sswLock);
    pageWindowOperation(SUB_WINDOW_QUIT);

    DEBUG();
    while(g_pageWindowMainThreadRunning)
        usleep(50000);
    DEBUG();
}

bool createSubWindow(WindowSurface **sub)
{
    YUNOS_MM::MMAutoLock locker(g_sswLock);

    DEBUG();
    g_subSurface = NULL;

    if (!g_pageWindowMainThreadRunning)
        initializePageWindow();

    pageWindowOperation(SUB_WINDOW_CREATE);

    DEBUG();
    while (!g_subSurface && g_subSurface != (WindowSurface*)(-1)) {
        DEBUG("g_subSurface: %p", g_subSurface);
        usleep(50000);
    }

    DEBUG("g_subSurface: %p, gHasName %d", g_subSurface);
    *sub = g_subSurface;
    if (g_subSurface == (WindowSurface*)(-1))
        *sub = NULL;

    g_subSurface = NULL;
    return true;
}

bool createWindow(WindowSurface **surface)
{
    YUNOS_MM::MMAutoLock locker(g_sswLock);

    DEBUG();
    g_Surface = NULL;

    if (!g_pageWindowMainThreadRunning)
        initializePageWindow();

    pageWindowOperation(SURFACE_WINDOW_CREATE);

    DEBUG();
    while ((!g_Surface && g_Surface != (WindowSurface*)(-1))) {
        DEBUG("g_Surface: %p", g_Surface);
        usleep(50000);
    }

    DEBUG("g_Surface: %p", g_Surface);
    *surface = g_Surface;
    if (g_Surface == (WindowSurface*)(-1))
        *surface = NULL;

    g_Surface = NULL;
    return true;
}

std::string getWindowName(WindowSurface* window)
{
    std::string name;
    if (!gMainWindowThread.pointer())
        return name;

    return gMainWindowThread->getWindowName(window);
}
void pagewindowShow(bool visibility)
{
    if (visibility)
        pageWindowOperation(SURFACE_WINDOW_SHOW);
    else
        pageWindowOperation(SURFACE_WINDOW_HIDE);
}

#define SET_CURR_SUB_SURFACE_AND_LOCK(SUB)  \
YUNOS_MM::MMAutoLock locker(g_sswLock);       \
if (!SUB) {                                 \
    WARNING("invalid subsurface");          \
    return false;                           \
}                                           \
g_subSurface = SUB;

#define SET_CURR_SUB_SURFACE_AND_LOCK_VOID(SUB)  \
YUNOS_MM::MMAutoLock locker(g_sswLock);       \
if (!SUB) {                                 \
    WARNING("invalid subsurface");          \
    return;                                 \
}                                           \
g_subSurface = SUB;

#define WAIT_OP_UNTIL_DONE() while (g_subSurface) { \
        DEBUG();                                    \
        usleep(50000);                              \
    }

bool destroySubWindow(WindowSurface *sub)
{
    if (!sub)
        return true;

    SET_CURR_SUB_SURFACE_AND_LOCK(sub);
    pageWindowOperation(SUB_WINDOW_DELETE);

    DEBUG();
    // FIXME, seems not necessary to wait
    WAIT_OP_UNTIL_DONE();

    return true;
}
bool setSubWindowGeometry(WindowSurface *sub, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    SET_CURR_SUB_SURFACE_AND_LOCK(sub);

    g_subSurface_x = x;
    g_subSurface_y = y;
    g_subSurface_width = width;
    g_subSurface_height = height;
    pageWindowOperation(SUB_WINDOW_GEOMETRY);
    WAIT_OP_UNTIL_DONE();

    return true;
}

bool subWindowReleaseBuffer(WindowSurface *sub) {
    SET_CURR_SUB_SURFACE_AND_LOCK(sub);
    pageWindowOperation(SUB_WINDOW_RELEASE_BUFFER);
    WAIT_OP_UNTIL_DONE();

    return true;
}

bool subWindowRestoreBuffer(WindowSurface *sub) {
    SET_CURR_SUB_SURFACE_AND_LOCK(sub);
    pageWindowOperation(SUB_WINDOW_RESTORE_BUFFER);
    WAIT_OP_UNTIL_DONE();
    return true;
}

void setSubWindowFit(WindowSurface *sub)
{
    SET_CURR_SUB_SURFACE_AND_LOCK_VOID(sub);
    pageWindowOperation(SUB_WINDOW_FIT);
    WAIT_OP_UNTIL_DONE();
}

void renderSubWindow(WindowSurface *sub)
{
    SET_CURR_SUB_SURFACE_AND_LOCK_VOID(sub);
    pageWindowOperation(SUB_WINDOW_RENDER);
    WAIT_OP_UNTIL_DONE();
}


