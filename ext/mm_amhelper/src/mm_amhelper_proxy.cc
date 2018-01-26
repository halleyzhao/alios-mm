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

#include "mm_amhelper_proxy.h"

#ifndef MM_LOG_OUTPUT_V
#define MM_LOG_OUTPUT_V
#endif
#include <multimedia/mm_debug.h>


namespace YUNOS_MM {

DEFINE_LOGTAG(MMAMHelperProxy);


MMAMHelperProxy::MMAMHelperProxy(const yunos::SharedPtr<yunos::DService>& service) :
    yunos::DProxy(service,
        yunos::String("/com/saic/ivi/AudioManager/AudioFocus"),
        yunos::String("com.saic.ivi.AudioManager.AudioFocus"))
{
    MMLOGI("+\n");
    MMLOGV("-\n");
}

MMAMHelperProxy::~MMAMHelperProxy()
{
    MMLOGI("+\n");
    MMLOGV("-\n");
}


void MMAMHelperProxy::onBirth(const yunos::DLifecycleListener::BirthInfo &  birthInfo)
{
    MMLOGI("+\n");
}

void MMAMHelperProxy::onDeath(const yunos::DLifecycleListener::DeathInfo& deathInfo)
{
    MMLOGI("+\n");
}

}
