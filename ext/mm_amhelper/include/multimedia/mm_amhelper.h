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

#ifndef __mm_amhelper_H
#define __mm_amhelper_H

#include <dbus/DServiceManager.h>
#include <dbus/DProxy.h>
#include <dbus/DService.h>
#include <looper/Looper.h>

#include <multimedia/mm_types.h>
#include <multimedia/mm_errors.h>
#include <multimedia/mm_cpp_utils.h>


namespace YUNOS_MM {

class MMAMHelperProxy;

class MMAMHelper {
public:
    MMAMHelper();
    ~MMAMHelper();
    mm_status_t connect(const char * channel = NULL);
    void disconnect();
    const char * getConnectionId() const { return mConnectionId.c_str(); };

    static const char * playChnnelMain() { return "MainAudio"; }
    static const char * recordChnnelMic() { return "MIC"; }

private:
    bool createProxy();
    void destroyProxy();
    bool registerClient(const char * channel);
    void unregisterClient();
    bool connectAM();
    void disconnectAM();

private:
    yunos::IoLooper * mLooper;
    MMAMHelperProxy * mProxy;
    std::string mConnectionId;

    MM_DISALLOW_COPY(MMAMHelper)
    DECLARE_LOGTAG()
};

#ifdef ENABLE_DEFAULT_AUDIO_CONNECTION
#define ENSURE_AUDIO_DEF_CONNECTION_DECLARE() MMAMHelper * mEnsureAMHelper;
#define ENSURE_AUDIO_DEF_CONNECTION_INIT() mEnsureAMHelper = NULL

#define ENSURE_AUDIO_DEF_CONNECTION_ENSURE(_connection_id, _channel) do {\
    if (!_connection_id.empty()) {\
        MMLOGI("audio connection id is set: %s\n", _connection_id.c_str());\
        break;\
    }\
    try {\
        mEnsureAMHelper = new MMAMHelper();\
        if (mEnsureAMHelper->connect(_channel) != MM_ERROR_SUCCESS) {\
            MMLOGE("audio connection id not set, use default, but failed to connect\n");\
            delete mEnsureAMHelper;\
            mEnsureAMHelper = NULL;\
            break;\
        }\
        _connection_id = mEnsureAMHelper->getConnectionId();\
        MMLOGE("audio connection id not set, use default: %s\n", _connection_id.c_str());\
    } catch (...) {\
        MMLOGE("audio connection id not set, use default, but no mem\n");\
        break;\
    }\
}while(0)

#define ENSURE_AUDIO_DEF_CONNECTION_CLEAN() do {\
    if (!mEnsureAMHelper) break;\
    mEnsureAMHelper->disconnect();\
    delete mEnsureAMHelper;\
    mEnsureAMHelper = NULL;\
}while(0)
#endif

}

#endif
