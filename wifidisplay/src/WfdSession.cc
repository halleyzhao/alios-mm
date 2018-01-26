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

#include "WfdSession.h"
#include "WfdParameters.h"
#include "WfdMediaMsgInfo.h"

#include "multimedia/media_attr_str.h"

namespace YUNOS_MM {

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

BEGIN_MSG_LOOP(WfdSession)
    MSG_ITEM(WFD_MSG_START, onStart)
    MSG_ITEM2(WFD_MSG_RTSP, handleRtspMessage)
    MSG_ITEM2(WFD_MSG_MEDIA, handleMediaEvent)
    MSG_ITEM2(WFD_MSG_KEEP_ALIVE, handleWfdKeepAlive)
    MSG_ITEM2(WFD_MSG_REQUEST_IDR, handleWfdRequestIDR)
    MSG_ITEM2(WFD_MSG_PLAYER_START, handlePlayerStart)
    MSG_ITEM(WFD_MSG_STOP, onStop)
    MSG_ITEM(WFD_MSG_TEARDOWN_TIMEOUT, onRtspTearDownTimeout)
END_MSG_LOOP()

WfdSession::WfdSession(const char* iface, IWfdSessionCB* listener, bool server)
    : MMMsgThread("WfdSession"),
      WfdRtspFramework::WfdRtspListener(),
      mLastRecvMessageTimeUs(0ll),
      mIsServer(server),
      mNetworkInterface(iface),
      mRtsp(NULL),
      mRtspPlaySession(0),
      mWfdSessionCb(listener),
      mRtpPort(19000),
      mState(UNINITIALIZED),
      mRtspState(INIT),
      mStopReplyId(-1),
      mCondition(mLock){

    ENTER1();

    if (!server) {
        std::string iface1 = mm_get_env_str("mm.wfd.sink.iface", NULL);
        if (!iface1.empty()) {
            mNetworkInterface = iface1;
            INFO("override wfd sink iface: %s", mNetworkInterface.c_str());
        }
    }

    mUIBCCtrl.mUibcPort = -1;
    mUIBCCtrl.mKeyboard = false;
    mUIBCCtrl.mMouse = false;
    mUIBCCtrl.mSingleTouch = false;
    mUIBCCtrl.mMultiTouch = false;
    mUIBCCtrl.mEnable = false;

    EXIT1();
}

WfdSession::~WfdSession() {
    ENTER1();

    mCondition.broadcast();
    // don't call stop as onStop call virtual func
    //stop();

    EXIT1();
}

mm_status_t WfdSession::start() {
    ENTER1();
    mm_status_t status;
    int ret;

    if (!mWfdSessionCb)
        EXIT_AND_RETURN1(MM_ERROR_OP_FAILED);

    run();

    mState = INITIALIZING;

    ret = postMsg(WFD_MSG_START, 0, NULL);

    if (!ret)
        status = MM_ERROR_SUCCESS;
    else
        status = MM_ERROR_OP_FAILED;

    EXIT_AND_RETURN1(status);
}

void WfdSession::onStart(param1_type param1, param2_type param2, uint32_t rspId) {
    ENTER1();

    MMAutoLock lock(mLock);

    if (mState != INITIALIZING)
        EXIT1();

    mRtsp = new WfdRtspFramework(mNetworkInterface.c_str(), mIsServer);

    mRtsp->setListener(this);

    mm_status_t status = mRtsp->wfdConnect();//the local  ip & remote ip
    if (status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC) {
        ERROR("fail to setup rtsp connection w/ status %d", status);
        mState = UNINITIALIZED;
        mWfdSessionCb->onDisplayError(IWfdSessionCB::WFD_ERROR_CONNECT_FAIL);
        EXIT1();
    }

    mState = INITIALIZED;

    EXIT1();
}

void WfdSession::onStop(param1_type param1, param2_type param2, uint32_t rspId) {
    ENTER1();

    {
        MMAutoLock lock(mLock);

        if (mState == UNINITIALIZED) {
            postReponse(rspId, 0, 0);
            EXIT1();
        }
    }

    mStopReplyId = rspId;

    if (mRtspState >= ABOUT_TO_PLAY) {

        // stop mediaplayer or mediarecorder
        stopMediaPipeline();

        if (mIsServer) {
            INFO("onStop is called in rtsp state %d, trigger teardown", mRtspState);
            sendWfdM5(RTSP_REQUEST_METHOD_TEARDOWN);
        } else {
            INFO("onStop is called in rtsp state %d, send teardown", mRtspState);
            sendWfdM8();
        }
        mRtspState = ABOUT_TO_TEARDOWN;
        postMsg(WFD_MSG_TEARDOWN_TIMEOUT, 0, NULL,
                RTSP_TEARDOWN_TRIGGER_TIMEOUT * 1000000);
    } else {
        INFO("onStop is called while session is not setup, rtsp state %d", mRtspState);
        mRtspState = TEARDOWN;
        stopInternal();
    }

    EXIT1();
}

void WfdSession::onRtspTearDownTimeout(param1_type param1, param2_type param2, uint32_t rspId) {
    ENTER1();

    if (mRtspState == ABOUT_TO_TEARDOWN) {
        WARNING("wfd wait tear down timeout, force stop");
        stopInternal();
    } else {
        WARNING("rtsp tear down timeout, but rtsp state is %d", mRtspState);
        EXIT1();
    }

    mRtspState = TEARDOWN;

    EXIT1();
}

void WfdSession::stopInternal() {
    ENTER1();

    teardownMediaPipeline();

    INFO("Media Pipeline is destroyed");

    if (mRtsp) {
        INFO("destroy rtsp framework");
        mRtsp->setListener(NULL);
        mm_status_t status = mRtsp->wfdDisConnect();
        if (status != MM_ERROR_SUCCESS) {
            ERROR("fail to teardown rtsp connection");
        }
        mRtsp = NULL;
    }

    INFO("rtsp is disconnected");

    mWfdSessionCb->onDisplayDisconnected();

    {
        MMAutoLock lock(mLock);
        mState = UNINITIALIZED;
    }

    postReponse(mStopReplyId, 0, 0);
    mStopReplyId = -1;

    EXIT1();
}

mm_status_t WfdSession::stop() {
    ENTER1();

    mm_status_t status;
    int ret;
    param1_type resp_param1;
    param2_type resp_param2;


    {
        MMAutoLock lock(mLock);

        if (mState == UNINITIALIZED) {
            INFO("not init, stop() need do nothing");
            exit();
            EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
        }

        mState = STOPPING;
    }

    ret = sendMsg(WFD_MSG_STOP, 0, NULL, &resp_param1, &resp_param2);

    if (!ret && !resp_param1)
        status = MM_ERROR_SUCCESS;
    else
        status = MM_ERROR_OP_FAILED;

    exit();

    mState = UNINITIALIZED;

    EXIT_AND_RETURN1(status);
}

mm_status_t WfdSession::pause() {
    ENTER1();

    EXIT_AND_RETURN1(MM_ERROR_UNSUPPORTED);
}

mm_status_t WfdSession::resume() {
    ENTER1();

    EXIT_AND_RETURN1(MM_ERROR_UNSUPPORTED);
}

class MMParamRefBase: public MMRefBase {
public:
    MMParamRefBase(MMParamSP param)
        : mParam(param) {}

