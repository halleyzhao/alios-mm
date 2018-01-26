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
#include "WfdSinkSession.h"
#include "WfdParameters.h"
#include "WfdMediaMsgInfo.h"
#include "multimedia/media_attr_str.h"

#include "native_surface_help.h"

namespace YUNOS_MM
{

#define ENTER() INFO(">>>\n")
#define EXIT() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define WFD_MSG_START            1
#define WFD_MSG_RTSP             2
#define WFD_MSG_MEDIA            3
#define WFD_MSG_KEEP_ALIVE       4
#define WFD_MSG_STOP             5
#define WFD_MSG_TEARDOWN_TIMEOUT 6
#define WFD_MSG_REQUEST_IDR      7
#define WFD_MSG_PLAYER_START     8

WfdSinkSession::WfdSinkSession(const char* iface, IWfdSessionCB* listener, WfdSinkConfig *config)
                                     : WfdSession(iface, listener, false),
                                       mWindow(NULL),
                                       mOwnWindow(true),
                                       mPlayer(NULL),
                                       mRequestIDR(false),
                                       mRequestIDRPending(false),
                                       mWidth(-1),
                                       mHeight(-1),
                                       mParametersFlags(-1)
{
    ENTER();
    if (config) {
        INFO("get display surface");
        mWindow = config->surface;
        mOwnWindow = false;
    }
    EXIT();
}

WfdSinkSession::~WfdSinkSession()
{
    ENTER();

    if (mOwnWindow && mWindow) {
        WARNING("surface should be destroy before");
        destroySimpleSurface((WindowSurface*)mWindow);
    }

    EXIT();
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

void WfdSinkSession::onMessage(int msg, int param1, int param2, const MMParamSP param)
{
    ENTER();
    WfdSession::onMediaPipelineMessage(msg, param1, param2, param);
    EXIT();
}

void WfdSinkSession::handleMediaEvent(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId)
{
    ENTER();
    INFO("WfdSinkSession::handleMediaEvent :state:%d, msg:%d\n", mState, msg);
    {
        MMAutoLock lock(mLock);

        if (mState == UNINITIALIZED) {
            INFO("player is UNINITIALIZED, ignore msg %d", msg);
            EXIT();
        }
    }

    //int param2 = -1;
    MMParamRefBase *metaRef = DYNAMIC_CAST<MMParamRefBase*>(param3.get());
    if(!metaRef) {
        ERROR("get metaRef fail");
        EXIT();
    }

    MMParamSP param = metaRef->getParam();

    switch ( msg ) {
        case Component::kEventPrepareResult:
            INFO("kEventPrepareResult: param1: %d\n", param1);
            if (mState != PREPARING)
                WARNING("receive prepared while not in preparing state");
            mState = PREPARED;
            postMsg(WFD_MSG_PLAYER_START, 0, NULL, 0);
            break;
        case Component::kEventStartResult:
            INFO("kEventStartResult: param1: %d\n", param1);
            if (mState != STARTING)
                WARNING("receive started while not in starting state");
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
        case Component::kEventError:
            INFO("kEventError, param1: %d\n", param1);
            mWfdSessionCb->onDisplayError(1); // error unknown
            break;
        case Component::kEventRequestIDR:
            INFO("get the message of request idr\n");
            {
                MMAutoLock lock(mLock);
                mRequestIDR = true;
            }
            break;
        default:
            INFO("msg: %d, ignore\n", msg);
            break;
    }

    EXIT();
}

bool WfdSinkSession::handleRtspOption(MediaMetaSP request)
{
    ENTER();

    bool ret = WfdSession::handleRtspOption(request);

    EXIT_AND_RETURN(ret);
}

bool WfdSinkSession::handleRtspGetParameter(MediaMetaSP request)
{
    int size, messageId, session, cseq;

    if ( request->getInt32(RTSP_MSG_CONTENT_LENGTH, size) )
        messageId = 3;
    else if ( request->getInt32(RTSP_MSG_SESSION, session) )
        messageId = 16;
    else {
        ERROR("request has errorr\n");
        EXIT_AND_RETURN( false );
    }

    if ( !request->getInt32(RTSP_MSG_CSEQ, cseq) ) {
        ERROR("read the parameter:%s failed\n", RTSP_MSG_CSEQ);
        EXIT_AND_RETURN( false );
    }

    MediaMetaSP meta;
    bool ret;
    if (messageId == 3)
        ret = handleRequestM3(request);
    else
        ret = handleRequestM16(request);
    if (!ret) {
        ERROR("handle the message%d find error\n", messageId);
        EXIT_AND_RETURN( false );
    }

    meta = MediaMeta::create();
    if (!meta) {
       ERROR("fail to create meta for M7 response");
       EXIT_AND_RETURN(false);
    }
    meta->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
    meta->setString(RTSP_REQUEST_METHOD, RTSP_MSG_VERSION);
    meta->setInt32(RTSP_MSG_CSEQ, cseq);
    if (messageId == 3) {
        meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M3); // response to M3
        meta->setInt32(WFD_RTSP_PARAMETER_FLAG, mParametersFlags);
        meta->setInt32(LOCAL_RTP_PORT, mRtpPort);
    } else {
        meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M16); // response to M16
        meta->setInt32(RTSP_MSG_SESSION, session);
    }

