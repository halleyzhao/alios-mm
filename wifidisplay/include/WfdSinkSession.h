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

#ifndef __WFD_SOURCE_SINK_H__
#define __WFD_SOURCE_SINK_H__

#include "WfdSession.h"
#include "multimedia/cowplayer.h"

namespace YUNOS_MM
{

typedef struct tagVideoConfigData
{
    int width;
    int height;
    int fps;
}VideoConfigData;

class WfdSinkSession : public WfdSession,
                       public CowPlayer::Listener
{
public:
typedef struct WfdSinkConfig {
    int nativeDisplayWidth;
    int nativeDisplayHeight;
    int nativeDisplayRefreshRate;
    void *surface;
    const char *surfaceName;
} WfdSinkConfig;

public:
    WfdSinkSession(const char* iface, IWfdSessionCB* listener, WfdSinkConfig *config = NULL);
    virtual ~WfdSinkSession();

    // MediaPlayer::Listener
    void onMessage(int msg, int param1, int param2, const MMParamSP param);

protected:
    //after accept the request from net, handleRtsp* is handle the request
    virtual bool handleRtspOption(MediaMetaSP request);//request
    virtual bool handleRtspGetParameter(MediaMetaSP request);//request
    virtual bool handleRtspSetParameter(MediaMetaSP request);// source sink
    virtual bool handleRtspPlay(MediaMetaSP request);

    //FIXME handleRequestM3 M16 is the analysis the M3 and M6
    bool handleRequestM3(MediaMetaSP request);
    bool handleRequestM4(MediaMetaSP request);
    bool handleRequestM5(MediaMetaSP request);
    bool handleRequestM16(MediaMetaSP request);

    //sink send M* message to source
    virtual bool sendWfdM2(); // sink
    virtual bool sendWfdM6(); // sink
    virtual bool sendWfdM7(); // sink
    virtual bool sendWfdM8(); // sink
    virtual bool sendWfdM9(); // sink
    virtual bool sendWfdM13();//sink

    //after accept the response message from net, handleResponeM* handle the reponse
    virtual bool handleResponseM2( MediaMetaSP ); // sink
    virtual bool handleResponseM6( MediaMetaSP ); // sink
    virtual bool handleResponseM7( MediaMetaSP ); // sink
    virtual bool handleResponseM8( MediaMetaSP ); // source sink
    virtual bool handleResponseM9( MediaMetaSP ); // source sink
    virtual bool handleResponseM13( MediaMetaSP ); //source sink

    virtual void setupMediaPipeline();
    virtual void teardownMediaPipeline();
    virtual void stopMediaPipeline();
    void playMediaPipeline();
    mm_status_t parseVideoFormat( const char* content);
    mm_status_t getCEAWidthHeight(int& width, int& height, const char* content);
    mm_status_t getVESAWidthHeight(int& width, int& height, const char* content);
    mm_status_t getHHWidthHeight(int& width, int& height, const char* content);

private:
    void *mWindow;
    bool mOwnWindow;
    CowPlayer *mPlayer;
    bool mRequestIDR;
    bool mRequestIDRPending;

    int mWidth;
    int mHeight;
    int mParametersFlags;
    DECLARE_MSG_HANDLER2(handleMediaEvent)
    DECLARE_MSG_HANDLER2(handleWfdRequestIDR)
    DECLARE_MSG_HANDLER2(handlePlayerStart)
};

} // end of namespace YUNOS_MM
#endif //__wfd_source_sink_h