    virtual ~MMParamRefBase() {}
    MMParamSP getParam() { return mParam; }

private:
    MMParamSP mParam;
};

class MediaMetaRefBase: public MMRefBase {
public:
    MediaMetaRefBase(MediaMetaSP meta)
        : mMeta(meta) {}

    virtual ~MediaMetaRefBase() {}
    MediaMetaSP getMeta() { return mMeta; }

private:
    MediaMetaSP mMeta;
};


void WfdSession::onMediaPipelineMessage(int msg, int param1, int param2, const MMParamSP param) {
    ENTER();

    MMRefBase *base = NULL;

    if (param) {
        param->writeInt32(param2);
        base = new MMParamRefBase(param);
    } else {
        MMParamSP paramm(new MMParam);
        paramm->writeInt32(param2);
        base = new MMParamRefBase(paramm);
    }

    MMRefBaseSP param3(base);

    postMsg(WFD_MSG_MEDIA, msg, (void*)param1, 0, param3);

    EXIT();
}

void WfdSession::handleRtspMessage(param1_type param1, param2_type param2, param3_type param3, uint32_t rspId) {
    ENTER();

    if (mRtspState == TEARDOWN) {
        INFO("rtsp is teardown, ignore wfd msg %d", param1);
        EXIT1();
    }

    INFO("handle wfdRtsp message %d", param1);

    MediaMetaRefBase *metaRef = DYNAMIC_CAST<MediaMetaRefBase*>(param3.get());
    if(!metaRef){
        ERROR("get metaRef fail");
        EXIT1();
    }
    MediaMetaSP meta = metaRef->getMeta();

    if (param1 == WfdRtspFramework::WFD_MSG_CONNECTED) {
        const char *ip;
        if (mRtspState != INIT)
            WARNING("receive WFD_MSG_CONNECTED in bad state %d", mRtspState);

        if (meta && meta->getString(WFD_RTSP_REMOTE_IP, ip) && ip) {
            mRemoteIP = ip;
        } else {
            if (mIsServer) {
                INFO("remote addr is unkonwn to wfd source");
                mWfdSessionCb->onDisplayError(1); // error unknown
            }
        }

        ip = NULL;
        if (meta && meta->getString(WFD_RTSP_LOCAL_IP, ip) && ip) {
            mLocalIP = ip;
        } else {
            INFO("local addr is un-determined");
            mWfdSessionCb->onDisplayError(1); // error unknown
        }

        if (mIsServer) {
            INFO("rtsp has been connected, about to send M1");
            sendWfdM1();
        }

    } else if (param1 == WfdRtspFramework::WFD_MSG_RTSP_RESPONSE) {
        MediaMetaSP request;
        MediaMetaSP response = meta;
        const char* rtspMsgID = NULL;

        request = rmRequestFromList(response);
        if (!request)
            EXIT1();

        if (!checkRtspResponse(response)) {
            const char *status = NULL, *reason = NULL;
            request->getString(WFD_RTSP_MSG_ID, rtspMsgID);
            request->getString(RTSP_STATUS, status);
            request->getString(RTSP_REASON, reason);
            ERROR("wfd rtsp request %s get response with status %s reason %s",
                  rtspMsgID, status, reason);

            mWfdSessionCb->onDisplayError(1); // error unknown

            EXIT();
        }

        request->getString(WFD_RTSP_MSG_ID, rtspMsgID);

        if (!rtspMsgID) {
            ERROR("oops, what to response!?");
            EXIT1();
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M1)) {
            handleResponseM1(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M2)) {
            handleResponseM2(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M3)) {
            handleResponseM3(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M4)) {
            handleResponseM4(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M5)) {
            handleResponseM5(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M6)) {
            handleResponseM6(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M7)) {
            handleResponseM7(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M8)) {
            handleResponseM8(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M9)) {
            handleResponseM9(response);
        } else if (!strcmp(rtspMsgID, WFD_RTSP_MSG_M13)){
            handleResponseM13(response);
        } else {
            INFO("receive %s response, just need do nothing", rtspMsgID);
        }

    } else if (param1 == WfdRtspFramework::WFD_MSG_RTSP_REQUEST) {
        MediaMetaSP request = meta;
        const char* method = NULL;
        bool ret = false;

        if (!request ||
            !request->getString(RTSP_REQUEST_METHOD, method) ||
            !method) {
            ERROR("request(%p) has no method field", request.get());
            EXIT();
        }

        if (!strcmp(method, RTSP_REQUEST_METHOD_OPTION))
            ret = handleRtspOption(request);
        else if (!strcmp(method, RTSP_REQUEST_METHOD_SETUP))
            ret = handleRtspSetup(request);
        else if (!strcmp(method, RTSP_REQUEST_METHOD_PLAY))
            ret = handleRtspPlay(request);
        else if (!strcmp(method, RTSP_REQUEST_METHOD_PAUSE))
            ret = handleRtspPause(request);
        else if (!strcmp(method, RTSP_REQUEST_METHOD_TEARDOWN))
            ret = handleRtspTearDown(request);
        else if (!strcmp(method, RTSP_REQUEST_METHOD_GETPARAMETER))
            ret = handleRtspGetParameter(request);
        else if (!strcmp(method, RTSP_REQUEST_METHOD_SETPARAMETER))
            ret = handleRtspSetParameter(request);

        if (!ret) {
            ERROR("handle request %s fail", method);

            mWfdSessionCb->onDisplayError(1); // error unknown
        }
    } else if (param1 == WfdRtspFramework::WFD_MSG_RTSP_DISCONNECTED) {
        ERROR("rtsp disconnect");
        mWfdSessionCb->onDisplayError(IWfdSessionCB::WFD_ERROR_CONNECT_FAIL);
    } else
        ERROR("Unknown WFD_MSG_RTSP type %d", param1);

