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

#ifndef __wfd_parameters_h
#define __wfd_parameters_h

namespace YUNOS_MM {

// wfd rtsp message id
extern const char *WFD_RTSP_MSG_ID;

extern const char *WFD_RTSP_MSG_M1;
extern const char *WFD_RTSP_MSG_M2;
extern const char *WFD_RTSP_MSG_M3;
extern const char *WFD_RTSP_MSG_M4;
extern const char *WFD_RTSP_MSG_M5;
extern const char *WFD_RTSP_MSG_M6;
extern const char *WFD_RTSP_MSG_M7;
extern const char *WFD_RTSP_MSG_M8;
extern const char *WFD_RTSP_MSG_M9;
extern const char *WFD_RTSP_MSG_M10;
extern const char *WFD_RTSP_MSG_M11;
extern const char *WFD_RTSP_MSG_M12;
extern const char *WFD_RTSP_MSG_M13;
extern const char *WFD_RTSP_MSG_M14;
extern const char *WFD_RTSP_MSG_M15;
extern const char *WFD_RTSP_MSG_M16;

extern const char *RTSP_MSG_TYPE;

extern const char *RTSP_MSG_TYPE_REQUEST;
extern const char *RTSP_MSG_TYPE_RESPONSE;

extern const char *RTSP_MSG_CSEQ;
extern const char *RTSP_MSG_DATE;
extern const char *RTSP_MSG_SERVER;
extern const char *RTSP_MSG_SESSION;
extern const char *RTSP_MSG_REQUIRE;
extern const char *RTSP_MSG_USER_AGENT;
extern const char *RTSP_MSG_VERSION;
extern const char *RTSP_MSG_CONTENT_LENGTH;
extern const char *RTSP_MSG_TRANSPORT;

extern const char *RTSP_RAW_MSG;
extern const char *LOCAL_RTP_PORT;

extern const char *RTSP_HEADER_LINE;

// status line files
extern const char *RTSP_VERSION;
extern const char *RTSP_STATUS;
extern const char *RTSP_REASON;
extern const char *RTSP_REASON_BAD_REQUEST;
extern const char *RTSP_REASON_PREPROCESS_FAIL;

// request line files
extern const char *RTSP_URL;

// rtsp method
extern const char *RTSP_REQUEST_METHOD;

extern const char *RTSP_REQUEST_METHOD_OPTION;
extern const char *RTSP_REQUEST_METHOD_GETPARAMETER;
extern const char *RTSP_REQUEST_METHOD_SETPARAMETER;
extern const char *RTSP_REQUEST_METHOD_SETUP;
extern const char *RTSP_REQUEST_METHOD_PLAY;
extern const char *RTSP_REQUEST_METHOD_PAUSE;
extern const char *RTSP_REQUEST_METHOD_TEARDOWN;

// rtsp message body attribute
extern const char *RTSP_MSG_CONTENT;

extern const char *WFD_RTSP_AUDIO_CODECS;
extern const char *WFD_RTSP_VIDEO_FORMATS;
extern const char *WFD_RTSP_CONTENT_PROTECTION;
extern const char *WFD_RTSP_PRESENTION_URL;
extern const char *WFD_RTSP_CLIENT_RTP_PORTS;

extern const char *WFD_RTSP_TRIGGER_METHOD;
extern const char *WFD_RTSP_IDR_REQUEST;
extern const char *WFD_RTSP_UIBC_CAPABILITY;
extern const char *WFD_RTSP_UIBC_SETTING;
extern const char *WFD_RTSP_PARAMETER_FLAG;
extern const char *WFD_RTSP_REMOTE_IP;
extern const char *WFD_RTSP_LOCAL_IP;

#define WFD_VIDEO_FORMAT_FLAG 1
#define WFD_AUDIO_CODEC_FLAG  2
#define WFD_3D_VIDEO_FORMAT_FLAG  4
#define WFD_CONTENT_PROTECTION_FLAG  8
#define WFD_DISPLAY_EDID_FLAG  16
#define WFD_COUPLED_SINK_FLAG  32
#define WFD_CLIENT_RTP_PORTS_FLAG  64
#define WFD_UIBC_CAPABILITY_FLAG  128

} // end of namespace YUNOS_MM
#endif //__wfd_parameters_h
