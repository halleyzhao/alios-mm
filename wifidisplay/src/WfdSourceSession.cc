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

#include "WfdSourceSession.h"
#include "WfdParameters.h"
#include "WfdMediaMsgInfo.h"

#include "multimedia/media_attr_str.h"
#include "multimedia/component.h"

namespace YUNOS_MM {

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

WfdSourceSession::WfdSourceSession(const char* iface, IWfdSessionCB* listener)
    : WfdSession(iface, listener, true),
      mRecorder(NULL) {

    ENTER1();

    EXIT1();
}

WfdSourceSession::~WfdSourceSession() {
    ENTER1();

    EXIT1();
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

void WfdSourceSession::onMessage(int msg, int param1, int param2, const MMParamSP param) {
    ENTER();

    WfdSession::onMediaPipelineMessage(msg, param1, param2, param);

    EXIT();
}

void WfdSourceSession::handleMediaEvent(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId) {
    ENTER();

    {
        MMAutoLock lock(mLock);

        if (mState == UNINITIALIZED) {
            INFO("recorder is UNINITIALIZED, ignore msg %d", msg);
            EXIT1();
        }
    }

    //int param2 = -1;
    MMParamRefBase *metaRef = DYNAMIC_CAST<MMParamRefBase*>(param3.get());
    if(!metaRef){
        ERROR("get metaRef fail");
        EXIT1();
    }

    MMParamSP param = metaRef->getParam();

    switch ( msg ) {
        case Component::kEventPrepareResult:
            INFO("kEventPrepareResult: param1: %d\n", param1);
            if (mState != PREPARING)
                WARNING("receive prepared while not in preparing state %d", mState);
            mState = PREPARED;
            break;
        case Component::kEventStartResult:
            INFO("kEventStartResult: param1: %d\n", param1);
            if (mState != STARTING)
                WARNING("receive started while not in starting state %d", mState);
            mState = STARTED;
            mRtspState = PLAYING;
            break;
        case Component::kEventStopped:
            INFO("kEventStopped: param1: %d\n", param1);
            mState = STOPPED;
            break;
        case Component::kEventEOS:
            INFO("kEventEOS: param1: %d\n", param1);
            break;
#if 0
        case MSG_INFO_EXT: {
                if (meta)
                    param2 = meta->readInt32();
                if ((int)param1 == WFD_MSG_INFO_RTP_CREATE) {
                    MMAutoLock lock(mLock);
                    mCondition.signal();
                }
            }
            INFO("MSG_INFO_EXT: param1 %d param2 %d\n", param1, param2);
            break;
#endif
        case Component::kEventError:
            INFO("kEventError, param1: %d\n", param1);
            mWfdSessionCb->onDisplayError(1); // error unknown
            break;
        default:
            INFO("msg: %d, ignore\n", msg);
            break;
    }

    EXIT();
}

bool WfdSourceSession::handleRtspOption(MediaMetaSP request) {
    ENTER();

    bool ret = WfdSession::handleRtspOption(request);

    EXIT_AND_RETURN(ret);
}

bool WfdSourceSession::handleRtspSetup(MediaMetaSP request) {
    ENTER();
    MediaMetaSP meta;
    int cseq = -1;
    const char* transport = NULL;

    INFO("process request(SETUP) from client");

    if (mRtspState != ABOUT_TO_SETUP || mRecorder) {
        ERROR("bad rtsp status %d, recorder is %p", mRtspState, mRecorder);
        EXIT_AND_RETURN(false);
    }

    setupMediaPipeline();

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("request(%p) has no valid cseq(%d)", request.get(), cseq);
        EXIT_AND_RETURN(false);
    }

    meta = MediaMeta::create();
    if (!meta) {
        ERROR("fail to create meta for M5 response");
        EXIT_AND_RETURN(false);
    }

    meta->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
    meta->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_SETUP);
    meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M6); // response to M6
    meta->setInt32(RTSP_MSG_CSEQ, cseq); // use cseq from request
    meta->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);
    //FIXME:now during the wifidisplay, the source rtp port is invaild!