    EXIT();
}

void WfdSession::handleMediaEvent(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId) {
    ENTER();
    // need to override

    EXIT();
}

void WfdSession::handleWfdKeepAlive(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId) {
    ENTER1();

    if (mRtspState >= ABOUT_TO_TEARDOWN) {
        INFO("rtsp is state %d, no need to keep alive", mRtspState);
        EXIT1();
    }

    if ((getTimeUs() - mLastRecvMessageTimeUs) > SESSION_TIMEOUT_SEC * 1000000) {
        ERROR("wfd rtsp session time out");
        mWfdSessionCb->onDisplayError(1); // error unknown
    }

    sendWfdM16();

    postMsg(WFD_MSG_KEEP_ALIVE, 0, NULL, CHECK_SESSION_ALIVE_PERIOD * 1000000);

    EXIT1();
}

// WfdRtspListener
void WfdSession::onRtspMessage(int msg, int param1, const MediaMetaSP meta) {
    ENTER();

    MMRefBase *base = new MediaMetaRefBase(meta);
    MMRefBaseSP param3(base);
    postMsg(WFD_MSG_RTSP, msg, (void*)param1, 0, param3);

    mLastRecvMessageTimeUs = getTimeUs();

    EXIT();
}

MediaMetaSP  WfdSession::rmRequestFromList(MediaMetaSP response) {
    ENTER();

    int cseq = -1;
    MediaMetaSP request;
    std::list<MediaMetaSP>::iterator it = mRequestSend.begin();

    if (!response || !response->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("NULL response(%p) or invalid cseq(%d)", response.get(), cseq);
        return request;
    }

    if (mRequestSend.size() != 1) {
        ERROR("find the mRequestSend size:%d\n", mRequestSend.size());
        return request;
    }

    while (it != mRequestSend.end()) {
        int responseCseq;
        if ((*it)->getInt32(RTSP_MSG_CSEQ, responseCseq) && responseCseq == cseq) {
            request = *it;
        }

        it = mRequestSend.erase(it);
    }

    if (!request)
        ERROR("no matching request for the response w/ cseq %d", cseq);

    return request;
}

