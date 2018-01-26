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

#include "WfdParameters.h"

namespace YUNOS_MM {

// wfd rtsp message id
const char *WFD_RTSP_MSG_ID = "wfd-rtsp-msg-id";

const char *WFD_RTSP_MSG_M1 = "wfd-rtsp-m1";
const char *WFD_RTSP_MSG_M2 = "wfd-rtsp-m2";
const char *WFD_RTSP_MSG_M3 = "wfd-rtsp-m3";
const char *WFD_RTSP_MSG_M4 = "wfd-rtsp-m4";
const char *WFD_RTSP_MSG_M5 = "wfd-rtsp-m5";
const char *WFD_RTSP_MSG_M6 = "wfd-rtsp-m6";
const char *WFD_RTSP_MSG_M7 = "wfd-rtsp-m7";
const char *WFD_RTSP_MSG_M8 = "wfd-rtsp-m8";
const char *WFD_RTSP_MSG_M9 = "wfd-rtsp-m9";
const char *WFD_RTSP_MSG_M10 = "wfd-rtsp-m10";
const char *WFD_RTSP_MSG_M11 = "wfd-rtsp-m11";
const char *WFD_RTSP_MSG_M12 = "wfd-rtsp-m12";
const char *WFD_RTSP_MSG_M13 = "wfd-rtsp-m13";
const char *WFD_RTSP_MSG_M14 = "wfd-rtsp-m14";
const char *WFD_RTSP_MSG_M15 = "wfd-rtsp-m15";
const char *WFD_RTSP_MSG_M16 = "wfd-rtsp-m16";

// rtsp message id
const char *RTSP_MSG_TYPE = "rtsp-msg-type";

const char *RTSP_MSG_TYPE_REQUEST = "rtsp-request";
const char *RTSP_MSG_TYPE_RESPONSE = "rtsp-response";

const char *RTSP_MSG_CSEQ = "CSeq";
const char *RTSP_MSG_DATE = "Date";
const char *RTSP_MSG_SERVER = "Server";
const char *RTSP_MSG_SESSION = "Session";
const char *RTSP_MSG_REQUIRE = "Require: org.wfa.wfd1.0\r\n";
const char *RTSP_MSG_USER_AGENT = "cow/0.2.0 (Linux, YunOS6.0)";
const char *RTSP_MSG_VERSION = "RTSP/1.0";
const char *RTSP_MSG_CONTENT_LENGTH = "Content-Length";
const char *RTSP_MSG_TRANSPORT = "Transport";

// misc
const char *RTSP_RAW_MSG = "rtsp-raw-msg";
const char *LOCAL_RTP_PORT = "local-rtp-port";

// status line files
const char *RTSP_HEADER_LINE = "rtsp-header-line";

const char *RTSP_VERSION = "rtsp-version";
const char *RTSP_STATUS = "rtsp-status";

const char *RTSP_REASON = "rtsp-reason";
const char *RTSP_REASON_BAD_REQUEST = "Bad Request";
const char *RTSP_REASON_PREPROCESS_FAIL = "Pre-processe fail";

// request line files
const char *RTSP_URL = "rtsp-url";

// rtsp method
const char *RTSP_REQUEST_METHOD = "rtsp-request-mdthod";

const char *RTSP_REQUEST_METHOD_OPTION = "OPTIONS";
const char *RTSP_REQUEST_METHOD_GETPARAMETER = "GET_PARAMETER";
const char *RTSP_REQUEST_METHOD_SETPARAMETER = "SET_PARAMETER";
const char *RTSP_REQUEST_METHOD_SETUP = "SETUP";
const char *RTSP_REQUEST_METHOD_PLAY = "PLAY";
const char *RTSP_REQUEST_METHOD_PAUSE = "PAUSE";
const char *RTSP_REQUEST_METHOD_TEARDOWN = "TEARDOWN";

// wfd rtsp message body attribute
const char *RTSP_MSG_CONTENT = "rtsp-content";

const char *WFD_RTSP_AUDIO_CODECS = "wfd_audio_codecs";
const char *WFD_RTSP_VIDEO_FORMATS = "wfd_video_formats";
const char *WFD_RTSP_CONTENT_PROTECTION = "wfd_content_protection";
const char *WFD_RTSP_CLIENT_RTP_PORTS = "wfd_client_rtp_ports";
const char *WFD_RTSP_PRESENTION_URL = "wfd_presentation_URL";
const char *WFD_RTSP_TRIGGER_METHOD = "wfd_trigger_method";
const char *WFD_RTSP_IDR_REQUEST = "wfd_idr_request";
const char *WFD_RTSP_UIBC_CAPABILITY = "wfd_uibc_capability";
const char *WFD_RTSP_UIBC_SETTING = "wfd_uibc_setting";
const char *WFD_RTSP_REMOTE_IP = "remoteIP";
const char *WFD_RTSP_LOCAL_IP = "localIP";

const char *WFD_RTSP_PARAMETER_FLAG = "wfd_parameter_flag";
} // namespace YUNOS_MM