#define WFD_SOURCE_RTP_PORT 6667
    meta->setInt32(LOCAL_RTP_PORT, WFD_SOURCE_RTP_PORT);

    if (!mRecorder) {
        // 412 pre-process fail
        meta->setInt32(RTSP_STATUS, 412);
        meta->setString(RTSP_REASON, RTSP_REASON_PREPROCESS_FAIL);
    }

    request->getString(RTSP_MSG_TRANSPORT, transport);
    meta->setString(RTSP_MSG_TRANSPORT, transport);

    // M5 response
    mm_status_t status = mRtsp->sendRtspMessage(meta);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("fail to create meta for response");
        EXIT_AND_RETURN(false);
    }

    mRtspState = READY;

    postMsg(WFD_MSG_KEEP_ALIVE, 0, NULL, CHECK_SESSION_ALIVE_PERIOD * 1000000);

    EXIT_AND_RETURN(true);
}

bool WfdSourceSession::handleRtspPlay(MediaMetaSP request) {
    ENTER();

    MediaMetaSP meta;
    int cseq = -1;
    int session;

    INFO("process request(PLAY) from client");

    if (mRtspState != READY || !mRecorder) {
        ERROR("bad rtsp status %d, recorder is %p", mRtspState, mRecorder);
        EXIT_AND_RETURN(false);
    }

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("request(%p) has no valid cseq(%d)", request.get(), cseq);
        EXIT_AND_RETURN(false);
    }

    if (!request->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("request(%p) has no session field", request.get());
        EXIT_AND_RETURN(false);
    }

    if (session != mRtspPlaySession) {
        ERROR("Unkonwn session %d in PLAY request", session);
        EXIT_AND_RETURN(false);
    }

    meta = MediaMeta::create();
    if (!meta) {
        ERROR("fail to create meta for M7 response");
        EXIT_AND_RETURN(false);
    }

    meta->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
    meta->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_PLAY);
    meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M7); // response to M7
    meta->setInt32(RTSP_MSG_CSEQ, cseq); // use cseq from request
    meta->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);

    // TODO check mState is PREPARED, if not, send rtsp response with error
    mState = STARTING;

    // M7 response
    mm_status_t status = mRtsp->sendRtspMessage(meta);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("fail to create meta for response");
        EXIT_AND_RETURN(false);
    }

    status = mRecorder->start();

    if (status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC) {
        ERROR("fail to start media recorder");
        teardownMediaPipeline();
    }

    mRtspState = ABOUT_TO_PLAY;

    EXIT_AND_RETURN(true);
}

bool WfdSourceSession::handleRtspPause(MediaMetaSP request) {
    ENTER();

    bool ret = WfdSession::handleRtspPause(request);
    EXIT_AND_RETURN(ret);
}
bool WfdSourceSession::handleRtspTearDown(MediaMetaSP request) {
    ENTER();

    MediaMetaSP meta;
    int cseq = -1;
    int session;
    bool ret = true;

    INFO("process request(TEARDOWN) from client");

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("request(%p) has no valid cseq(%d)", request.get(), cseq);
        EXIT_AND_RETURN(false);
    }

    if (!request->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("request(%p) has no session field", request.get());
        EXIT_AND_RETURN(false);
    }

    meta = MediaMeta::create();
    if (!meta) {
        ERROR("fail to create meta for M8 response");
        EXIT_AND_RETURN(false);
    }

    meta->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
    meta->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_TEARDOWN);
    meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M8); // response to M8
    meta->setInt32(RTSP_MSG_CSEQ, cseq); // use cseq from request
    meta->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);

    // M8 response
    mm_status_t status = mRtsp->sendRtspMessage(meta);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("fail to create meta for response");
    }

    if (mRtspState != ABOUT_TO_TEARDOWN || !mRecorder) {
        INFO("invalid tear down request, rtsp state(%d), recorder %p",
             mRtspState, mRecorder);
        ret = false;
    }

    mRtspState = TEARDOWN;

    stopInternal();

    EXIT_AND_RETURN(ret);
}