bool WfdSession::checkRtspResponse(MediaMetaSP response) {
    ENTER();

    int status;

    if (response && response->getInt32(RTSP_STATUS, status) && status == 200 ) {
        EXIT_AND_RETURN( true );
    }

    EXIT_AND_RETURN(false);
}

bool WfdSession::handleRtspOption(MediaMetaSP request) {
    ENTER();
    MediaMetaSP meta;
    int cseq = -1;

    INFO("process request(OPTION) from client");

    if (mRtspState != INIT)
        WARNING("receive option request in rtsp state %d", mRtspState);

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("request(%p) has no valid cseq(%d)", request.get(), cseq);
        EXIT_AND_RETURN(false);
    }

    meta = MediaMeta::create();
    if (!meta) {
        ERROR("fail to create meta to response OPTION");
        EXIT_AND_RETURN(false);
    }

    meta->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
    meta->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_OPTION);

    if (mIsServer)
        meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M2); // response to M2
    else
        meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M1); // response to M1

    meta->setInt32(RTSP_MSG_CSEQ, cseq); // use cseq from request

    mm_status_t status = mRtsp->sendRtspMessage(meta);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("fail to create meta for response");
        EXIT_AND_RETURN(false);
    }

    if (!mIsServer) {
        sendWfdM2();
        EXIT_AND_RETURN(true);
    }

    // send wfd M3 getParameter
    sendWfdM3();


    EXIT_AND_RETURN(true);
}

