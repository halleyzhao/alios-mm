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

#ifndef __vr_view_controller_impl_h
#define __vr_view_controller_impl_h

#include <multimedia/mm_cpp_utils.h>
#include <VrViewController.h>

#include <thread/LooperThread.h>
#include <string/String.h>

#include <sensor/SensorManager.h>

namespace yunos {

namespace yvr {

class VrViewControllerListener {
public:
    virtual void onHeadPoseUpdate() = 0;
};

class SensorTrackor;

class VrViewControllerImpl : public VrViewController {

public:
    VrViewControllerImpl(float pitch, float roll, float yaw, bool autoCtrl);
    virtual ~VrViewControllerImpl();

    virtual bool start();
    virtual bool stop();
    virtual bool isStopped();
    //virtual bool reset();

    //bool setFov();
    virtual bool reCentralize();
    virtual bool setHeadPoseInStartSpace(float pitch, float roll, float yaw);
    virtual bool getHeadPoseInStartSpace(float &pitch, float &roll, float &yaw);
    virtual bool setHeadPoseByAddAngle(float pitch, float roll, float yaw);
    virtual vector<float> getAngleChangeMatrix();
    virtual bool hasHeadPoseUpdate();

    void createOrientationTracking();
    void destroyOrientationTracking();

    void setListener( VrViewControllerListener* listener);

friend class SensorTrackor;
private:
    YUNOS_MM::Lock mLock;

    float mCurPitch;
    float mCurRoll;
    float mCurYaw;

    float mOrigPitch;
    float mOrigRoll;
    float mOrigYaw;

    float mStartPitch;
    float mStartRoll;
    float mStartYaw;

    bool mAutoCtrl;
    bool mStarted;
    bool mUpdatePending;

    SharedPtr<LooperThread> mSensorLooper;

    SharedPtr<SensorManager> mSensorMgr;
    SensorInfoList mTypeList;
    SharedPtr<SensorInfo> mSensor;
    SharedPtr<SensorTrackor> mTrackor;

    int64_t mTrackStartOffsetUs;
    int64_t mTrackStartUs;
    bool mTrackStart;

    vector<float> mStartR;
    bool mUpdateStartR;
    vector<float> mRotDiffMatrix;
    vector<float> mPrevR;

    VrViewControllerListener* mListener;

    inline void getStartHeadPose(float &pitch, float &roll, float &yaw) {
        pitch = mStartPitch;
        roll = mStartRoll;
        yaw = mStartYaw;
    }

    inline void setStartHeadPose(float pitch, float roll, float yaw) {
        mStartPitch = pitch;
        mStartRoll = roll;
        mStartYaw = yaw;
    }

    void setAngleChangeMatrix(vector<float> &vec);

    void checkHeadPoseChange(float pitch, float roll, float yaw);

    void checkHeadPoseTrack(float pitch, float roll, float yaw);

    static int64_t getTimeUs();
private:
    bool setStartRMatrix(vector<float> &s);
    bool setPrevRMatrix(vector<float> &s);
    void checkHeadPoseChange_l(float pitch, float roll, float yaw);

    vector<float>& getStartRMatrix();
    vector<float>& getPrevRMatrix();
private:
    static const char * MM_LOG_TAG;

private:
    VrViewControllerImpl(const VrViewControllerImpl&);
    VrViewControllerImpl& operator=(const VrViewControllerImpl&);
};

}; // end of namespace yvr
}; // end of namespace yunos

#endif // __vr_view_controller_impl_h
