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

#include "multimedia/mm_debug.h"
#include "multimedia/mm_cpp_utils.h"
#include <pointer/SharedPtr.h>
#include <PowerControllerInner.h>
typedef yunos::SharedPtr<yunos::PowerControllerInner> MMPowerControllerSP;
typedef yunos::SharedPtr<yunos::PowerControllerInner::PowerRequest> MMPowerRequestSP;

class AutoWakeLock
{
public:
    explicit AutoWakeLock();
    ~AutoWakeLock();
private:
    MMPowerRequestSP m_lock;
};