bool WfdSourceSession::handleRtspSetParameter(MediaMetaSP request) {
    ENTER();

    const char* data = NULL;
    MediaMetaSP meta;
    int cseq = -1;
    int session;

    INFO("process request(SET_PARAMETER) from client");

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("request(%p) has no valid cseq(%d)", request.get(), cseq);
        EXIT_AND_RETURN1(false);
    }

    if (!request->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("request(%p) has no session field", request.get());
        EXIT_AND_RETURN1(false);
    }

    if (!request || !request->getString(RTSP_MSG_CONTENT, data) || !data) {
        ERROR("request(%p) contains no rtsp msg body (%p)",
              request.get(), data);
        EXIT_AND_RETURN1(false);
    }

    if (!strncmp(WFD_RTSP_IDR_REQUEST, data, strlen(WFD_RTSP_IDR_REQUEST))) {
        INFO("receive idr request, will to encode one IDR frame\n");
        //send the idr request to codec
        MediaMetaSP meta2 = MediaMeta::create();
        meta2->setString(MEDIA_ATTR_IDR_FRAME, "yes");
        if (mRecorder)
            mRecorder->setParameter( meta2 );
        else
            ERROR("find the mRecorder:NULL\n");

        // TODO request IDR to recorder
        meta = MediaMeta::create();
        if (!meta) {
            ERROR("fail to create meta for M8 response");
            EXIT_AND_RETURN1(false);
        }

        meta->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
        meta->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_SETPARAMETER);
        meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M13); // response to M8
        meta->setInt32(RTSP_MSG_CSEQ, cseq); // use cseq from request

        if (mRtspPlaySession == session)
            meta->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);
        else
            ERROR("bad session received(%d), my session is %d",
                   session, mRtspPlaySession);

        // M13 response
        mm_status_t status = mRtsp->sendRtspMessage(meta);
        if (status != MM_ERROR_SUCCESS) {
            ERROR("fail to create meta for response");
        }
        EXIT_AND_RETURN(true);
    }

    EXIT_AND_RETURN1(false);
}

