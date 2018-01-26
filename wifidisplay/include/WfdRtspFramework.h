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

#ifndef __wfd_rtsp_framework_h
#define __wfd_rtsp_framework_h

#include "multimedia/mmthread.h"
#include "multimedia/mm_cpp_utils.h"
#include "multimedia/media_meta.h"
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>

#include <string.h>
#include <string>
#include <list>
#include <unistd.h>

namespace YUNOS_MM {

//#undef WFD_HAS_AUDIO
//#undef WFD_OUTPUT_RTP
#define WFD_OUTPUT_RTP
//#define WFD_HAS_AUDIO

#define NONBLOCK_TCP_ACCEPT
#define DEFAULT_RTSP_PORT 7236 // WFD 4.5.4
#define SESSION_TIMEOUT_SEC 30
#define CHECK_SESSION_ALIVE_PERIOD 20
#define RTSP_TEARDOWN_TRIGGER_TIMEOUT 2
#define REQUEST_IDR_PERIOD 1
class WfdRtspFramework : public MMThread {

public:
    WfdRtspFramework(const char* iface, bool server);

    mm_status_t wfdConnect();
    mm_status_t wfdDisConnect();

    enum wfd_message_type {
        /*
         * WFD rtsp has been connected between source and sink
         */
        WFD_MSG_CONNECTED,
        /*
         * WFD receives rtsp request
         * param1: rtsp cseq
         * obj: rtsp request msg
         */
        WFD_MSG_RTSP_REQUEST,
        /*
         * WFD receives rtsp response
         * param1: rtsp cseq
         * obj: rtsp response msg
         */
        WFD_MSG_RTSP_RESPONSE,
        /*
         * WFD rtsp connection has been teardown
         */
        WFD_MSG_TEARDOWN,
        /*
         * WFD rtsp connection has been lost
         */
        WFD_MSG_RTSP_DISCONNECTED,
    };

    class WfdRtspListener {
    public:
        WfdRtspListener() {}
        virtual ~WfdRtspListener() {}

        virtual void onRtspMessage(int msg, int param1, const MediaMetaSP metaSP) = 0;

        MM_DISALLOW_COPY(WfdRtspListener)
    };

    mm_status_t setListener(WfdRtspListener* listener);

    /*
     * @param meta: meta data for rtsp message
     *     WFD_RTSP_MSG_ID
     *     RTSP_MSG_TYPE
     *     RTSP_REQUEST_METHOD
     */
    mm_status_t sendRtspMessage(MediaMetaSP meta);

    /*
     * use RTSP_MSG_CONTENT to pass in rtsp text content
     */
    void parseWfdParameter(MediaMetaSP meta);

protected:
    virtual ~WfdRtspFramework();
    virtual void main();
    mm_status_t queueRtspMessage_locked(const void *data, ssize_t size);
    mm_status_t wfdRtspAccept();
    mm_status_t getClientIpPort();

private:
    // wfd
    virtual mm_status_t prepareM1(MediaMetaSP); // source
    virtual mm_status_t prepareM2(MediaMetaSP); // sink
    virtual mm_status_t prepareM3(MediaMetaSP); // source
    virtual mm_status_t prepareM4(MediaMetaSP); // source
    virtual mm_status_t prepareM5(MediaMetaSP); // source
    virtual mm_status_t prepareM6(MediaMetaSP); // sink
    virtual mm_status_t prepareM7(MediaMetaSP); // sink
    virtual mm_status_t prepareM8(MediaMetaSP); // sink
    virtual mm_status_t prepareM9(MediaMetaSP); // sink
    virtual mm_status_t prepareM10(MediaMetaSP); // sink
    virtual mm_status_t prepareM11(MediaMetaSP); // sink
    virtual mm_status_t prepareM12(MediaMetaSP); // source sink
    virtual mm_status_t prepareM13(MediaMetaSP); // sink
    virtual mm_status_t prepareM14(MediaMetaSP); // source sink
    virtual mm_status_t prepareM15(MediaMetaSP); // source sink
    virtual mm_status_t prepareM16(MediaMetaSP); // source
    virtual mm_status_t prepareResponse(MediaMetaSP); // source sink

    bool prepareResponseM3(MediaMetaSP, std::string &);
    bool prepareResponseM6(MediaMetaSP, std::string &);
    bool prepareResponseM7(MediaMetaSP, std::string &);
    bool prepareResponseM8(MediaMetaSP, std::string &);
    bool prepareResponseM13(MediaMetaSP, std::string &);

    void sendRtspResponseError(int status, const char* reason, int cseq);
    void appendCommonMessage(std::string &msg, int32_t cseq, int32_t sessionID = -1);

    // IO
    void interrupt();
    mm_status_t wfdRtspRead(int fd);
    mm_status_t wfdRtspWrite(int fd);

    // parser
    void parseRtspMessage(MediaMetaSP &meta);
    size_t parseNextWord(std::string &line,
                          size_t offset,
                          std::string &field);
    void parseStartLine(MediaMetaSP result, std::string &line);
    int stringToNumber(std::string &field);

private:

    bool mIsServer;
    bool mConnected;
    bool mInProgress;
    bool mShuttingDown;
    Lock mLock;
    bool mContinue;

    std::string mNetworkInterface;
    WfdRtspListener* mListener;

    std::list<std::string> mRtspMessages;
    std::string mRtspRecvString;

    int mInterruptFd[2];
    int mSocket;
    int mListeningSocket;

    int mCSeq;

    MM_DISALLOW_COPY(WfdRtspFramework)
    DECLARE_LOGTAG()
};

} // end of namespace YUNOS_MM
#endif //__wfd_rtsp_framework_h
