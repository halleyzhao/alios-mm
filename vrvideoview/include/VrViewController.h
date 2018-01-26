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

#ifndef __vr_view_controller_h
#define __vr_view_controller_h

#include <vector>

namespace yunos {

namespace yvr {

#define PI (3.1415926f)

class VrViewController {

public:
    static VrViewController* create(float pitch, float roll, float yaw, bool autoCtrl = false);

    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isStopped() = 0;
    //virtual bool reset() = 0;

    //bool setFov();
    virtual bool reCentralize() = 0;

    /*
     * Head pose from start space in three freedom degree
     * pitch ranges [-PI/2 to PI/2]
     * roll ranges [-PI, PI]
     * yaw ranges [-PI, PI]
     */
    virtual bool setHeadPoseInStartSpace(float pitch, float roll, float yaw) = 0;
    virtual bool getHeadPoseInStartSpace(float &pitch, float &roll, float &yaw) = 0;

    virtual std::vector<float> getAngleChangeMatrix() = 0;
    virtual bool hasHeadPoseUpdate() = 0;

    virtual ~VrViewController();

protected:
    VrViewController(float pitch, float roll, float yaw);

private:
    VrViewController(const VrViewController&);
    VrViewController& operator=(const VrViewController&);
};

}; // end of namespace yvr
}; // end of namespace yunos

#endif // __vr_view_controller_h
