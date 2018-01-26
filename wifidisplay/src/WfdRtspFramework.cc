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

#include "WfdRtspFramework.h"
#include "WfdParameters.h"

#include "multimedia/mm_debug.h"

#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
//#include <linux/tcp.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <ifaddrs.h>

namespace YUNOS_MM {

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define META_NULL MediaMetaSP((MediaMeta*)NULL)

#define WFDSINK_CONNECT_RETRY_COUNT (10)
#define WFDSINK_CONNECT_RETRY_WAIT  (500000) // 500ms

WfdRtspFramework::WfdRtspFramework(const char* iface, bool server)
    : MMThread("WfdRtspFramework"),
      mIsServer(server),
      mConnected(false),
      mInProgress(false),
      mShuttingDown(false),
      mContinue(false),
      mNetworkInterface(iface),
      mListener(NULL),
      mSocket(-1),
      mListeningSocket(-1),
      mCSeq(server?1:10) {

    ENTER1();

    mInterruptFd[0] = -1;
    mInterruptFd[1] = -1;

    EXIT1();
}

WfdRtspFramework::~WfdRtspFramework() {
    ENTER1();

    EXIT1();
}

mm_status_t WfdRtspFramework::setListener(WfdRtspListener* listener) {
    ENTER1();

    MMAutoLock lock(mLock);
    mListener = listener;

    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::wfdConnect() {
    ENTER1();
    MMAutoLock lock(mLock);

    int ret, flags, localSocket = -1;
    int port;
    const int opt = 1;
    size_t pos;
    std::string iface;
    struct sockaddr_in addr;

    // create listening socket
    localSocket = socket( AF_INET, SOCK_STREAM, 0);

    if (localSocket < 0) {
        ERROR("fail to create rtsp listening socket");
        EXIT_AND_RETURN1(MM_ERROR_IO);
    }

    if ( mIsServer ) {
        // configure listening socket
        ret = setsockopt(localSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret < 0) {
            ERROR("setsockopt is failed,ret:%d\n", ret);
            goto bad;
        }
    }

#ifdef NONBLOCK_TCP_ACCEPT
    flags = fcntl(localSocket, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }

    ret = fcntl(localSocket, F_SETFL, flags | O_NONBLOCK);
    if (ret < 0) {
        goto bad;
    }
#endif
    // extract rtsp ip and port
    iface = mNetworkInterface;
    INFO("rtsp server bind to iface %s", iface.c_str());

    pos = iface.find(':');
    if (pos != std::string::npos) {
        char *end;
        const char *s = iface.c_str() + pos + 1;
        port = strtoul(s, &end, 10);
        if (end == s || *end != '\0' || port > 65535) {
            ERROR("parse the port failed;iface:%s, s:%s, end:%s, port:%d\n", iface.c_str(), s, end, port);
            goto bad;
        } else {
           iface.erase(pos);
        }
    } else {
        INFO("use WFD default control port %d", DEFAULT_RTSP_PORT);
        port = DEFAULT_RTSP_PORT;
    }

    // bind and listen for rtsp connection
    //INFO("sleep 6s waiting p2p IP becomes available");
    //usleep(6*1000*1000);
    INFO("bind to %s:%d and start listening", mIsServer ? "0.0.0.0" : iface.c_str(), port);
    memset(addr.sin_zero, 0, sizeof(addr.sin_zero));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (!mIsServer) {
        int count = 0;
        do {
            addr.sin_addr.s_addr = inet_addr(iface.c_str());
            in_addr_t x = ntohl(addr.sin_addr.s_addr);
            INFO("connecting socket %d to %d.%d.%d.%d:%d\n", localSocket,
                    (x >> 24), (x >> 16) & 0xff, (x >> 8) & 0xff, x & 0xff, ntohs(addr.sin_port));
            ret = connect(localSocket, (const struct sockaddr *)&addr, sizeof(addr));
            if (!ret || (errno == EINPROGRESS)) {
                if (errno == EINPROGRESS)
                    mInProgress = true;
                INFO("connect to rtsp server, ret %d, in progress %d", ret, mInProgress);
                break;
            }
            INFO("ret:%d, errno:%d, strerror(errno):%s\n", ret, errno, strerror(errno));
            usleep(WFDSINK_CONNECT_RETRY_WAIT);
        } while (count++ < WFDSINK_CONNECT_RETRY_COUNT);
        if (ret && errno != EINPROGRESS) {
           ERROR("fail to connect to rtsp server, errno %d, strerror(errno):%s", errno, strerror(errno));
           goto bad;
        }
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        ret = bind(localSocket, (const struct sockaddr *)&addr, sizeof(addr));
        if (ret) {
             ERROR("bind return error %d, %s", errno, strerror(-errno));
             goto bad;
        }

        ret = listen(localSocket, 4);
        if (ret) {
           ERROR("listen return error %d, %s", errno, strerror(-errno));
           goto bad;
        }
    }

    if (mIsServer)
        mListeningSocket = localSocket;
    else {
        mSocket = localSocket;
        if (getClientIpPort() != MM_ERROR_SUCCESS) {
            ERROR("get client ip port failed\n");
            goto bad;
        }
    }
    localSocket = -1;

#ifndef NONBLOCK_TCP_ACCEPT
    // accept rtsp connection
    if (mIsServer && (wfdRtspAccept() != MM_ERROR_SUCCESS)) {
        ERROR("fail to accept client");
        goto bad;
    }
#endif

    ret = pipe(mInterruptFd);
    if (ret != 0) {
        ERROR("fail to create interrupt fd pairs");
        mInterruptFd[0] = mInterruptFd[1] = -1;
        goto bad;
    }

    INFO("create interrupt fd pairs %d %d", mInterruptFd[0], mInterruptFd[1]);

    mContinue = true;
    if (!mIsServer)
        mConnected = true;
    // start thread
    create();

#ifdef NONBLOCK_TCP_ACCEPT
    EXIT_AND_RETURN1(MM_ERROR_ASYNC);
#else
    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
#endif

bad:
    if (mListeningSocket > 0)
        close(mListeningSocket);
    mListeningSocket = -1;

    if (mSocket > 0)
        close(mSocket);
    mSocket = -1;

    if (localSocket > 0)
        close(localSocket);
    EXIT_AND_RETURN1(MM_ERROR_IO);
}

mm_status_t WfdRtspFramework::wfdRtspAccept() {
    ENTER1();

    int ret, flags;
    struct sockaddr_in addr;
    struct sockaddr_in remoteAddr;
    in_addr_t number;
    socklen_t addrLen = sizeof(remoteAddr);
    std::string remoteIP, localIP;
    char buf[24];

    INFO("listen and wait for rtsp connection");

    if (!mIsServer) {
        ERROR("rtsp client should not accept");
        goto error;
    }

    mSocket = accept(
            mListeningSocket, (struct sockaddr *)&remoteAddr, &addrLen);

    if (mSocket < 0) {
        ERROR("accept return error %d, %s", errno, strerror(-errno));
        goto error;
    }

    number = ntohl(remoteAddr.sin_addr.s_addr);
    INFO("get socket (%d) for incoming rtsp connection from %d.%d.%d.%d:%d",
         mSocket,
         number >> 24,
         (number >> 16) & 0xff,
         (number >> 8) & 0xff,
         number & 0xff,
         ntohs(remoteAddr.sin_port));

    flags = fcntl(mSocket, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }

    ret = fcntl(mSocket, F_SETFL, flags | O_NONBLOCK);
    if (ret < 0) {
        close(mSocket);
        mSocket = -1;
        goto error;
    }

    ret = getsockname(
            mSocket, (struct sockaddr *)&addr, &addrLen);

    if (ret) {
        ERROR("getsockname error %d, %s", errno, strerror(-errno));
    } else {

        number = ntohl(addr.sin_addr.s_addr);
        sprintf(buf,
            "%d.%d.%d.%d",
            (number >> 24),
            (number >> 16) & 0xff,
            (number >> 8) & 0xff,
            number & 0xff);
    }
    INFO("local address is %s", buf);
    localIP.append( buf );

    number = ntohl(remoteAddr.sin_addr.s_addr);
    sprintf(buf,
        "%d.%d.%d.%d",
        (number >> 24),
        (number >> 16) & 0xff,
        (number >> 8) & 0xff,
        number & 0xff);

    INFO("client address is %s", buf);
    remoteIP.append( buf );

    mConnected = true;

    if (mListener) {
        MediaMetaSP meta = MediaMeta::create();
        //if it is source device, now WfdSession has get local(source) ip or remote(sink) ip
        meta->setString(WFD_RTSP_REMOTE_IP, remoteIP.c_str());
        meta->setString(WFD_RTSP_LOCAL_IP, localIP.c_str());
        mListener->onRtspMessage(WFD_MSG_CONNECTED, 0, meta);
    }

    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);

error:
    close(mListeningSocket);
    mListeningSocket = -1;
    EXIT_AND_RETURN1(MM_ERROR_IO);
}

mm_status_t WfdRtspFramework::wfdDisConnect() {
    ENTER1();

    {
        MMAutoLock lock(mLock);
        mContinue = false;
    }

    interrupt();
    destroy();

    {
        MMAutoLock lock(mLock);
        if (mSocket > 0)
            close(mSocket);
        mSocket = -1;
        if (mIsServer > 0)
            close(mListeningSocket);
        mListeningSocket = 0;

        close(mInterruptFd[0]);
        close(mInterruptFd[1]);
    }

    delete this;

    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::queueRtspMessage_locked(const void *data, ssize_t size) {
    ENTER1();

    std::string msg;

    msg.assign((const char*)data, size);
    mRtspMessages.push_back(msg);

    interrupt();

    EXIT_AND_RETURN1(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::sendRtspMessage(MediaMetaSP meta) {
    ENTER1();

    const char *value = NULL;
    mm_status_t status = MM_ERROR_INVALID_PARAM;

    if (!meta)
        EXIT_AND_RETURN1(status);

    MMAutoLock lock(mLock);
    if (!mConnected)
        EXIT_AND_RETURN1(MM_ERROR_NOT_INITED);

    if (meta->getString(RTSP_MSG_TYPE, value) &&
        !strcmp(value, RTSP_MSG_TYPE_RESPONSE)) {
        status = prepareResponse(meta);
    } else if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(value, WFD_RTSP_MSG_M1)) {
        status = prepareM1(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M2)) {
       status = prepareM2(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M3)) {
       status = prepareM3(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M4)) {
       status = prepareM4(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M5)) {
       status = prepareM5(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M16)) {
       status = prepareM16(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M6)) {
       status = prepareM6(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M7)) {
       status = prepareM7(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M8)) {
       status = prepareM8(meta);
    } else if (!strcmp(value, WFD_RTSP_MSG_M13)) {
       status = prepareM13(meta);
    }
    if (status != MM_ERROR_SUCCESS) {
        ERROR("fail to prepare msg for %s", value);
        EXIT_AND_RETURN1(status);
    }

    if (meta->getString(RTSP_RAW_MSG, value) && value) {
        status = queueRtspMessage_locked(value, strlen(value));
    } else {
        ERROR("prepare rtsp message %s fail", value);
    }

    EXIT_AND_RETURN1(status);
}

void WfdRtspFramework::parseWfdParameter(MediaMetaSP meta) {
    ENTER();
    const char* data = NULL;

    if (!meta || !meta->getString(RTSP_MSG_CONTENT, data) || !data) {
        ERROR("media meta(%p) contains no rtsp msg data", meta.get());
        EXIT1();
    }

    size_t offset = 0, offsetLineEnd, lineOffset;
    size_t size = strlen(data);

    while (offset < size) {

        offsetLineEnd = offset;

        while (offsetLineEnd < size &&
               (data[offsetLineEnd] != '\r' || data[offsetLineEnd + 1] != '\n'))
            offsetLineEnd++;

        if (offsetLineEnd >= size)
            EXIT();

        std::string line;
        line.assign(&data[offset], offsetLineEnd - offset);

        lineOffset = line.find(":");
        if (lineOffset != std::string::npos) {
            std::string header, field;
            const char* lineData = line.c_str();

            header.assign(&lineData[0], lineOffset);
            lineOffset++;

            while (lineOffset < line.size() && lineData[lineOffset] == ' ')
                lineOffset++;

            field.assign(&lineData[lineOffset]);

            meta->setString(header.c_str(), field.c_str());
            INFO("parse content attribute %s and value is\n%s",
                  header.c_str(), field.c_str());
        }

        offset = offsetLineEnd + 2; // \r\n
    }

    EXIT();
}

void WfdRtspFramework::interrupt() {

    char dummy = 0;

    ssize_t n;
    do {
        n = write(mInterruptFd[1], &dummy, 1);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        ERROR("Error writing to pipe (%s)", strerror(errno));
    }
}

void WfdRtspFramework::main()
{
    ENTER1();

    fd_set rs, ws;
    int ret;
    int maxFd;
    mm_status_t status;

    if (mIsServer)
        maxFd = mInterruptFd[0];
    //else
    //    mSocket = mListeningSocket;

restart:
    if (!mContinue) {
        WARNING("rtsp looper exit, connected %d continue %d", mConnected, mContinue);
        if (!mIsServer)
            mListeningSocket = mSocket;
        EXIT1();
    }
    if (!mIsServer) {
        /*if (!mConnected) {
            if (wfdConnect() != MM_ERROR_SUCCESS) {
                ERROR("resocket to server failed!\n");
                EXIT1();
            }
            mConnected = true;
            mSocket = mListeningSocket;
        }*/
        maxFd = mInterruptFd[0]>mSocket? mInterruptFd[0]:mSocket;
    }

    FD_ZERO(&rs);
    FD_ZERO(&ws);

    FD_SET(mInterruptFd[0], &rs);

    {
        MMAutoLock lock(mLock);

        if (mConnected)
            FD_SET(mSocket, &rs);
        else if (mIsServer)
            FD_SET(mListeningSocket, &rs);

        if (mConnected && !mRtspMessages.empty())
            FD_SET(mSocket, &ws);
    }

    ret = select(maxFd + 1, &rs, &ws, NULL, NULL);
    if (ret < 0 && mInProgress) {
        ERROR("wfd sink connection in progress with error ret %d %s", ret, strerror(errno));
        if (mListener)
            mListener->onRtspMessage(WFD_MSG_RTSP_DISCONNECTED, 0, META_NULL);
        usleep(1000000);
        goto restart;
    }

    if (ret >= 0) {
        if (!mIsServer && mInProgress) {
            INFO("connected to rtsp server, ret %d", ret);
            MMAutoLock lock(mLock);
            mInProgress = false;
            getClientIpPort();
        }

        if (ret == 0) {
            WARNING("zero selected fd, should be connection success");
            goto restart;
        }
    }

    if (mIsServer) {
        if (FD_ISSET(mListeningSocket, &rs)) {
            if (wfdRtspAccept() != MM_ERROR_SUCCESS)
                ERROR("fail to accept client w/ status");

            maxFd = mSocket;
        }
    }

    if (FD_ISSET(mInterruptFd[0], &rs)) {
        char tmp;
        ssize_t n;

        do {
            n = read(mInterruptFd[0], &tmp, 1);
        } while(n < 0 && errno == EINTR);

        if (n < 0)
            WARNING("fail to read interrupt socket %d w/ err %s",
                    mInterruptFd[0], strerror(errno));
    }

    if (mConnected && FD_ISSET(mSocket, &rs)) {
        status = wfdRtspRead(mSocket);
        if (status != MM_ERROR_SUCCESS) {
            ERROR("read socket(%d) failed w/ status %d", status);

            mConnected = false;

            if (mListener)
                mListener->onRtspMessage(WFD_MSG_RTSP_DISCONNECTED, 0, META_NULL);
        }
    }

    if (mConnected && FD_ISSET(mSocket, &ws)) {
        status = wfdRtspWrite(mSocket);
        if (status != MM_ERROR_SUCCESS) {
            ERROR("write socket(%d) failed w/ status %d", status);

            mConnected = false;

            if (mListener)
                mListener->onRtspMessage(WFD_MSG_RTSP_DISCONNECTED, 0, META_NULL);
        }
    }

goto restart;
}

mm_status_t WfdRtspFramework::wfdRtspRead(int fd) {
    ENTER();
    MMAutoLock lock(mLock);

    ssize_t n;
    char buf[512];

    do {
        n = recv(fd, buf, sizeof(buf), 0);
    } while (n < 0 && errno == EINTR);

    if (n <= 0) {
        ERROR("socket (%d) recv error, ret %d, err is %s", fd, n, strerror(errno));
        EXIT_AND_RETURN(MM_ERROR_IO);
    }

    mRtspRecvString.append(buf, n);
    if (!mListener) {
        WARNING("rtsp has no listener, no need to parse incoming data");
        EXIT_AND_RETURN(MM_ERROR_SUCCESS);
    }

    INFO("read rtsp msg, size is %d, msg:\n%s", n, mRtspRecvString.c_str());

    if (mRtspRecvString.size() > 0 && mRtspRecvString.c_str()[0] == '$') {
        ERROR("should not be here, size is %d:\n%s",
              mRtspRecvString.size(), mRtspRecvString.c_str());
    }

    MediaMetaSP meta;
    const char* value;
    int type = 0;

    do {
        meta.reset();
        parseRtspMessage(meta);

        value = NULL;
        if (meta && meta->getString(RTSP_MSG_TYPE, value)) {
            if (!strcmp(value, RTSP_MSG_TYPE_REQUEST))
                type = WFD_MSG_RTSP_REQUEST;
            else if (!strcmp(value, RTSP_MSG_TYPE_RESPONSE))
                type = WFD_MSG_RTSP_RESPONSE;

            if (type == WFD_MSG_RTSP_REQUEST ||
                type == WFD_MSG_RTSP_RESPONSE)
                mListener->onRtspMessage(type, 0, meta);
            else
                WARNING("Neither request nor response msg");
        } else
                WARNING("meta(%p) rtsp msg type is not specified", meta.get());

    } while (mRtspRecvString.size() > 3 && meta);


    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::wfdRtspWrite(int fd) {
    ENTER();
    MMAutoLock lock(mLock);

    if (mRtspMessages.empty()) {
        WARNING("rtsp has nothing to write, should not be here");
        EXIT_AND_RETURN(MM_ERROR_SUCCESS);
    }

    while (!mRtspMessages.empty()) {
        ssize_t n;
        std::string msg = mRtspMessages.front();
        INFO("about to write rtsp msg, size is %d, msg:\n%s",
             msg.size(), msg.c_str());

        do {
            n = send(fd, msg.data(), msg.size(), 0);
        } while (n < 0 && errno == EINTR);

        if (n <= 0) {
            ERROR("socket (%d) send error, ret %d, err is %s", fd, n, strerror(errno));
            EXIT_AND_RETURN(MM_ERROR_IO);
        }

        if (n < (ssize_t)msg.size()) {
            INFO("partially write msg (%d/%d)", n, msg.size());
            msg.erase(msg.begin(), msg.begin() + n);
            break;
        }

        mRtspMessages.pop_front();
    }

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

const char * WfdRtspFramework::MM_LOG_TAG = "WfdRtsp";

#define RTSP_MSG_BEGIN()                            \
    if (!meta)                                      \
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);   \
    std::string msg;                                \
    const char* value;

#define META_TO_RTSP_MSG(key)                       \
    if (meta->getString(key, value))                \
        msg.append(value);                          \
    else {                                          \
        ERROR("fail to get key %s", key);           \
        EXIT_AND_RETURN1(MM_ERROR_INVALID_PARAM);   \
    }

#define RTSP_MSG_END()                             \
    meta->setInt32(RTSP_MSG_CSEQ, mCSeq);          \
    meta->setString(RTSP_RAW_MSG, msg.c_str());    \
    mCSeq++;

mm_status_t WfdRtspFramework::prepareM1(MediaMetaSP meta) {
    ENTER();
    //source -> sink
    RTSP_MSG_BEGIN()

    META_TO_RTSP_MSG(RTSP_REQUEST_METHOD)

    msg.append(" * ");
    msg.append(RTSP_MSG_VERSION);
    msg.append("\r\n");

    appendCommonMessage(msg, mCSeq);

    msg.append(RTSP_MSG_REQUIRE);
    msg.append("\r\n");

    RTSP_MSG_END();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM2(MediaMetaSP meta) {
    ENTER();
    return prepareM1( meta );
}

mm_status_t WfdRtspFramework::prepareM3(MediaMetaSP meta) {
    ENTER();

    char buf[128];

    RTSP_MSG_BEGIN()
    META_TO_RTSP_MSG(RTSP_REQUEST_METHOD)

    msg.append(" rtsp://localhost/wfd1.0 ");
    msg.append(RTSP_MSG_VERSION);
    msg.append("\r\n");

    appendCommonMessage(msg, mCSeq);

    std::string body =
        "wfd_content_protection\r\n"
        "wfd_video_formats\r\n"
        "wfd_audio_codecs\r\n"
        "wfd_client_rtp_ports\r\n";

    msg.append("Content-Type: text/parameters\r\n");
    sprintf(buf, "Content-Length: %d\r\n", body.size());
    msg.append(buf);
    msg.append("\r\n");
    msg.append(body);

    RTSP_MSG_END();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM4(MediaMetaSP meta) {
    ENTER();

    char buf[128];
    const char *data = NULL, *ip = NULL;
    std::string body;

    RTSP_MSG_BEGIN()
    META_TO_RTSP_MSG(RTSP_REQUEST_METHOD)
    msg.append(" rtsp://localhost/wfd1.0 ");
    msg.append(RTSP_MSG_VERSION);
    msg.append("\r\n");

    appendCommonMessage(msg, mCSeq);

    if (meta->getString(WFD_RTSP_VIDEO_FORMATS, data) && strcmp("none", data)) {
        body.append(WFD_RTSP_VIDEO_FORMATS);
        body.append(": ");
        // hard code to CBP L3.1 1280x720 p60
        body.append("00 00 01 01 00000040 00000000 00000000 00 0000 0000 00 none none");
        body.append("\r\n");
    }

#ifdef WFD_HAS_AUDIO
    if (meta->getString(WFD_RTSP_AUDIO_CODECS, data) && strcmp("none", data)) {
        body.append(WFD_RTSP_AUDIO_CODECS);
        body.append(": ");
        //body.append("LPCM 00000002 00"); // hard code to PCM 2ch 48kHz
        body.append("AAC 00000001 00");
        body.append("\r\n");
    }
#endif
    if (!meta->getString(WFD_RTSP_LOCAL_IP, ip)) {
        ERROR("get the localIP: failed\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    sprintf(buf, "%s: rtsp://%s/wfd1.0/streamid=0 none\r\n", WFD_RTSP_PRESENTION_URL, ip);
    body.append(buf);

    if (meta->getString(WFD_RTSP_CLIENT_RTP_PORTS, data) && data) {
        body.append(WFD_RTSP_CLIENT_RTP_PORTS);
        body.append(": ");
        body.append(data);
        body.append("\r\n");
    }

    msg.append("Content-Type: text/parameters\r\n");
    sprintf(buf, "Content-Length: %d\r\n", body.size());
    msg.append(buf);

    msg.append("\r\n");

    msg.append(body);

    RTSP_MSG_END();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM5(MediaMetaSP meta) {
    ENTER();

    char buf[128];
    const char *data = NULL;

    RTSP_MSG_BEGIN()
    META_TO_RTSP_MSG(RTSP_REQUEST_METHOD)

    msg.append(" rtsp://localhost/wfd1.0 ");
    msg.append(RTSP_MSG_VERSION);
    msg.append("\r\n");

    appendCommonMessage(msg, mCSeq);
    msg.append("Content-Type: text/parameters\r\n");

    std::string body;

    body.append(WFD_RTSP_TRIGGER_METHOD);
    body.append(": ");

    // just sanity check
    if (!meta->getString(WFD_RTSP_TRIGGER_METHOD, data) ||
        !data) {
        WARNING("WFD_RTSP_TRIGGER_METHOD is not set  %s, assume SETUP", data);
        data = RTSP_REQUEST_METHOD_SETUP;
    }

    body.append(data);
    body.append("\r\n");

    sprintf(buf, "Content-Length: %d\r\n", body.size());
    msg.append(buf);
    msg.append("\r\n");
    msg.append(body);

    RTSP_MSG_END();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM6(MediaMetaSP meta) {
    ENTER();
    char buf[128];
    const char *ip = NULL;
    int port;
    std::string msg;

    if(!meta->getString(WFD_RTSP_REMOTE_IP, ip)) {
        ERROR("not find ip\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    if(!meta->getInt32(LOCAL_RTP_PORT, port)) {
        ERROR("not find port\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    //sprintf(buf, "SETUP rtsp://%s/wfd1.0/streamid=0 RTSP/1.0\r\n",ip);
    sprintf(buf, "%s rtsp://%s/wfd1.0/streamid=0 %s\r\n", RTSP_REQUEST_METHOD_SETUP, ip, RTSP_MSG_VERSION);
    msg.append( buf );

    appendCommonMessage(msg, mCSeq);

    //sprintf(buf, "Transport: RTP/AVP/UDP;unicast;client_port=%d\r\n", port);
    sprintf(buf, "%s: RTP/AVP/UDP;unicast;client_port=%d\r\n",
                 RTSP_MSG_TRANSPORT, port);
    msg.append( buf );
    msg.append("\r\n");

    meta->setInt32(RTSP_MSG_CSEQ, mCSeq++);
    meta->setString(RTSP_RAW_MSG, msg.c_str());

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM7(MediaMetaSP meta) {
    ENTER();
    char buf[128];
    const char  *ip = NULL;
    int session;
    std::string msg;

    if (!meta->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("not find session\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }
    if (!meta->getString(WFD_RTSP_REMOTE_IP, ip)) {
        ERROR("not find ip\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    //sprintf(buf, "PLAY rtsp://%s/wfd1.0/streamid=0 RTSP/1.0\r\n", ip);
    sprintf(buf, "%s rtsp://%s/wfd1.0/streamid=0 %s\r\n",
                 RTSP_REQUEST_METHOD_PLAY,
                 ip, RTSP_MSG_VERSION);
    msg.append( buf );

    appendCommonMessage(msg, mCSeq);

    sprintf(buf, "Session: %d\r\n", session);
    msg.append( buf );
    msg.append("\r\n");

    meta->setInt32(RTSP_MSG_CSEQ, mCSeq++);
    meta->setString(RTSP_RAW_MSG, msg.c_str());

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM8(MediaMetaSP meta) {
    ENTER();
    char buf[128];
    const char *ip = NULL;
    int session;
    std::string msg;

    if (!meta->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("not find session\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }
    if (!meta->getString(WFD_RTSP_REMOTE_IP, ip)) {
        ERROR("not find ip\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    //sprintf(buf, "TEARDOWN rtsp://%s/wfd1.0/streamid=0 RTSP/1.0\r\n", ip);
    sprintf(buf, "%s rtsp://%s/wfd1.0/streamid=0 %s\r\n",
                 RTSP_REQUEST_METHOD_TEARDOWN,
                 ip, RTSP_MSG_VERSION);
    msg.append( buf );

    appendCommonMessage(msg, mCSeq);

    sprintf(buf, "Session: %d\r\n", session);
    msg.append( buf );
    msg.append("\r\n");

    meta->setInt32(RTSP_MSG_CSEQ, mCSeq++);
    meta->setString(RTSP_RAW_MSG, msg.c_str());

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM9(MediaMetaSP meta) {
    ENTER();
    char buf[128];
    const char *ip = NULL;
    int session;
    std::string msg;

    if (!meta->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("not find session\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }
    if (!meta->getString(WFD_RTSP_REMOTE_IP, ip)) {
        ERROR("not find ip\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    //sprintf(buf, "PAUSE rtsp://%s/wfd1.0/streamid=0 RTSP/1.0\r\n", ip);
    sprintf(buf, "%s rtsp://%s/wfd1.0/streamid=0 %s\r\n",
                 RTSP_REQUEST_METHOD_PAUSE,
                 ip, RTSP_MSG_VERSION);
    msg.append( buf );

    appendCommonMessage(msg, mCSeq);

    sprintf(buf, "Session: %d\r\n", session);
    msg.append( buf );
    msg.append("\r\n");

    meta->setInt32(RTSP_MSG_CSEQ, mCSeq++);
    meta->setString(RTSP_RAW_MSG, msg.c_str());

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM10(MediaMetaSP meta) {
    ENTER();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM11(MediaMetaSP meta) {
    ENTER();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM12(MediaMetaSP meta) {
    ENTER();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM13(MediaMetaSP meta) {
    ENTER();
    char buf[128];
    const char *ip = NULL;
    int session;
    std::string msg;

    if (!meta->getInt32(RTSP_MSG_SESSION, session)) {
        ERROR("not find session\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }
    if (!meta->getString(WFD_RTSP_REMOTE_IP, ip)) {
        ERROR("not find ip\n");
        EXIT_AND_RETURN(MM_ERROR_INVALID_PARAM);
    }

    //sprintf(buf, "SET_PARAMETER rtsp://%s/wfd1.0/streamid=0 RTSP/1.0\r\n", ip);
    sprintf(buf, "%s rtsp://%s/wfd1.0/streamid=0 %s\r\n",
                 RTSP_REQUEST_METHOD_SETPARAMETER,
                 ip, RTSP_MSG_VERSION);
    msg.append( buf );

    //sprintf(buf, "CSeq: %d\r\n", mCSeq++);
    sprintf(buf, "%s: %d\r\n", RTSP_MSG_CSEQ, mCSeq);
    msg.append( buf );

    //sprintf(buf, "Session: %d\r\n", session);
    sprintf(buf, "%s: %d\r\n", RTSP_MSG_SESSION, session);
    msg.append( buf );

    sprintf(buf, "User-Agent: %s\r\n",RTSP_MSG_USER_AGENT);
    msg.append( buf );

    msg.append("Content-Type: text/parameters\r\n");

    sprintf(buf, "%s: 17\r\n", RTSP_MSG_CONTENT_LENGTH);
    msg.append( buf );
    msg.append("\r\n");

    sprintf(buf, "%s\r\n", WFD_RTSP_IDR_REQUEST);
    msg.append( buf );

    meta->setInt32(RTSP_MSG_CSEQ, mCSeq++);
    meta->setString(RTSP_RAW_MSG, msg.c_str());

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM14(MediaMetaSP meta) {
    ENTER();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM15(MediaMetaSP meta) {
    ENTER();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareM16(MediaMetaSP meta) {
    ENTER();

    char buf[32];
    int session = 0;

    RTSP_MSG_BEGIN()
    META_TO_RTSP_MSG(RTSP_REQUEST_METHOD)

    msg.append(" rtsp://localhost/wfd1.0 ");
    msg.append(RTSP_MSG_VERSION);
    msg.append("\r\n");

    appendCommonMessage(msg, mCSeq);

    meta->getInt32(RTSP_MSG_SESSION, session);
    sprintf(buf, "Session: %d\r\n", session);
    msg.append(buf);

    msg.append("\r\n");

    RTSP_MSG_END();

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

mm_status_t WfdRtspFramework::prepareResponse(MediaMetaSP meta) {
    ENTER();

    std::string msg;
    const char* value = NULL;
    int cseq;
    int s;
    mm_status_t status = MM_ERROR_OP_FAILED;

    if (!meta)
        EXIT_AND_RETURN1(status);

    if (!meta->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("response msg: has no request CSeq");
        EXIT_AND_RETURN1(status);
    }

    if (meta->getInt32(RTSP_STATUS, s) && s != 200) {
        // fail to create media recorder
        ERROR("rtsp send response with error status %d", s);
        sendRtspResponseError(s, RTSP_REASON_PREPROCESS_FAIL, cseq);
        EXIT_AND_RETURN1(status);
    }

    msg.append("RTSP/1.0 200 OK\r\n");

    if (meta->getString(RTSP_REQUEST_METHOD, value))
        INFO("prepare response message for in coming request method %s", value);

    // M1 response
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M1, value)) {
        appendCommonMessage(msg, cseq); //cseq = mSourceSeq
        msg.append( "Public: org.wfa.wfd1.0, GET_PARAMETER, SET_PARAMETER\r\n" );
        msg.append("\r\n");
    }

    // M2 response
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M2, value)) {
        appendCommonMessage(msg, cseq);//cseq = mSinkCSeq
        msg.append(
            "Public: org.wfa.wfd1.0, SETUP, TEARDOWN, PLAY, PAUSE, "
            "GET_PARAMETER, SET_PARAMETER\r\n");
        msg.append("\r\n");
    }

    // M3 response
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M3, value)) {
        if (!prepareResponseM3(meta, msg)) {
            ERROR("bad SETUP request");
            sendRtspResponseError(400, RTSP_REASON_BAD_REQUEST, cseq);
            EXIT_AND_RETURN1(status);
        }
    }

    //M4 M5 response
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        ((!strcmp(WFD_RTSP_MSG_M4, value)) ||
         (!strcmp(WFD_RTSP_MSG_M5, value)))) {
        appendCommonMessage(msg, cseq);
        msg.append("\r\n");
    }

    // M6 response
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M6, value)) {

        if (!prepareResponseM6(meta, msg)) {
            ERROR("bad SETUP request");
            sendRtspResponseError(400, RTSP_REASON_BAD_REQUEST, cseq);
            EXIT_AND_RETURN1(status);
        }
    }

    // M7 response
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M7, value)) {

        if (!prepareResponseM7(meta, msg)) {
            ERROR("bad PLAY request");
            sendRtspResponseError(400, RTSP_REASON_BAD_REQUEST, cseq);
            EXIT_AND_RETURN1(status);
        }
    }

    // M8 response
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M8, value)) {

        if (!prepareResponseM8(meta, msg)) {
            ERROR("bad TEARDOWN request");
            sendRtspResponseError(400, RTSP_REASON_BAD_REQUEST, cseq);
            EXIT_AND_RETURN1(status);
        }
    }

    // M13 response - request idr
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M13, value)) {

        if (!prepareResponseM13(meta, msg)) {
            ERROR("bad SET_PARAMETER request");
            sendRtspResponseError(400, RTSP_REASON_BAD_REQUEST, cseq);
            EXIT_AND_RETURN1(status);
        }
    }

    // M16 response keep alive
    if (meta->getString(WFD_RTSP_MSG_ID, value) &&
        !strcmp(WFD_RTSP_MSG_M16, value)) {
        appendCommonMessage(msg, cseq);//cseq
        msg.append("\r\n");
    }
    meta->setString(RTSP_RAW_MSG, msg.c_str());

    status = MM_ERROR_SUCCESS;

    EXIT_AND_RETURN1(status);
}

bool WfdRtspFramework::prepareResponseM3(MediaMetaSP meta, std::string &msg)
{
    std::string body;
    char buf[256];
    int parameterFlag = 0, cseq, sinkPort;

    if( !meta->getInt32(WFD_RTSP_PARAMETER_FLAG, parameterFlag)) {
        ERROR("not found the parameterFlag\n");
        EXIT_AND_RETURN(false);
    }

    if( !meta->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("not found the cseq\n");
        EXIT_AND_RETURN(false);
    }

    if( !meta->getInt32(LOCAL_RTP_PORT, sinkPort)) {
        ERROR("not found the sinkPort\n");
        EXIT_AND_RETURN(false);
    }

    if (parameterFlag & WFD_VIDEO_FORMAT_FLAG) {
        //sprintf(buf,"%s: 00 00 01 01 00000040 00000000 00000000 00 0000 0000 00 none none\r\n", WFD_RTSP_VIDEO_FORMATS);
        /*
         * native 40 1080p 60fps
         * preferred-display-mode-supported 00
         * H.264 codec, H.264 codec
         */
        if (mm_check_env_str("wfd.video.quality", NULL, "low", false))
            sprintf(buf,"%s: 40 00 02 04 0000002F 00000000 00000000 00 0000 0000 11 none none, 01 04 0000002F 00000000 00000000 00 0000 0000 11 none none\r\n", WFD_RTSP_VIDEO_FORMATS);
        else
            sprintf(buf,"%s: 40 00 02 04 0001FFFF 1FFFFFFF 00000FFF 00 0000 0000 11 none none, 01 04 0001FFFF 1FFFFFFF 00000FFF 00 0000 0000 11 none none\r\n", WFD_RTSP_VIDEO_FORMATS);
        body.append( buf );
    }

    if (parameterFlag & WFD_AUDIO_CODEC_FLAG) {
        if (mm_check_env_str("wfd.rtsp.audio", NULL, "aac", true))
            sprintf(buf, "%s: AAC 00000001 00\r\n", WFD_RTSP_AUDIO_CODECS);
        else if (mm_check_env_str("wfd.rtsp.audio", NULL, "lpcm", false))
            sprintf(buf, "%s: LPCM 00000003 00, AAC 00000001 00\r\n", WFD_RTSP_AUDIO_CODECS);
        else if (mm_check_env_str("wfd.rtsp.audio", NULL, "none", true))
            sprintf(buf, "%s: none\r\n", WFD_RTSP_AUDIO_CODECS);
        body.append( buf );
    }

    if (parameterFlag & WFD_3D_VIDEO_FORMAT_FLAG)
        body.append("wfd_3d_video_formats: none\r\n");
    if (parameterFlag & WFD_CONTENT_PROTECTION_FLAG) {
        sprintf(buf, "%s: none\r\n", WFD_RTSP_CONTENT_PROTECTION);
        body.append( buf );
    }
    if (parameterFlag & WFD_DISPLAY_EDID_FLAG)
        body.append("wfd_display_edid: none\r\n");
    if (parameterFlag & WFD_COUPLED_SINK_FLAG)
        body.append("wfd_coupled_sink: none\r\n");
    if (parameterFlag & WFD_CLIENT_RTP_PORTS_FLAG) {
        sprintf(buf, "%s: RTP/AVP/UDP;unicast %d 0 mode=play\r\n", WFD_RTSP_CLIENT_RTP_PORTS, sinkPort);
        body.append(buf);
    }

    if (parameterFlag & WFD_UIBC_CAPABILITY_FLAG) {
        body.append("wfd_uibc_capability: wfd_uibc_capability: input_category_list=GENERIC;generic_cap_list=Keyboard, Mouse, SingleTouch, MultiTouch;hidc_cap_list=none;port=none\r\n");
    }
    //sprintf(buf, "CSeq: %d\r\nContent-Length: %d\r\nContent-Type: text/parameters\r\n\r\n", cseq, body.size());
    sprintf(buf, "%s: %d\r\n%s: %d\r\nContent-Type: text/parameters\r\n\r\n",
                RTSP_MSG_CSEQ, cseq,  RTSP_MSG_CONTENT_LENGTH,  body.size());

    msg.append(buf);
    msg.append(body.c_str());//write the end flags
    INFO("responseM3:%s,size:%d\n", msg.c_str(), msg.size());

    EXIT_AND_RETURN(true);
}

bool WfdRtspFramework::prepareResponseM6(MediaMetaSP meta, std::string &msg) {
    ENTER();

    int32_t sessionID = -1;
    const char* transport = NULL;
    std::string temp;
    size_t offset;
    const char* clientPort;
    int cseq, clientRtpPort, clientRtcpPort, serverRtpPort = -1;
    char buf[128];

    if (!meta->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("response msg: has no request CSeq");
        EXIT_AND_RETURN1(false);
    }

    meta->getInt32(RTSP_MSG_SESSION, sessionID);

    appendCommonMessage(msg, cseq, sessionID);

    if (!meta->getString(RTSP_MSG_TRANSPORT, transport) || !transport) {
        ERROR("request SETUP has no Transport struct");
        EXIT_AND_RETURN1(false);
    }

    if (strncmp(transport, "RTP/AVP/UDP;unicast;", 20) &&
        strncmp(transport, "RTP/AVP;unicast;", 16)) {
        ERROR("request SETUP has bad Transport struct:\n%s", transport);
        EXIT_AND_RETURN1(false);
    }

    temp = transport;
    offset = temp.find("client_port=");

    if (offset == std::string::npos) {
        EXIT_AND_RETURN1(false);
    }

    clientPort = &transport[offset];

    if (sscanf(clientPort,
               "client_port=%d-%d", &clientRtpPort, &clientRtcpPort) == 2) {

    } else if (sscanf(clientPort,
               "client_port=%d", &clientRtpPort) == 1) {
        clientRtcpPort = -1;
    } else if (!strcmp(transport, "RTP/AVP/UDP;unicast;")) {
        clientRtpPort = 19000;
        clientRtcpPort = -1;
    }else {
        EXIT_AND_RETURN1(false);
    }

    if (clientRtcpPort >= 0) {
        ERROR("don't support rtcp");
        EXIT_AND_RETURN1(false);
    }

    meta->getInt32(LOCAL_RTP_PORT, serverRtpPort);

    sprintf(buf,
            "%s: RTP/AVP/UDP;unicast;client_port=%d;server_port=%d\r\n",
            RTSP_MSG_TRANSPORT, clientRtpPort, serverRtpPort);

    msg.append(buf);
    msg.append("\r\n");

    EXIT_AND_RETURN(true);
}

bool WfdRtspFramework::prepareResponseM7(MediaMetaSP meta, std::string &msg) {
    ENTER();

    int32_t sessionID = -1;
    int cseq;

    if (!meta->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("response msg: has no request CSeq");
        EXIT_AND_RETURN1(false);
    }

    meta->getInt32(RTSP_MSG_SESSION, sessionID);

    appendCommonMessage(msg, cseq, sessionID);
    msg.append("Range: npt=now-\r\n");
    msg.append("\r\n");

    EXIT_AND_RETURN(true);
}

bool WfdRtspFramework::prepareResponseM8(MediaMetaSP meta, std::string &msg) {
    ENTER();

    int32_t sessionID = -1;
    int cseq;

    if (!meta->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("response msg: has no request CSeq");
        EXIT_AND_RETURN1(false);
    }

    meta->getInt32(RTSP_MSG_SESSION, sessionID);

    appendCommonMessage(msg, cseq, sessionID);
    msg.append("Connection: close\r\n");
    msg.append("\r\n");

    EXIT_AND_RETURN(true);
}

bool WfdRtspFramework::prepareResponseM13(MediaMetaSP meta, std::string &msg) {
    ENTER();

    int32_t sessionID = -1;
    int cseq;

    if (!meta->getInt32(RTSP_MSG_CSEQ, cseq)) {
        ERROR("response msg: has no request CSeq");
        EXIT_AND_RETURN1(false);
    }

    if (!meta->getInt32(RTSP_MSG_SESSION, sessionID)) {
        ERROR("M13 request has invalid session");
        EXIT_AND_RETURN(false);
    }

    appendCommonMessage(msg, cseq, sessionID);
    msg.append("\r\n");

    EXIT_AND_RETURN(true);
}

void WfdRtspFramework::appendCommonMessage(
        std::string &msg, int32_t cseq, int32_t sessionID) {

    time_t now = time(NULL);
    struct tm *now2 = gmtime(&now);
    char buf[128];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", now2);

    msg.append(RTSP_MSG_DATE);
    msg.append(": ");
    msg.append(buf);
    msg.append("\r\n");

    msg.append(RTSP_MSG_SERVER);
    msg.append(": ");
    msg.append(RTSP_MSG_USER_AGENT);
    msg.append("\r\n");

    if (cseq >= 0) {
        msg.append(RTSP_MSG_CSEQ);
        msg.append(": ");
        sprintf(buf, "%d\r\n", cseq);
        msg.append(buf);
    }

    if (sessionID >= 0) {
        sprintf(buf, "Session: %d;timeout=%" PRId64 "\r\n",
                sessionID, (long long)SESSION_TIMEOUT_SEC);
        msg.append(buf);
    }
}

int WfdRtspFramework::stringToNumber(std::string &field) {
    char *end;
    int number = 9999;
    number = strtoul(field.c_str(), &end, 10);
    if (end == field.c_str()) {
        ERROR("no valid number, str is %s", field.c_str());
        number = 99999;
    }
    return number;
}

void WfdRtspFramework::parseRtspMessage(MediaMetaSP &meta) {
    ENTER();

    size_t offset = 0, lineOffset = 0;
    size_t offsetLineEnd = 0;
    size_t size = mRtspRecvString.size();
    const char *data = mRtspRecvString.c_str();
    MediaMetaSP result = MediaMeta::create();

    bool rtspHeaderFound = false;

    while (offset < size) {

        offsetLineEnd = offset;

        while (offsetLineEnd < size &&
               (data[offsetLineEnd] != '\r' || data[offsetLineEnd + 1] != '\n'))
            offsetLineEnd++;

        if (offsetLineEnd >= size)
            EXIT();

        std::string line;
        line.assign(&data[offset], offsetLineEnd - offset);

        // parse rtsp request/status line
        if (offset == 0) {
            parseStartLine(result, line);
            offset = offsetLineEnd + 2; // \r\n
            continue;
        }

        if (offsetLineEnd == offset) {
            rtspHeaderFound = true;
            offset += 2; // \r\n
            break;
        }

        lineOffset = line.find(":");
        if (lineOffset != std::string::npos) {
            std::string header, field;
            const char* lineData = line.c_str();

            header.assign(&lineData[0], lineOffset);
            lineOffset++;

            while (lineOffset < line.size() && lineData[lineOffset] == ' ')
                lineOffset++;

            field.assign(&lineData[lineOffset]);

            if (!strcmp(RTSP_MSG_CSEQ, header.c_str())) {
                int number = stringToNumber(field);
                result->setInt32(RTSP_MSG_CSEQ, number);
            } else if (!strcmp(RTSP_MSG_CONTENT_LENGTH, header.c_str())) {
                int number = stringToNumber(field);
                result->setInt32(RTSP_MSG_CONTENT_LENGTH, number);
            } else if (!strcmp(RTSP_MSG_SESSION, header.c_str())) {
                int number = stringToNumber(field);
                result->setInt32(RTSP_MSG_SESSION, number);
            } else
                result->setString(header.c_str(), field.c_str());

            VERBOSE("parse header: %s\n%s", header.c_str(), field.c_str());
        }

       offset = offsetLineEnd + 2; // \r\n
    }

    if (!rtspHeaderFound) {
        INFO("input buffer has no complete rtsp header, wait to recv more data");
        EXIT();
    }

    int contentLength = 0;
    result->getInt32(RTSP_MSG_CONTENT_LENGTH, contentLength);

    VERBOSE("get rtsp message content w/ length %d", contentLength);

    if (size < (offset + contentLength)) {
        INFO("input buffer has partial content, wait more data");
        EXIT1();
    }

    if (contentLength) {
        std::string content;
        content.assign(&data[offset], contentLength);
        VERBOSE("get rtsp content:\n%s", content.c_str());
        result->setString(RTSP_MSG_CONTENT, content.c_str());
    }

    meta = result;
    mRtspRecvString.erase(0, offset + contentLength);

    EXIT();
}

size_t WfdRtspFramework::parseNextWord(std::string &line, size_t offset, std::string &field) {
    ENTER();
    size_t offsetPrev = offset;
    const char* data = line.c_str();
    size_t size = line.size();

    if (data == NULL || (offset + 2) > size)
        EXIT_AND_RETURN(0);

    offset = line.find(' ', offsetPrev);

    if (offset == std::string::npos) {
        offset = 0;
        field.assign(&data[offsetPrev], size - offsetPrev);
    } else
        field.assign(&data[offsetPrev], offset - offsetPrev);

    VERBOSE("parse word %s", field.c_str());
    VERBOSE("line preoffset %d offset %d", offsetPrev, offset);

    EXIT_AND_RETURN(offset + 1);
}

void WfdRtspFramework::parseStartLine(MediaMetaSP result, std::string &line) {
    ENTER();

    std::string field;

    size_t lineOffset = parseNextWord(line, 0, field);

    if (!strncmp(field.c_str(), "RTSP/", 5)) {
        result->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_RESPONSE);
        result->setString(RTSP_VERSION, field.c_str());
        if (lineOffset) {
            char *end;
            int number;
            lineOffset = parseNextWord(line, lineOffset, field);
            number = strtoul(field.c_str(), &end, 10);
            if (end == field.c_str()) {
                ERROR("no valid status field in status line");
                number = 777;
            }
            result->setInt32(RTSP_STATUS, number);
        }
        if (lineOffset) {
            lineOffset = parseNextWord(line, lineOffset, field);
            result->setString(RTSP_REASON, field.c_str());
        }
    } else {
        result->setString(RTSP_MSG_TYPE, RTSP_MSG_TYPE_REQUEST);
        result->setString(RTSP_REQUEST_METHOD, field.c_str());
        if (lineOffset) {
            lineOffset = parseNextWord(line, lineOffset, field);
            result->setString(RTSP_URL, field.c_str());
        }
        if (lineOffset) {
            lineOffset = parseNextWord(line, lineOffset, field);
            result->setString(RTSP_VERSION, field.c_str());
        }
    }

    EXIT();
}

void WfdRtspFramework::sendRtspResponseError(int status, const char* reason, int cseq) {
    ENTER1();
    ERROR("send rtsp response(cseq %d) with status %d and reason %s",
          cseq, status, reason);

    std::string msg;
    char buf[10];
    sprintf(buf, "%d ", status);

    msg.append("RTSP/1.0 ");
    msg.append(buf);
    msg.append(reason);
    msg.append("\r\n");

    appendCommonMessage(msg, cseq);
    msg.append("\r\n");

    queueRtspMessage_locked(msg.c_str(), msg.size());

    EXIT1();
}

mm_status_t WfdRtspFramework::getClientIpPort()
{
    ENTER();
    sockaddr_in addrMy;
    memset(&addrMy,0,sizeof(addrMy));
    int ret, len = sizeof(addrMy);

    ret = getsockname(mSocket,(sockaddr*)&addrMy,&len);
    if (ret != 0) {
        ERROR("Getsockname Error:%d\n", ret);
        EXIT_AND_RETURN( MM_ERROR_OP_FAILED);
    }

    INFO("Current Socket IP:%s,port:%d\n", inet_ntoa(addrMy.sin_addr), ntohs(addrMy.sin_port));

    MediaMetaSP meta = MediaMeta::create();
    meta->setString(WFD_RTSP_LOCAL_IP, inet_ntoa(addrMy.sin_addr));
    if (!mInProgress && mListener)
        mListener->onRtspMessage(WFD_MSG_CONNECTED, 0, meta);

    EXIT_AND_RETURN(MM_ERROR_SUCCESS);
}

} // namespace YUNOS_MM
