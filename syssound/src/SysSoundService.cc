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

#include <multimedia/SysSoundService.h>
#include "UBusNode.h"
#include "SysSoundServiceAdaptor.h"

namespace YUNOS_MM {
typedef MMSharedPtr<ServiceNode<SysSoundServiceAdaptor> > SysSoundServiceSNSP;

class SysSoundServiceImp : public SysSoundService {
public:
    SysSoundServiceImp()
    {
    }

    ~SysSoundServiceImp()
    {
        mService.reset();
    }

public:
    bool publish()
    {
        MMASSERT(mService == NULL);
        try {
            mService = ServiceNode<SysSoundServiceAdaptor>::create("ssndsvr");
            if (!mService->init()) {
                MMLOGE("failed to init\n");
                mService.reset();
                return false;
            }

            MMLOGI("success\n");
            return true;
        } catch (...) {
            MMLOGE("no mem\n");
            return false;
        }
    }

private:
    SysSoundServiceSNSP mService;

    DECLARE_LOGTAG()
    MM_DISALLOW_COPY(SysSoundServiceImp)
};

DEFINE_LOGTAG(SysSoundService);
DEFINE_LOGTAG(SysSoundServiceImp);

/*static */SysSoundServicePtr SysSoundService::publish()
{
    try {
        SysSoundServiceImp * imp = new SysSoundServiceImp();
        if (!imp->publish()) {
            MMLOGE("failed to publish\n");
            delete imp;
            return SysSoundServicePtr((SysSoundService*)NULL);
        }

        return SysSoundServicePtr((SysSoundService*)imp);
    } catch (...) {
        MMLOGE("no mem\n");
        return SysSoundServicePtr((SysSoundService*)NULL);
    }
}

}