bool WfdSourceSession::sendWfdM3() {
    ENTER();
    MediaMetaSP m3 = MediaMeta::create();

    if (!m3) {
        ERROR("fail to create media meta for M3");
        EXIT_AND_RETURN1( false );
    }

    m3->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M3);
    m3->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m3->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_GETPARAMETER);

    mRtsp->sendRtspMessage(m3);

    mRequestSend.push_back(m3);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::sendWfdM4() {
    ENTER();
    MediaMetaSP m4 = mWfdSinkParameters;

    if (!m4) {
        ERROR("client parameters is NULL M4");
        EXIT_AND_RETURN1( false );
    }

    m4->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M4);
    m4->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m4->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_SETPARAMETER);
    m4->setString(WFD_RTSP_LOCAL_IP, mLocalIP.c_str());
    mRtsp->sendRtspMessage(m4);

    mRequestSend.push_back(m4);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::sendWfdM5(const char* method) {
    ENTER();

    MediaMetaSP m5 = MediaMeta::create();

    if (!m5) {
        ERROR("fail to create media meta for M5");
        EXIT_AND_RETURN1( false );
    }

    m5->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M5);
    m5->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m5->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_SETPARAMETER);
    //m5->setString(WFD_RTSP_TRIGGER_METHOD, RTSP_REQUEST_METHOD_SETUP);
    m5->setString(WFD_RTSP_TRIGGER_METHOD, method);

    mRtsp->sendRtspMessage(m5);
    mRtspState = ABOUT_TO_SETUP;

    mRequestSend.push_back(m5);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::sendWfdM12() {
    ENTER();

    WfdSession::sendWfdM12();

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::sendWfdM14() {
    ENTER();

    WfdSession::sendWfdM14();

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::sendWfdM15() {
    ENTER();

    WfdSession::sendWfdM15();

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::sendWfdM16() {
    ENTER();

    MediaMetaSP m16 = MediaMeta::create();

    if (!m16) {
        ERROR("client parameters is NULL M4");
        EXIT_AND_RETURN1( false );
    }

    m16->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M16);
    m16->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m16->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_GETPARAMETER);
    m16->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);

    mRtsp->sendRtspMessage(m16);

    mRequestSend.push_back(m16);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::handleResponseM3(MediaMetaSP response) {
    ENTER();
    const char* data = NULL;
    INFO("receive M3 response");

    mRtsp->parseWfdParameter(response);

    // save a copy of client parameter set
    mWfdSinkParameters = response;

    if (response->getString(WFD_RTSP_CLIENT_RTP_PORTS, data)) {

        unsigned port0 = 0, port1 = 0;
        if (sscanf(data,
                   "RTP/AVP/UDP;unicast %u %u mode=play",
                   &port0,
                   &port1) == 2
            || sscanf(data,
                   "RTP/AVP/TCP;unicast %u %u mode=play",
                   &port0,
                   &port1) == 2) {
                if (port0 == 0 || port0 > 65535 || port1 != 0) {
                    ERROR("Client chose its wfd_client_rtp_ports poorly (%s)",
                          data);

                    mWfdSessionCb->onDisplayError(1); // error unknown

                    EXIT_AND_RETURN1( false );
                }
        } else if (strcmp(data, "RTP/AVP/TCP;interleaved mode=play")) {
            ERROR("Unsupported value for wfd_client_rtp_ports (%s)",
                  data);

            mWfdSessionCb->onDisplayError(1); // error unknown

            EXIT_AND_RETURN1( false );
        }

        mRtpPort = port0;
    }

    data = NULL;
    if (response->getString(WFD_RTSP_VIDEO_FORMATS, data) && strcmp("none", data)) {

    } else
        INFO("client doesn't support video, %s", data);

    if (response->getString(WFD_RTSP_AUDIO_CODECS, data) && strcmp("none", data)) {

    } else
        INFO("client doesn't support audio, %s", data);

    if (response->getString(WFD_RTSP_CONTENT_PROTECTION, data)) {
        // do nothing
    }

    sendWfdM4();

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::handleResponseM4(MediaMetaSP response) {
    ENTER();
    INFO("receive M4 response");

    sendWfdM5(RTSP_REQUEST_METHOD_SETUP);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::handleResponseM5(MediaMetaSP response) {
    ENTER();
    INFO("receive M5 response");
    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::handleResponseM12(MediaMetaSP response) {
    ENTER();

    WfdSession::handleResponseM12(response);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::handleResponseM14(MediaMetaSP response) {
    ENTER();

    WfdSession::handleResponseM14(response);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::handleResponseM15(MediaMetaSP response) {
    ENTER();

    WfdSession::handleResponseM15(response);

    EXIT_AND_RETURN( true );
}

bool WfdSourceSession::handleResponseM16(MediaMetaSP response) {
    ENTER();
    INFO("receive M16 response");
    EXIT_AND_RETURN( true );
}

void WfdSourceSession::setupMediaPipeline() {
    ENTER();

    int usage = RU_None;
    const char* data = NULL;

    if (mWfdSinkParameters->getString(WFD_RTSP_VIDEO_FORMATS, data) &&
        strcmp("none", data)) {
        INFO("media recorder has video");
        usage |= RU_VideoRecorder;
    }

#ifdef WFD_HAS_AUDIO
    if (mWfdSinkParameters->getString(WFD_RTSP_AUDIO_CODECS, data) &&
        strcmp("none", data)) {
        INFO("media recorder has audio");
        usage |= RU_AudioRecorder;
    }
#endif

    if (usage == RU_None)
        EXIT1();

    mRecorder = new CowRecorder();
    if (!mRecorder)
        EXIT1();

    mRtspPlaySession = (int32_t)mRecorder;
    if (mRtspPlaySession < 0)
        mRtspPlaySession *= -1;

    mRecorder->setRecorderUsage((RecorderUsage)usage);
    mRecorder->setListener(this);

    if (usage & RU_VideoRecorder) {
        bool isAuto = mm_check_env_str("mm.wfd.source.video", NULL, "auto", false);

        if (isAuto)
            mRecorder->setVideoSourceUri("auto://");
        else
            mRecorder->setVideoSourceUri("wfd://");

	mRecorder->setVideoEncoder(MEDIA_MIMETYPE_VIDEO_AVC);
        mRecorder->setVideoSourceFormat(1280, 720, 'YV12');
        // TODO set to 60fps
        INFO("***********************************************************todo set to 60fps");
    }

#ifdef WFD_HAS_AUDIO
    if (usage & RU_AudioRecorder) {
        mRecorder->setAudioSourceUri("cras://remote_submix");
        //mRecorder->setAudioSourceUri("cras://mic");
        //mRecorder->setAudioEncoder(MEDIA_MIMETYPE_AUDIO_RAW);
        mRecorder->setAudioEncoder(MEDIA_MIMETYPE_AUDIO_AAC);
    }
#endif

#ifdef WFD_OUTPUT_RTP
    char buf[128];
    sprintf(buf, "rtp://%s:%d/video src://%s", mRemoteIP.c_str(), mRtpPort, mLocalIP.c_str());
    INFO("set recorder output format: %s", buf);
    mRecorder->setOutputFormat(buf);
    mRecorder->setOutputFile(buf);
    sprintf(buf, "rtp://%s:%d/audio src://%s", mRemoteIP.c_str(), mRtpPort, mLocalIP.c_str());
    INFO("set recorder output format: %s", buf);
    mRecorder->setOutputFormat(buf);
#else
    mRecorder->setOutputFormat("mp4");
    mRecorder->setOutputFile("/data/wfd-record.mp4");
#endif

    //mm_status_t status = mRecorder->prepareAsync();
    // FIXME use Async
    MediaMetaSP meta = MediaMeta::create();
    meta->setFloat(MEDIA_ATTR_FRAME_RATE, 24.0f);
    mRecorder->setParameter(meta);

    mm_status_t status = mRecorder->prepare();

    if (status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC) {
        ERROR("prepare media recorder with error %d", status);
        teardownMediaPipeline();
    }

    mState = PREPARING;
    //FIXME: now the local rtp port is invaild, comment it
    //if need, may open it
    /*mLocalRtpPort = 6666;
    mLocalRtpPort = 6667;
    // wait for port binding for rtp
    if (mLocalRtpPort < 0) {
        INFO("waiting RTP setup");
        mCondition.timedWait(CHECK_SESSION_ALIVE_PERIOD * 1000000ll);
    }

    if (mLocalRtpPort < 0) {
        ERROR("media recorder fail to setup rtp");
        //mRecorder->setListener(NULL);
        //MediaRecorder::destroy(mRecorder);
        mRecorder->reset();
        MediaRecorder::destroy(mRecorder);
        mRecorder = NULL;
    }

    INFO("RTP binding to port %d", mLocalRtpPort);*/

    mWfdSessionCb->onDisplayConnected(1280, 720, 0/* secure */, mRtspPlaySession);

    INFO("media recorder prepare return status %d", mState);

    EXIT();
}

void WfdSourceSession::stopMediaPipeline() {
    ENTER();

    if (mRecorder)
        mRecorder->stop();

    EXIT();
}

void WfdSourceSession::teardownMediaPipeline() {

    ENTER();

    if (mRecorder) {
        INFO("destroy recorder");
        mRecorder->setListener(NULL);
        mRecorder->reset();
        MM_RELEASE(mRecorder);
        mRecorder = NULL;
    }

    EXIT();
}

} // end of namespace YUNOS_MM