    mm_status_t status = mRtsp->sendRtspMessage(meta);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("fail to create meta for response");
        EXIT_AND_RETURN(false);
    }
    EXIT_AND_RETURN(true);
}

bool WfdSinkSession::handleRequestM3(MediaMetaSP request)
{
    ENTER();
    const char* content = NULL;
    int cseq;
    if (!request->getString( RTSP_MSG_CONTENT, content)) {
        ERROR("get the M3 content failed\n");
        EXIT_AND_RETURN(false);
    }

    INFO("M3's content:%s,size:%d\n", content, strlen(content));

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not find the cseq from request\n");
        EXIT_AND_RETURN(false);
    }

    int flags = 0,i;
    const char *p = NULL, *q = content;
    const char *keyTab[8] = { WFD_RTSP_VIDEO_FORMATS,
                              WFD_RTSP_AUDIO_CODECS,
                              "wfd_3d_video_formats",
                              WFD_RTSP_CONTENT_PROTECTION,
                              "wfd_display_edid",
                              "wfd_coupled_sink",
                              WFD_RTSP_CLIENT_RTP_PORTS,
                              WFD_RTSP_UIBC_CAPABILITY };
    for ( i = 0; i < 8; i++) {
        p = strstr( q, keyTab[i]);
        if (p) {
           flags |= (1<<i);
        }
    }

    mParametersFlags = flags;
    INFO("mParameterFlags:0x%x\n", flags);
    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleRequestM16(MediaMetaSP request)
{
    ENTER();
    int cseq,session;
    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not find the cseq\n");
        EXIT_AND_RETURN(false);
    }
    if (!request->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("not find the session\n");
        EXIT_AND_RETURN(false);
    }

    if(session != mRtspPlaySession) {
       ERROR("session:%d != mRtspPlaySession:%d\n", session, mRtspPlaySession);
       EXIT_AND_RETURN(false);
    }
    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleRtspSetParameter(MediaMetaSP request)
{
    ENTER();
    const char* content = NULL;
    int cseq, messageID;

    if ( !request->getString( RTSP_MSG_CONTENT, content)) {
       ERROR("not found the %s\n", RTSP_MSG_CONTENT);
       EXIT_AND_RETURN(false);
    }

    if ( strstr(content, WFD_RTSP_TRIGGER_METHOD)) {
        messageID = 5;
    } else if ( strstr( content, WFD_RTSP_CLIENT_RTP_PORTS)){
        messageID = 4;
    } else {
        ERROR("not find the message\n");
        EXIT_AND_RETURN(false);
    }

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not find the cseq from request\n");
        EXIT_AND_RETURN(false);
    }

    MediaMetaSP meta;
    bool ret;
    if (messageID == 4)
        ret = handleRequestM4(request);
    else
        ret = handleRequestM5(request);
    if (!ret) {
        ERROR("find error\n");
        EXIT_AND_RETURN(false);
    }

    meta = MediaMeta::create();
    if (!meta) {
       ERROR("fail to create meta for response");
       EXIT_AND_RETURN(false);
    }
    meta->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
    meta->setString(RTSP_REQUEST_METHOD, RTSP_MSG_VERSION);
    meta->setInt32(RTSP_MSG_CSEQ, cseq);
    if (messageID == 4) {
        meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M4); // response to M3
    } else {
        meta->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M5); // response to M16
    }

    mm_status_t status = mRtsp->sendRtspMessage(meta);
    if (status != MM_ERROR_SUCCESS) {
        ERROR("fail to create meta for response");
        EXIT_AND_RETURN(false);
    }

    if (messageID == 5) {
       const char *operate = NULL;
       operate = strstr(content, WFD_RTSP_TRIGGER_METHOD);
       if( !operate ) {
          ERROR("not find the method\n");
          EXIT_AND_RETURN(false);
       }
       operate += strlen(WFD_RTSP_TRIGGER_METHOD);
       operate += 2;
       if (strstr(operate, "SETUP"))
           sendWfdM6();
       else if (strstr(operate, "PLAY"))
           sendWfdM7();
       else if (strstr(operate, "TEARDOWN")) {
           stopMediaPipeline();
           sendWfdM8();
       }
       else {
          ERROR("not support the method:%s\n", operate);
          EXIT_AND_RETURN(false);
       }
    }

    EXIT_AND_RETURN(true);
}

