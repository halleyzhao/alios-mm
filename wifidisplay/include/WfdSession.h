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

#ifndef __wfd_session_h
#define __wfd_session_h

#include "multimedia/mm_debug.h"
#include "multimedia/mmmsgthread.h"
#include "multimedia/media_meta.h"
#include "multimedia/mmparam.h"

#include "WfdRtspFramework.h"

#include <string>
#include <list>
#include <unistd.h>

namespace YUNOS_MM {

class IWfdSessionCB {
public:
    IWfdSessionCB() {}
    virtual ~IWfdSessionCB() {}

    virtual void onDisplayConnected(
                     uint32_t width,
                     uint32_t height,
                     uint32_t flags,
                     uint32_t session) = 0;

    virtual void onDisplayDisconnected() = 0;

    enum {
        WFD_ERROR_NONE,
        WFD_ERROR_UNKNOWN,
        WFD_ERROR_CONNECT_FAIL
    };
    virtual void onDisplayError(int32_t error) = 0;

    virtual void onDisplayUIBCInfo(bool enable, int port) {} ;
};

#define WFD_MSG_START 1
#define WFD_MSG_RTSP 2
#define WFD_MSG_MEDIA 3
#define WFD_MSG_KEEP_ALIVE 4
#define WFD_MSG_STOP 5
#define WFD_MSG_TEARDOWN_TIMEOUT 6
#define WFD_MSG_REQUEST_IDR 7
#define WFD_MSG_PLAYER_START 8

class MMParam;

class WfdSession : public MMMsgThread,
                   public WfdRtspFramework::WfdRtspListener {

public:
    WfdSession(const char* iface, IWfdSessionCB* listener, bool server);
    virtual ~WfdSession();

    mm_status_t start();
    mm_status_t stop();
    mm_status_t resume();
    mm_status_t pause();

    // WfdRtspListener
    void onRtspMessage(int msg, int param1, const MediaMetaSP metaSP);

protected:
    virtual bool handleRtspOption(MediaMetaSP request);// source sink
    virtual bool handleRtspSetup(MediaMetaSP request);// source
    virtual bool handleRtspPlay(MediaMetaSP request);// source
    virtual bool handleRtspPause(MediaMetaSP request);// source
    virtual bool handleRtspTearDown(MediaMetaSP request);// source
    virtual bool handleRtspGetParameter(MediaMetaSP request);// sink
    virtual bool handleRtspSetParameter(MediaMetaSP request);// source sink

    virtual bool sendWfdM1(); // source
    virtual bool sendWfdM2(); // sink
    virtual bool sendWfdM3(); // source
    virtual bool sendWfdM4(); // source
    virtual bool sendWfdM5(const char*); // source
    virtual bool sendWfdM6(); // sink
    virtual bool sendWfdM7(); // sink
    virtual bool sendWfdM8(); // sink
    virtual bool sendWfdM9(); // sink
    virtual bool sendWfdM10(); // sink
    virtual bool sendWfdM11(); // sink
    virtual bool sendWfdM12(); // source sink
    virtual bool sendWfdM13(); // sink
    virtual bool sendWfdM14(); // source sink
    virtual bool sendWfdM15(); // source sink
    virtual bool sendWfdM16(); // source

    virtual bool handleResponseM1(MediaMetaSP); // source
    virtual bool handleResponseM2(MediaMetaSP); // sink
    virtual bool handleResponseM3(MediaMetaSP); // source
    virtual bool handleResponseM4(MediaMetaSP); // source
    virtual bool handleResponseM5(MediaMetaSP); // source
    virtual bool handleResponseM6(MediaMetaSP); // sink
    virtual bool handleResponseM7(MediaMetaSP); // sink
    virtual bool handleResponseM8(MediaMetaSP); // sink
    virtual bool handleResponseM9(MediaMetaSP); // sink
    virtual bool handleResponseM10(MediaMetaSP); // sink
    virtual bool handleResponseM11(MediaMetaSP); // sink
    virtual bool handleResponseM12(MediaMetaSP); // source sink
    virtual bool handleResponseM13(MediaMetaSP); // sink
    virtual bool handleResponseM14(MediaMetaSP); // source sink
    virtual bool handleResponseM15(MediaMetaSP); // source sink
    virtual bool handleResponseM16(MediaMetaSP); // source

    MediaMetaSP mWfdSinkParameters;
    int64_t mLastRecvMessageTimeUs;

protected:

    MediaMetaSP rmRequestFromList(MediaMetaSP response);
    bool checkRtspResponse(MediaMetaSP response);
    void stopInternal();
    void onMediaPipelineMessage(int msg, int param1, int param2, const MMParamSP meta);

    virtual void setupMediaPipeline() = 0;
    virtual void teardownMediaPipeline() = 0;
    virtual void stopMediaPipeline() = 0;

    enum WfdRtspState {
        INIT,
        ABOUT_TO_SETUP,
        READY,
        ABOUT_TO_PLAY,
        PLAYING,
        ABOUT_TO_TEARDOWN,
        TEARDOWN,
    };

    enum State {
        UNINITIALIZED,
        INITIALIZING,
        INITIALIZED,
        PREPARING,
        PREPARED,
        STARTING,
        STARTED,
        PAUSED,
        STOPPING,
        STOPPED,
    };

    bool mIsServer;
    std::string mNetworkInterface;

    WfdRtspFramework *mRtsp;
    int32_t mRtspPlaySession;
    IWfdSessionCB *mWfdSessionCb;
    std::string mRemoteIP;
    std::string mLocalIP;
    int mRtpPort;

    typedef struct _UIBCCtrl {
        int mUibcPort;
        bool mKeyboard;
        bool mMouse;
        bool mSingleTouch;
        bool mMultiTouch;
        bool mEnable;
    } UIBCCtrl;

    UIBCCtrl mUIBCCtrl;

    // requests are send and waiting for response from peer
    std::list<MediaMetaSP> mRequestSend;

    State mState;
    WfdRtspState mRtspState;

    uint32_t mStopReplyId;

    Lock mLock;
    Condition mCondition;

    DECLARE_MSG_LOOP()
    DECLARE_MSG_HANDLER(onStart)
    DECLARE_MSG_HANDLER2(handleRtspMessage)
    DECLARE_MSG_HANDLER2(handleMediaEvent)
    DECLARE_MSG_HANDLER2(handleWfdKeepAlive) // 4.5.1
    DECLARE_MSG_HANDLER2(handleWfdRequestIDR)
    DECLARE_MSG_HANDLER2(handlePlayerStart)
    DECLARE_MSG_HANDLER(onStop)
    DECLARE_MSG_HANDLER(onRtspTearDownTimeout)

    static const char * MM_LOG_TAG;
};

} // end of namespace YUNOS_MM
#endif //__wfd_ession_h
