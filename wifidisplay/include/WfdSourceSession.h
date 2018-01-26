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

#ifndef __wfd_source_session_h
#define __wfd_source_session_h

#include "WfdSession.h"
#include "multimedia/cowrecorder.h"

namespace YUNOS_MM {

class WfdSourceSession : public WfdSession,
                         public CowRecorder::Listener {

public:
    WfdSourceSession(const char* iface, IWfdSessionCB* listener);
    virtual ~WfdSourceSession();

    // MediaRecorder::Listener
    void onMessage(int msg, int param1, int param2, const MMParamSP param);

protected:
    virtual bool handleRtspOption(MediaMetaSP request);// source sink
    virtual bool handleRtspSetup(MediaMetaSP request);// source
    virtual bool handleRtspPlay(MediaMetaSP request);// source
    virtual bool handleRtspPause(MediaMetaSP request);// source
    virtual bool handleRtspTearDown(MediaMetaSP request);// source
    virtual bool handleRtspSetParameter(MediaMetaSP request);// source sink

    virtual bool sendWfdM3(); // source
    virtual bool sendWfdM4(); // source
    virtual bool sendWfdM5(const char*); // source
    virtual bool sendWfdM12(); // source sink
    virtual bool sendWfdM14(); // source sink
    virtual bool sendWfdM15(); // source sink
    virtual bool sendWfdM16(); // source

    virtual bool handleResponseM3(MediaMetaSP); // source
    virtual bool handleResponseM4(MediaMetaSP); // source
    virtual bool handleResponseM5(MediaMetaSP); // source
    virtual bool handleResponseM12(MediaMetaSP); // source sink
    virtual bool handleResponseM14(MediaMetaSP); // source sink
    virtual bool handleResponseM15(MediaMetaSP); // source sink
    virtual bool handleResponseM16(MediaMetaSP); // source

    virtual void setupMediaPipeline();
    virtual void teardownMediaPipeline();
    virtual void stopMediaPipeline();

private:

    CowRecorder *mRecorder;

    DECLARE_MSG_HANDLER2(handleMediaEvent)

};

} // end of namespace YUNOS_MM
#endif //__wfd_source_session_h