bool WfdSinkSession::handleRtspPlay(MediaMetaSP request)
{
    ENTER();

    playMediaPipeline();

    EXIT_AND_RETURN(true);
}

bool WfdSinkSession::handleRequestM4(MediaMetaSP request)
{
    ENTER();
    const char* content = NULL;
    int cseq, i;
    if (!request->getString( RTSP_MSG_CONTENT, content)) {
        ERROR("get the M4 content failed\n");
        EXIT_AND_RETURN(false);
    }

    INFO("M4's content:%s,size:%d\n", content, strlen(content));

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not find the cseq from request\n");
        EXIT_AND_RETURN(false);
    }

    const char *start = NULL, *end, *line = NULL;
    start = strstr(content, WFD_RTSP_VIDEO_FORMATS);
    start += strlen(WFD_RTSP_VIDEO_FORMATS);//start =": 00......"
    if (parseVideoFormat(start+1) != MM_ERROR_SUCCESS) {
        ERROR("not get the width or height\n");
        EXIT_AND_RETURN(false);
    }

    char buf[128];
    int port;
    //get the source ip
    line = strstr(content, WFD_RTSP_PRESENTION_URL);
    if (!line) {
        ERROR("not find the %s\n", WFD_RTSP_PRESENTION_URL );
        EXIT_AND_RETURN(false);
    }
    start = strstr(line, "rtsp://");
    end = strstr(line, "/wfd1.0/streamid=0 none");
    if ((!start) || (!end)) {
       ERROR("%s has problem\n", line);
       EXIT_AND_RETURN(false);
    }
    i = 0;
    start += strlen("rtsp://");
    while(start < end)
        buf[i++] = *start++;
    buf[i] = '\0';
    mRemoteIP = buf;

    //get the client port
    line = strstr(content, WFD_RTSP_CLIENT_RTP_PORTS);
    if (!line) {
        ERROR("not find the %s\n", WFD_RTSP_CLIENT_RTP_PORTS );
        EXIT_AND_RETURN(false);
    }

    start = strstr(line, "RTP/AVP/UDP;unicast ");
    start += strlen("RTP/AVP/UDP;unicast ");
    char * end2 = NULL;
    port= strtoul(start, &end2, 10);
    if (end2 == start || *end2 != ' ') {
        ERROR("get the local port failed:%s\n", line);
        EXIT_AND_RETURN( false);
    }
    if (port != mRtpPort) {
        INFO("CATION:the sink port changed:%d->%d", mRtpPort, port);
        mRtpPort = port;
    }
    INFO("source rtp ip:%s, sink rtp port:%d,width:%d,height:%d\n",
        mRemoteIP.c_str(), mRtpPort, mWidth, mHeight);

    // get uibc capability
    // wfd_uibc_capability: input_category_list=GENERIC;generic_cap_list=Keyboard, Mouse, SingleTouch, MultiTouch;hidc_cap_list=none;port=46133
    line = strstr(content, WFD_RTSP_UIBC_CAPABILITY);
    if (line) {
        INFO("setting uibc capability");
        start = strstr(line, "Keyboard");
        if (start) {
            INFO("UIBC input type Keyboard");
            mUIBCCtrl.mKeyboard = true;
        }

        start = strstr(line, "Mouse");
        if (start) {
            INFO("UIBC input type Mouse");
            mUIBCCtrl.mMouse = true;
        }

        start = strstr(line, "SingleTouch");
        if (start) {
            INFO("UIBC input type SingleTouch");
            mUIBCCtrl.mSingleTouch = true;
        }

        start = strstr(line, "MultiTouch");
        if (start) {
            INFO("UIBC input type MultiTouch");
            mUIBCCtrl.mMultiTouch = true;
        }

        start = strstr(line, "port=");
        if (start) {
            start += 5;
            unsigned int port = strtoul(start, NULL, 10);
            if (port <= 65535)
                mUIBCCtrl.mUibcPort = (int)port;
            else
                mUIBCCtrl.mUibcPort = -1;
            INFO("UIBC port is %d", mUIBCCtrl.mUibcPort);
        } else {
            ERROR("no UIBC port");
            mUIBCCtrl.mUibcPort = -1;
        }
    }

    // get uibc setting
    // wfd_uibc_setting: enable
    line = strstr(content, WFD_RTSP_UIBC_SETTING);
    if (line) {
        INFO("uibc setting");
        start = strstr(line, "enable");
        if (start)
            mUIBCCtrl.mEnable = true;
        else
            mUIBCCtrl.mEnable = false;

        mWfdSessionCb->onDisplayUIBCInfo(mUIBCCtrl.mEnable, mUIBCCtrl.mUibcPort);
        INFO("UIBC setting enable %d", mUIBCCtrl.mEnable);
    }

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleRequestM5(MediaMetaSP request)
{
    ENTER();
    int cseq;

    if (!request->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not find the cseq from request\n");
        EXIT_AND_RETURN(false);
    }

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::sendWfdM2()
{
    ENTER();

    MediaMetaSP m2 = MediaMeta::create();
    if (!m2) {
        ERROR("fail to create media meta for M1");
        EXIT_AND_RETURN( false );
    }

    m2->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M2);
    m2->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m2->setString(RTSP_REQUEST_METHOD, RTSP_REQUEST_METHOD_OPTION);

    mRtsp->sendRtspMessage(m2);

    mRequestSend.push_back(m2);
    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::sendWfdM6()
{
    ENTER();
    MediaMetaSP m6 = MediaMeta::create();
    if (!m6) {
        ERROR("fail to create media meta for M6");
        EXIT_AND_RETURN( false );
    }

    m6->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M6);
    m6->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m6->setString(WFD_RTSP_REMOTE_IP, mRemoteIP.c_str());
    m6->setInt32(LOCAL_RTP_PORT, mRtpPort);
    mRtsp->sendRtspMessage(m6);

    mRequestSend.push_back(m6);

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::sendWfdM7()
{
    ENTER();
    MediaMetaSP m7 = MediaMeta::create();
    if (!m7) {
        ERROR("fail to create media meta for M7");
        EXIT_AND_RETURN( false );
    }

    m7->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M7);
    m7->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m7->setString(WFD_RTSP_REMOTE_IP, mRemoteIP.c_str());
    m7->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);
    mRtsp->sendRtspMessage(m7);

    mRequestSend.push_back(m7);

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::sendWfdM8()
{
    ENTER();
    MediaMetaSP m8 = MediaMeta::create();
    if (!m8) {
        ERROR("fail to create media meta for M8");
        EXIT_AND_RETURN( false );
    }

    m8->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M8);
    m8->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m8->setString(WFD_RTSP_REMOTE_IP, mRemoteIP.c_str());
    m8->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);
    mRtsp->sendRtspMessage(m8);

    mRequestSend.push_back(m8);
    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::sendWfdM13()
{
    ENTER();
    MediaMetaSP m13 = MediaMeta::create();
    if (!m13) {
        ERROR("fail to create media meta for M13");
        EXIT_AND_RETURN( false );
    }

    m13->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M13);
    m13->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m13->setString(WFD_RTSP_REMOTE_IP, mLocalIP.c_str());
    m13->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);
    mRtsp->sendRtspMessage(m13);

    mRequestSend.push_back(m13);
    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::sendWfdM9()
{
    ENTER();
    MediaMetaSP m9 = MediaMeta::create();
    if (!m9) {
        ERROR("fail to create media meta for m9");
        EXIT_AND_RETURN( false );
    }

    m9->setString(WFD_RTSP_MSG_ID, WFD_RTSP_MSG_M9);
    m9->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
    m9->setString(WFD_RTSP_REMOTE_IP, mRemoteIP.c_str());
    m9->setInt32(RTSP_MSG_SESSION, mRtspPlaySession);
    mRtsp->sendRtspMessage(m9);

    mRequestSend.push_back(m9);
    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleResponseM2( MediaMetaSP response )
{
    ENTER();
    int cseq;

    if (!response->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not find the cseq from request\n");
        EXIT_AND_RETURN(false);
    }

    INFO("find the cseq:%d\n", cseq);

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleResponseM6( MediaMetaSP response )
{
    ENTER();
    const char *data= NULL;
    int session, sinkPort, cseq = -1, number;
    //check the header
    if ((!response->getString(RTSP_VERSION, data))||(strcmp(data,RTSP_MSG_VERSION))) {
        ERROR("header has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getInt32(RTSP_STATUS, number))||(number != 200)) {
        ERROR("number has problem:%d\n", number);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getString(RTSP_REASON, data))||(strcmp(data,"OK"))) {
        ERROR("result has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    //session
    if (!response->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("not find the session\n");
        EXIT_AND_RETURN( false );
    }
    mRtspPlaySession = session;
    //sink port && source port
    if (!response->getString(RTSP_MSG_TRANSPORT, data)) {
        ERROR("not find the %s\n", RTSP_MSG_TRANSPORT);
        EXIT_AND_RETURN( false );
    }
    const char *start = NULL;
    char *end = NULL;

    start = strstr(data, "client_port=");
    if (!start) {
        ERROR("not find the client_port\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }
    start += strlen("client_port=");
    sinkPort= strtoul(start, &end, 10);
    if (end == start || (*end != ';' && *end != '-')) {
        ERROR("get the client port failed:%s\n", data);
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }
    if (mRtpPort != sinkPort) {
        ERROR("mSinkPort:%d, sinkPort:%d\n", mRtpPort, sinkPort);
    }

    if (!response->getInt32(RTSP_MSG_CSEQ, cseq)) {
        WARNING("not find the cseq\n");
    }

    INFO("responseM6 cseq:%d, session:%d, client_port:%d\n", cseq, session, sinkPort);

    sendWfdM7();
    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleResponseM7( MediaMetaSP response )
{
    ENTER();
    const char *data= NULL;
    int number;
    //check the header
    if ((!response->getString(RTSP_VERSION, data))||(strcmp(data,RTSP_MSG_VERSION))) {
        ERROR("header has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getInt32(RTSP_STATUS, number))||(number != 200)) {
        ERROR("number has problem:%d\n", number);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getString(RTSP_REASON, data))||(strcmp(data,"OK"))) {
        ERROR("result has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    setupMediaPipeline();

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleResponseM8( MediaMetaSP response )
{
    ENTER();
    const char *data= NULL;
    int  number;
    //check the header
    if ((!response->getString(RTSP_VERSION, data))||(strcmp(data,RTSP_MSG_VERSION))) {
        ERROR("header has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getInt32(RTSP_STATUS, number))||(number != 200)) {
        ERROR("number has problem:%d\n", number);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getString(RTSP_REASON, data))||(strcmp(data,"OK"))) {
        ERROR("result has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    // TODO should handle tear down if source fail to response M8
    mRtspState = TEARDOWN;
    stopInternal();

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleResponseM9( MediaMetaSP response )
{
    ENTER();
    const char *data= NULL;
    int number;
    //check the header
    if ((!response->getString(RTSP_VERSION, data))||(strcmp(data,RTSP_MSG_VERSION))) {
        ERROR("header has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getInt32(RTSP_STATUS, number))||(number != 200)) {
        ERROR("number has problem:%d\n", number);
        EXIT_AND_RETURN( false );
    }

    if ((!response->getString(RTSP_REASON, data))||(strcmp(data,"OK"))) {
        ERROR("result has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    EXIT_AND_RETURN( true );
}

bool WfdSinkSession::handleResponseM13( MediaMetaSP response )
{
    ENTER();
    const char *data= NULL;
    int cseq;
    //check the header

    mRequestIDRPending = false;
    if ((!response->getString(RTSP_VERSION, data))||(strcmp(data,RTSP_MSG_VERSION))) {
        ERROR("header has problem:%s\n", data);
        EXIT_AND_RETURN( false );
    }

    //compare the cseq
    if (!response->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not find the cseq\n");
        EXIT_AND_RETURN( false );
    }

    EXIT_AND_RETURN( true );
}

#define ENSURE_PLAYER_OP_SUCCESS(OPERATION, RET, RET_SUCCESS, TIME) \
    do {                                                            \
        if(RET != MM_ERROR_SUCCESS && RET != MM_ERROR_ASYNC){           \
            ERROR("%s failed, ret=%d\n", OPERATION, RET);               \
            goto _exit;                                                 \
        }                                                               \
        TIME = 0;                                                       \
        while ( mState!= RET_SUCCESS) {                                 \
            usleep(10000);                                              \
            TIME += 10000;                                              \
            if (TIME > 100*1000*1000) {                                 \
                ERROR("%s timeout\n", OPERATION);                       \
                goto _exit;                                             \
            }                                                           \
        }                                                               \
    }while (0)


void WfdSinkSession::setupMediaPipeline()
{
    ENTER();

    mm_status_t status;

    INFO("now begin to setup the mediaplayer to play rtp stream\n");
    postMsg(WFD_MSG_REQUEST_IDR, 0, NULL, 0 * 1000000);

    //#0 set the surface
    WindowSurface *ws = static_cast<WindowSurface*>(mWindow);
    if (!ws) {
        ws = createSimpleSurface(1920, 1080);
        mWindow = ws;
        mOwnWindow = true;
    }
    if (!ws) {
        ERROR("init surface fail\n");
        EXIT();
    }
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    ws->setOffset(0, 100);
#else
    WINDOW_API(set_buffers_offset)(ws, 0, 100);
#endif

    //#1 create the mediaplayer
    INFO("now begin to create the player:%p\n", this);
    mPlayer = new CowPlayer();
    if ( !mPlayer ) {
        ERROR("create the player is error\n");
        if (mOwnWindow)
            destroySimpleSurface(ws);
        mWindow = NULL;
        EXIT();
    }

    /////////////////////////////////////////////////////////////
    status = mPlayer->setListener(this);
    if(status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC) {
        ERROR("setlistener failed: %d\n", status);
        mPlayer->setListener(NULL);
        mPlayer->reset();
        MM_RELEASE(mPlayer);
        if (mOwnWindow)
            destroySimpleSurface(ws);
        mWindow = NULL;
        EXIT();
    }

    /////////////////////////////////////////////////////////////
#ifndef __MM_YUNOS_LINUX_BSP_BUILD__
    status = mPlayer->setVideoSurface(ws);
    if(status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC) {
        ERROR("setdatasource failed: %d\n", status);
        mPlayer->setListener(NULL);
        mPlayer->reset();
        MM_RELEASE(mPlayer);
        if (mOwnWindow)
            destroySimpleSurface(ws);
        mWindow = NULL;
        EXIT();
    }
#endif

    ////////////////////////////////////////////////////////////
    std::map<std::string,std::string> header;
    char buf[64];
    //int timeOut;
    sprintf(buf, "rtp://%s:%d", mLocalIP.c_str(), mRtpPort);
    INFO("settting datasource:%s", buf);
    status = mPlayer->setDataSource(buf, &header);
    if(status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC){
        ERROR("setdatasource failed: %d,%s\n", status, buf);
        mPlayer->setListener(NULL);
        mPlayer->reset();
        MM_RELEASE(mPlayer);
        if (mOwnWindow)
            destroySimpleSurface(ws);
        mWindow = NULL;
        EXIT();
    }

    //////////////////////////////////////////////////////////
    MediaMetaSP meta;
    meta = MediaMeta::create();
    meta->setInt32(MEDIA_ATTR_WIDTH, mWidth);
    meta->setInt32(MEDIA_ATTR_HEIGHT, mHeight);
    meta->setInt32(MEDIA_ATTR_CODEC_LOW_DELAY, true);
    int percent = 80;
    meta->setInt32(MEDIA_ATTR_CODEC_DROP_ERROR_FRAME, percent);
    INFO("the width:%d, height:%d, codec_low_delay true, error frame drop percent %d\n",
         mWidth, mHeight, percent);
    status = mPlayer->setParameter(meta);
    if ((status != MM_ERROR_SUCCESS) && (status != MM_ERROR_ASYNC)) {
        ERROR("setParameter failed: %d\n", status);
        mPlayer->setListener(NULL);
        mPlayer->reset();
        MM_RELEASE(mPlayer);
        if (mOwnWindow)
            destroySimpleSurface(ws);
        mWindow = NULL;
        EXIT();
    }
    //////////////////////////////////////////////////////////
    INFO("now start to prepare the mediaplayer\n");
    status = mPlayer->prepareAsync();
    //ENSURE_PLAYER_OP_SUCCESS("prepare", status, PREPARED, timeOut);
    if(status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC){
        ERROR("prepareAsync failed: %d\n", status);
        mPlayer->setListener(NULL);
        mPlayer->reset();
        MM_RELEASE(mPlayer);
        if (mOwnWindow)
            destroySimpleSurface(ws);
        mWindow = NULL;
        EXIT();
    }

    INFO("now setup the mediaplayer is ok\n");

    EXIT();
#if 0
_exit:
    mPlayer->reset();
    MediaPlayer::destroy(mPlayer);
    destroySimpleSurface(ws);
    EXIT();
#endif
}

void WfdSinkSession::playMediaPipeline()
{
    ENTER();

    INFO("now start to start the mediaplayer\n");
    if ( mPlayer ) {
        mm_status_t  status = mPlayer->start();
        //ENSURE_PLAYER_OP_SUCCESS("start", status, STARTED, timeOut);
        if(status != MM_ERROR_SUCCESS && status != MM_ERROR_ASYNC){
           ERROR("setdatasource failed: %d\n", status);
           mPlayer->setListener(NULL);
           mPlayer->reset();
           MM_RELEASE(mPlayer);
           EXIT();
        }
    }

    EXIT();

}

void WfdSinkSession::stopMediaPipeline()
{
    ENTER();

    if (mPlayer) {
        mPlayer->stop();
    }

    EXIT();
}

void WfdSinkSession::teardownMediaPipeline()
{
    ENTER();

    if (mPlayer) {
        INFO("destroy player state:%d\n", mState);
        /*while(mState != STOPPED) {
            INFO("player state:%d\n", mState);
            usleep(1000*1000);
            continue;
        }*/
        usleep(1000*1000);
        mPlayer->setListener(NULL);
        mPlayer->reset();

        MM_RELEASE(mPlayer);
        mPlayer = NULL;
    }

    WindowSurface *ws = (WindowSurface *)mWindow;
    if (ws) {
        if (mOwnWindow)
            destroySimpleSurface(ws);
        mWindow = NULL;
    }

    EXIT();
}

mm_status_t WfdSinkSession::parseVideoFormat(const char* content)
{
    ENTER();
    mm_status_t ret = MM_ERROR_FATAL_ERROR;
    const char *p = NULL;

    if ((content[1] == '0') && (content[2] == '0')) {
        p = &content[13];
        ret = getCEAWidthHeight(mWidth, mHeight, p);
    }
    else if ((content[1] == '0') && (content[2] == '1')) {
        p = &content[22];
        ret = getVESAWidthHeight(mWidth, mHeight, p);
    }
    else if ((content[1] == '0') && (content[2] == '2')) {
        p = &content[31];
        ret = getHHWidthHeight(mWidth, mHeight, p);
    }
    else {
        ERROR("not support content:%s\n", content);
        EXIT_AND_RETURN(MM_ERROR_OP_FAILED);
    }

    EXIT_AND_RETURN(ret);
}

static const VideoConfigData CEAVideoInfo[18] =
{
    {640,480,60},   {720,480,60},  {720,480,30}, {720,576,50},
    {720,576,25},   {1280,720,30}, {1280,720,60}, {1920,1080,30},
    {1920,1080,60}, {1920,1080,30}, {1280,720,25}, {1280,720,50},
    {1920,1080,25}, {1920,1080,50}, {1920,1080,25}, {1280,720,24},
    {1920,1080,24}
};

mm_status_t WfdSinkSession::getCEAWidthHeight(int& width, int& height, const char* content)
{
    ENTER();
    int idx = 0, i;

    //#0 find idx
    for(i = 0; i < 8; i++) {
        if (content[i] != '0') {
            if (content[i] == '1')
                idx += 3;
            else if (content[i] == '2')
                idx += 2;
            else if (content[i] == '4')
                idx += 1;
            else if (content[i] == '8')
                idx += 0;
            else {
                ERROR("the CEA bitmap:%s is error\n", content);
                EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
            }
            break;
        } else {
            idx+=4;
        }
    }//for(i = 0; i < 8; i++) {

    idx = 32 - idx - 1;
    if ((i == 8)||(idx >= 18)) {
        ERROR("the CEA bitmap:%s is error,idx:%d\n", content, idx);
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    width = CEAVideoInfo[idx].width;
    height = CEAVideoInfo[idx].height;
    INFO("get the CEA width:%d, height:%d, fps:%d\n", width, height, CEAVideoInfo[idx].fps);

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdSinkSession::getVESAWidthHeight(int& width, int& height, const char* content)
{
    ENTER();
    EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
}

mm_status_t WfdSinkSession::getHHWidthHeight(int& width, int& height, const char* content)
{
    ENTER();
    EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
}

void WfdSinkSession::handleWfdRequestIDR(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId)
{
    ENTER();

    if (mRtspState >= ABOUT_TO_TEARDOWN) {
        INFO("rtsp is state %d, no need to request idr", mRtspState);
        EXIT();
    }
    INFO("get the idr message, state:%d, idr required:%d idr request pending %d\n",
         mRtspState, mRequestIDR, mRequestIDRPending);
    if (mRequestIDR && !mRequestIDRPending) {
        sendWfdM13();
        {
            MMAutoLock lock(mLock);
            mRequestIDR = false;
            mRequestIDRPending = true;
        }
    }

    postMsg(WFD_MSG_REQUEST_IDR, 0, NULL, REQUEST_IDR_PERIOD * 1000000);

    EXIT();
}

void WfdSinkSession::handlePlayerStart(param1_type msg, param2_type param1, param3_type param3, uint32_t rspId)
{
    ENTER();

    handleRtspPlay(MediaMetaSP((MediaMeta*)NULL));

    EXIT();
}

} // end of namespace YUNOS_MM
