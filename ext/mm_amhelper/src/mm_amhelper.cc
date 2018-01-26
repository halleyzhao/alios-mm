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

#include <dbus/dbus.h>

#include <multimedia/mm_amhelper.h>
#include "mm_amhelper_proxy.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>


namespace YUNOS_MM {

DEFINE_LOGTAG(MMAMHelper);


MMAMHelper::MMAMHelper()
    : mLooper(NULL),
    mProxy(NULL)
{
    MMLOGI("+\n");
    MMLOGV("-\n");
}

MMAMHelper::~MMAMHelper()
{
    MMLOGI("+\n");
    disconnect();
    MMLOGV("-\n");
}

mm_status_t MMAMHelper::connect(const char * channel/* = NULL*/)
{
    MMLOGI("+\n");
    if (!createProxy()) {
        return MM_ERROR_OP_FAILED;
    }

    if (!registerClient(channel)) {
        destroyProxy();
        return MM_ERROR_OP_FAILED;
    }

    if (!connectAM()) {
        unregisterClient();
        destroyProxy();
        return MM_ERROR_OP_FAILED;
    }

    return MM_ERROR_SUCCESS;
}

bool MMAMHelper::createProxy()
{
    MMLOGI("+\n");
    try {
        mLooper = new yunos::IoLooper();
    } catch (...) {
        MMLOGE("no mem\n");
        return false;
    }
    yunos::SharedPtr<yunos::DService> service = yunos::DServiceManager::getInstance()->getService(yunos::String("com.saic.ivi.AudioManager"));
    if (!service) {
        MMLOGE("failed to get service\n");
        MM_RELEASE(mLooper);
        return false;
    }

    try {
        mProxy = new MMAMHelperProxy(service);
    } catch (...) {
        MMLOGE("no mem\n");
        MM_RELEASE(mLooper);
        return false;
    }

    return true;
}

void MMAMHelper::destroyProxy()
{
    MM_RELEASE(mProxy);
    MM_RELEASE(mLooper);
}


void MMAMHelper::disconnect()
{
    MMLOGI("+\n");
    if (mProxy) {
        disconnectAM();
        unregisterClient();
    }
    destroyProxy();
}

bool MMAMHelper::registerClient(const char * channel)
{
    yunos::SharedPtr<yunos::DMessage> msg = mProxy->obtainMethodCallMessage(yunos::String("RegisterClient"));
    if (msg == NULL) {
        MMLOGE("failed to obtain message\n");
        return false;
    }

    msg->writeString("Media");
    msg->writeString(channel ? channel : "MainAudio");

    yunos::DError err;
    yunos::SharedPtr<yunos::DMessage> reply = mProxy->sendMessageWithReplyBlocking(msg, -1, &err);
    if (!reply) {
        MMLOGE("DError(%d): %s, detail: %s", err.type(), err.name().c_str(), err.message().c_str());
        if (err.type() == yunos::DError::ErrorType::BUS_ERROR_ACCESS_DENIED) {
            MMLOGE("permission is not allowed");
        }

        return false;
    }
    yunos::String connection_id = reply->readString();
    const char * p = strstr(connection_id.c_str(), "connection_id\":\"");
    if (!p) {
        MMLOGE("invalid response\n");
        return false;
    }
    p += strlen("connection_id\":\"");
    mConnectionId = "";
    while (p && *p != '\0' && *p != '\"') {
        mConnectionId += *(p++);
    }
    MMLOGI("connection_id: src: %s, dst: %s\n",
        connection_id.c_str(), mConnectionId.c_str());

    return true;
}

void MMAMHelper::unregisterClient()
{
    yunos::SharedPtr<yunos::DMessage> msg = mProxy->obtainMethodCallMessage(yunos::String("DeregisterClient"));
    if (msg == NULL) {
        MMLOGE("failed to obtain message\n");
        return;
    }

    msg->writeString(mConnectionId.c_str());

    yunos::DError err;
    yunos::SharedPtr<yunos::DMessage> reply = mProxy->sendMessageWithReplyBlocking(msg, -1, &err);
    if (!reply) {
        MMLOGE("DError(%d): %s, detail: %s", err.type(), err.name().c_str(), err.message().c_str());
        if (err.type() == yunos::DError::ErrorType::BUS_ERROR_ACCESS_DENIED) {
            MMLOGE("permission is not allowed");
        }

        return;
    }
    yunos::String result = reply->readString();
    MMLOGI("result: %s\n", result.c_str());
    mConnectionId = "";
}

bool MMAMHelper::connectAM()
{
    yunos::SharedPtr<yunos::DMessage> msg = mProxy->obtainMethodCallMessage(yunos::String("Connect"));
    if (msg == NULL) {
        MMLOGE("failed to obtain message\n");
        return false;
    }

    msg->writeString(mConnectionId.c_str());

    yunos::DError err;
    yunos::SharedPtr<yunos::DMessage> reply = mProxy->sendMessageWithReplyBlocking(msg, -1, &err);
    if (!reply) {
        MMLOGE("DError(%d): %s, detail: %s", err.type(), err.name().c_str(), err.message().c_str());
        if (err.type() == yunos::DError::ErrorType::BUS_ERROR_ACCESS_DENIED) {
            MMLOGE("permission is not allowed");
        }

        return false;
    }
    yunos::String result = reply->readString();
    if (result != "{\"result\":1,\"code\":0}") {
        MMLOGI("connect failed: %s\n", result.c_str());
        return false;
    }
    MMLOGI("result: %s\n", result.c_str());
    return true;
}

void MMAMHelper::disconnectAM()
{
    yunos::SharedPtr<yunos::DMessage> msg = mProxy->obtainMethodCallMessage(yunos::String("Disconnect"));
    if (msg == NULL) {
        MMLOGE("failed to obtain message\n");
        return;
    }

    msg->writeString(mConnectionId.c_str());

    yunos::DError err;
    yunos::SharedPtr<yunos::DMessage> reply = mProxy->sendMessageWithReplyBlocking(msg, -1, &err);
    if (!reply) {
        MMLOGE("DError(%d): %s, detail: %s", err.type(), err.name().c_str(), err.message().c_str());
        if (err.type() == yunos::DError::ErrorType::BUS_ERROR_ACCESS_DENIED) {
            MMLOGE("permission is not allowed");
        }

        return;
    }
    yunos::String result = reply->readString();
    MMLOGI("result: %s\n", result.c_str());
}

}
