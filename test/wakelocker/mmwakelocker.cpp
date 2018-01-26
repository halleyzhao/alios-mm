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

#include "mmwakelocker.h"
MM_LOG_DEFINE_MODULE_NAME("mmwakelocaker");

using yunos::PowerControllerInner;
static MMPowerRequestSP acquireWakeLock(){

    MMPowerControllerSP pc = PowerControllerInner::getInstance();
    if (!pc.pointer()) {
        ERROR( "power manager failed to create \n");
        return (MMPowerRequestSP(NULL));
   }

    MMPowerRequestSP req = pc->createPowerRequest(yunos::PowerControllerInner::SCREEN_INSTANT_ON
        | yunos::PowerControllerInner::KEEP_SCREEN_FULL | yunos::PowerControllerInner::SCREEN_DELAYED_OFF, MM_LOG_TAG);

    if (!req) {
        ERROR("create wakelock error\n");
        return (MMPowerRequestSP(NULL));
    }

    if (req->testSent()) {
        INFO("already hold, no need acquire\n");
        return (MMPowerRequestSP(NULL));
    }
    req->send();
    PRINTF("acquireWakeLock \n");
    return req;
}

static void releaseWakeLock(MMPowerRequestSP req) {
    if (req && req->testSent()) {
        INFO( "release wake lock\n");
        req->revoke();
    }
    PRINTF("releaseWakeLock \n");
}

AutoWakeLock::AutoWakeLock(){
    m_lock = acquireWakeLock();
    if(!m_lock){
        ERROR("acquireWakeLock fail or already hold\n");
    }
}

AutoWakeLock:: ~AutoWakeLock(){
    releaseWakeLock(m_lock);
}