bool WfdSession::handleRtspSetup(MediaMetaSP request) {
    ENTER();

    EXIT_AND_RETURN(false);
}

bool WfdSession::handleRtspPlay(MediaMetaSP request) {
    ENTER();

    EXIT_AND_RETURN(false);
}

bool WfdSession::handleRtspPause(MediaMetaSP request) {
    ENTER();

    EXIT_AND_RETURN(false);
}

bool WfdSession::handleRtspTearDown(MediaMetaSP request) {
    ENTER();

    EXIT_AND_RETURN(false);
}

bool WfdSession::handleRtspGetParameter(MediaMetaSP request) {
    ENTER();

    EXIT_AND_RETURN(false);
}

bool WfdSession::handleRtspSetParameter(MediaMetaSP request) {
    ENTER();

    EXIT_AND_RETURN1(false);
}

bool WfdSession::sendWfdM1() {
    ENTER();

    MediaMetaSP m1 = MediaMeta::create();
    if (!m1) {
        ERROR("fail to create media meta for M1");
        EXIT_AND_RETURN1( false );
    }

    m1->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M1);
    m1->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m1->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_OPTION);

    mRtsp->sendRtspMessage(m1);

    mRequestSend.push_back(m1);

    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM2() {
    ENTER();

    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM3() {
    ENTER();

    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM4() {
    ENTER();

    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM5(const char* method) {
    ENTER();

    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM6() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM7() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM8() {
    ENTER();
    EXIT_AND_RETURN( true );

}

bool WfdSession::sendWfdM9() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM10() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM11() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM12() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM13() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM14() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM15() {
    ENTER();
    EXIT_AND_RETURN( true );
}

bool WfdSession::sendWfdM16() {
    ENTER();
    EXIT_AND_RETURN( true );
}
//////
bool WfdSession::handleResponseM1(MediaMetaSP response) {
    ENTER();
    INFO("receive M1 response");
    // no need to any thing

    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM2(MediaMetaSP response) {
    ENTER();
    INFO("receive M2 response");
    // no need to any thing

    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM3(MediaMetaSP response) {
    ENTER();
    INFO("receive M3 response");

    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM4(MediaMetaSP response) {
    ENTER();
    INFO("receive M4 response");

    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM5(MediaMetaSP response) {
    ENTER();
    INFO("receive M5 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM6(MediaMetaSP response) {
    ENTER();
    INFO("receive M6 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM7(MediaMetaSP response) {
    ENTER();
    INFO("receive M7 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM8(MediaMetaSP response) {
    ENTER();
    INFO("receive M8 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM9(MediaMetaSP response) {
    ENTER();
    INFO("receive M9 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM10(MediaMetaSP response) {
    ENTER();
    INFO("receive M10 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM11(MediaMetaSP response) {
    ENTER();
    INFO("receive M11 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM12(MediaMetaSP response) {
    ENTER();
    INFO("receive M12 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM13(MediaMetaSP response) {
    ENTER();
    INFO("receive M13 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM14(MediaMetaSP response) {
    ENTER();
    INFO("receive M14 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM15(MediaMetaSP response) {
    ENTER();
    INFO("receive M15 response");
    EXIT_AND_RETURN( true );
}

bool WfdSession::handleResponseM16(MediaMetaSP response) {
    ENTER();
    INFO("receive M16 response");
    EXIT_AND_RETURN( true );
}

void WfdSession::handlePlayerStart(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId)
{
    ENTER();

    EXIT();
}

void WfdSession::handleWfdRequestIDR(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId) {
    ENTER();

    EXIT();
}

const char * WfdSession::MM_LOG_TAG = "WfdSession";

} // end of namespace YUNOS_MM
