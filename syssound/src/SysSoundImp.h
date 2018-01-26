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

#ifndef __SysSoundImp_H
#define __SysSoundImp_H

#include <multimedia/mmparam.h>
#include <multimedia/SysSound.h>
#include "UBusNode.h"
#include "SysSoundProxy.h"

namespace YUNOS_MM {
typedef MMSharedPtr<ClientNode<SysSoundProxy> > SysSoundCNSP;
template <class ClientType> class UBusMessage;

class SysSoundImp : public SysSound {
public:
    SysSoundImp();
    ~SysSoundImp();

public:
    bool connect();

    virtual mm_status_t play(SoundType soundType, float leftVolume, float rightVolume);
    mm_status_t handleMsg(MMParamSP param);
    bool handleSignal(const yunos::SharedPtr<yunos::DMessage>& msg) { return true; }

private:
    friend class UBusMessage<SysSoundImp>;

    yunos::SharedPtr<UBusMessage<SysSoundImp> > mMessager;
    SysSoundCNSP mClientNode;

    MM_DISALLOW_COPY(SysSoundImp)
    DECLARE_LOGTAG()
};

}

#endif
