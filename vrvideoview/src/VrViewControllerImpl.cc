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

#include "VrViewControllerImpl.h"
#include "VrEularAngleMatrix.h"
#include <multimedia/mm_debug.h>
#include <math.h>

using YUNOS_MM::MMAutoLock;

namespace yunos {

namespace yvr {

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define HEAD_POSE_UPDATE_THRESHOLD (0.02f)
#define HEAD_POSE_TRACKING_THRESHOLD (-0.65f)
#define HEAD_TRACK_START_OFFSET (1000000)

const char * VrViewControllerImpl::MM_LOG_TAG = "VrViewCtrl";

class SensorTrackor :public SensorEventCallback {
public:
    SensorTrackor(VrViewControllerImpl *p)
        : mOwner(p) {

    }

    void onNewSensorEvent(int type, const SensorEvent& evt) {
        SharedPtr<SensorManager> mgr = SensorManager::getInstance();
        if (type != SENSOR_TYPE_ORIENTATION && type != SENSOR_TYPE_ROTATION_VEC) {
            WARNING("receive unexpected sensor event %d", type);
            return;
        }

        if (!mOwner) {
            ERROR("trackor has no owner");
            return;
        }

        vector<float> matrix;
        vector<float> R;
        matrix.resize(9, 0);
        R.resize(9, 0);

        if (type == SENSOR_TYPE_ORIENTATION) {
            DEBUG("sensor[%d] timestamp:%lld roll:%f, pitch:%f, yaw:%f",
                   evt.sensorInfo->getID(), evt.timestamp, evt.values[0],
                   evt.values[1], evt.values[2]);
#if 0
            float roll;
            float pitch;
            float yaw;

            roll = (evt.values[0] - 180.0f) * 2 * PI / 360.0f;
            pitch = evt.values[1] * 2 * PI / 360.0f;
            yaw = evt.values[2] * 2 * PI / 360.0f;
#endif

        } else if (type == SENSOR_TYPE_ROTATION_VEC) {
            DEBUG("sensor[%d] timestamp:%lld r_vec0:%f, r_vec1:%f, r_vec2:%f, values[3] %f",
                   evt.sensorInfo->getID(), evt.timestamp, evt.values[0],
                   evt.values[1], evt.values[2], evt.values[3]);

            vector<float> vec3;
            float orientation[3];

            vec3.resize(3);
            vec3[0] = evt.values[0];
            vec3[1] = evt.values[1];
            vec3[2] = evt.values[2];

            SensorManager::getRotationMatrixFromVector(vec3, &matrix);

            // remap to surface 90 rotation
            if (!SensorManager::remapCoordinateSystem(matrix,
                                                      SensorManager::AXIS_Y,
                                                      SensorManager::AXIS_MINUS_X,
                                                      &R)) {
                ERROR("remap coordinate system fail");
                return;
            }

            SensorManager::getOrientation(R, orientation);
            DEBUG("orientation angle [roll %f pitch %f yaw %f]",
                  orientation[0], orientation[1], orientation[2]);

            // by pass coordinate remap
            // R = matrix;

            mOwner->checkHeadPoseTrack(orientation[1], orientation[0], orientation[2]);

            if (!mOwner->mTrackStart)
                return;

            if (!(mOwner->mUpdateStartR)) {
                if (!(mOwner->setStartRMatrix(R)))
                    ERROR("fail to set start rotation matrix");
                mOwner->setPrevRMatrix(R);
            }

            vector<float> &startR = mOwner->getStartRMatrix();

            SensorManager::getAngleChange(R, startR, orientation);

            DEBUG("angle from start space [roll %f pitch %f yaw %f]",
                  orientation[0], orientation[1], orientation[2]);

            vector<float> rd;

            rd.resize(9);
            VrEularAngleMatrix::getRotationDiffMatrix(rd, R, startR);

            vector<float> &prevR = mOwner->getPrevRMatrix();
            SensorManager::getAngleChange(R, prevR, orientation);
            mOwner->checkHeadPoseChange(orientation[0], orientation[1], orientation[2]);

            // only update prev R matrix after head pose update
            if (mOwner->mUpdatePending)
                mOwner->setPrevRMatrix(R);

            mOwner->setAngleChangeMatrix(rd);
        }

        return;
    }

    void onNewAccuracy(SharedPtr<SensorInfo> sensor, int accuracy) {
        DEBUG("onNewAccuracy : sensor[%d] accuracy=%d\n",
               sensor->getID(), accuracy);
    }

    void onFlushFinished(SharedPtr<SensorInfo> sensor) {
        DEBUG("onFlushFinished: sensor[%d]\n", sensor->getID());
    }

private:
    static const char * MM_LOG_TAG;

    VrViewControllerImpl *mOwner;
};

const char *SensorTrackor::MM_LOG_TAG = "VrViewCtrl";

VrViewControllerImpl::VrViewControllerImpl(float pitch, float roll, float yaw, bool autoCtrl)
    : VrViewController(pitch, roll, yaw),
      mCurPitch(pitch),
      mCurRoll(roll),
      mCurYaw(yaw),
      mOrigPitch(pitch),
      mOrigRoll(roll),
      mOrigYaw(yaw),
      mAutoCtrl(autoCtrl),
      mStarted(false),
      mUpdatePending(false),
      mTrackStartOffsetUs(HEAD_TRACK_START_OFFSET),
      mTrackStartUs(-1),
      mTrackStart(false),
      mUpdateStartR(false) {
    ENTER1();

    mTrackor = new SensorTrackor(this);

    mStartPitch = mOrigPitch;
    mStartRoll = mOrigRoll;
    mStartYaw = mOrigYaw;

    mRotDiffMatrix.resize(9, 0.0f);

    mRotDiffMatrix[0] = 1.0f;
    mRotDiffMatrix[4] = 1.0f;
    mRotDiffMatrix[8] = 1.0f;

    mListener = NULL;

    EXIT1();
}

VrViewControllerImpl::~VrViewControllerImpl() {
    ENTER1();

    mListener = NULL;
    stop();

    EXIT1();
}

static void createOrientationTracking(VrViewControllerImpl* instance) {
    if (instance)
        instance->createOrientationTracking();
}

static void destroyOrientationTracking(VrViewControllerImpl* instance) {
    if (instance)
        instance->destroyOrientationTracking();
}

bool VrViewControllerImpl::start() {
    ENTER1();
    MMAutoLock lock(mLock);

    if (mStarted == true) {
        INFO("alread started");
        return true;
    }

    mStarted = true;

    if (!mAutoCtrl)
        EXIT_AND_RETURN1(true);

    mSensorLooper = new LooperThread();

    mSensorLooper->start("VrViewOrientaton");
    if (mSensorLooper->isRunning() != true) {
        ERROR("LooperThread is not running");
        EXIT_AND_RETURN1(false);
    }

    mSensorLooper->sendTask(Task(yunos::yvr::createOrientationTracking, this));

    EXIT_AND_RETURN1(true);
}

bool VrViewControllerImpl::stop() {
    ENTER1();
    MMAutoLock lock(mLock);

    if (mStarted == false) {
        INFO("alread stopped");
        return true;
    }

    mStarted = false;

    if (mSensorLooper) {
        mSensorLooper->sendTask(Task(yunos::yvr::destroyOrientationTracking, this));
        mSensorLooper->requestExitAndWait();
    }

    mSensorLooper.reset();

    EXIT_AND_RETURN1(true);
}

bool VrViewControllerImpl::isStopped() {
    ENTER1();
    MMAutoLock lock(mLock);
    EXIT_AND_RETURN1(!mStarted);
}

bool VrViewControllerImpl::reCentralize() {
    ENTER1();
    MMAutoLock lock(mLock);

    if (!mStarted) {
        ERROR("operation fail as controller is stopped");
        EXIT_AND_RETURN1(false);
    }

    mCurPitch = mOrigPitch;
    mCurRoll = mOrigRoll;
    mCurYaw = mOrigYaw;

    EXIT_AND_RETURN1(true);
}

bool VrViewControllerImpl::setHeadPoseByAddAngle(float pitch, float roll, float yaw) {
    ENTER1();
    bool update;
    {
        MMAutoLock lock(mLock);

	float change1 = 0.0f, change2 = 0.0f, change3 = 0.0f;
        if (!mStarted) {
            ERROR("operation fail as controller is stopped");
            EXIT_AND_RETURN1(false);
        }

        pitch = mStartPitch + pitch;
        roll = mStartRoll + roll;
        yaw = mStartYaw + yaw;

        if (pitch < -PI/2 || pitch > PI/2) {
            WARNING("pitch[%f] is out of range", pitch);
        } else {
            change1 = pitch - mCurPitch;
            mCurPitch = pitch;
        }

        if (roll < -PI || roll > PI) {
            WARNING("roll[%f] is out of range", roll);
        } else {
            change2 = roll - mCurRoll;
            mCurRoll = roll;
        }

        if (yaw < -PI || yaw > PI) {
            WARNING("yaw[%f] is out of range", yaw);
        } else {
            change3 = yaw - mCurYaw;
            mCurYaw = yaw;
        }

        //mCurPitch = pitch;
        mCurRoll = roll;
        mCurYaw = yaw;

        VERBOSE("roll %f, pitch %f, yaw %f", mCurRoll, mCurPitch, mCurYaw);
        checkHeadPoseChange_l(change1, change2, change3);
        update = mUpdatePending;
    }
    if (mListener && update)
        mListener->onHeadPoseUpdate();

    EXIT_AND_RETURN1(true);
}

bool VrViewControllerImpl::setHeadPoseInStartSpace(float pitch, float roll, float yaw) {
    ENTER1();

    bool update = false;
    {
        MMAutoLock lock(mLock);

	float change1 = 0.0f, change2 = 0.0f, change3 = 0.0f;
        if (!mStarted) {
            ERROR("operation fail as controller is stopped");
            EXIT_AND_RETURN1(false);
        }

        if (pitch < -PI/2 || pitch > PI/2) {
            WARNING("pitch[%f] is out of range", pitch);
        } else {
            change1 = pitch - mCurPitch;
            mCurPitch = pitch;
        }

        if (roll < -PI || roll > PI) {
            WARNING("roll[%f] is out of range", roll);
        } else {
            change2 = roll - mCurRoll;
            mCurRoll = roll;
        }

        if (yaw < -PI || yaw > PI) {
            WARNING("yaw[%f] is out of range", yaw);
        } else {
            change3 = yaw - mCurYaw;
            mCurYaw = yaw;
        }

        //mCurPitch = pitch;
        mCurRoll = roll;
        mCurYaw = yaw;

        VERBOSE("roll %f, pitch %f, yaw %f", mCurRoll, mCurPitch, mCurYaw);
        checkHeadPoseChange_l(change1, change2, change3);
        update = mUpdatePending;

    }
    if (mListener && update)
        mListener->onHeadPoseUpdate();
    EXIT_AND_RETURN1(true);
}

bool VrViewControllerImpl::getHeadPoseInStartSpace(float &pitch, float &roll, float &yaw) {
    ENTER();
    MMAutoLock lock(mLock);

    if (!mStarted) {
        ERROR("operation fail as controller is stopped");
        EXIT_AND_RETURN1(false);
    }

    pitch = mCurPitch;
    roll = mCurRoll;
    yaw = mCurYaw;

    EXIT_AND_RETURN(true);
}

void VrViewControllerImpl::createOrientationTracking() {
    ENTER1();

    SharedPtr<SensorManager> sensorMgr = SensorManager::getInstance();
    //int sensorType = SENSOR_TYPE_ORIENTATION;
    int sensorType = SENSOR_TYPE_ROTATION_VEC;
    int64_t samplingPeriod = yunos::SENSOR_SAMPLING_NORMAL;
    int64_t maxReportLatency = 0;
    bool wakeup = false;

    if (!sensorMgr) {
        ERROR("Error: SensorManager::getInstance failed\n");
        goto exit;
    }

    mTypeList = sensorMgr->getSensorInfos(sensorType);
    mSensorMgr = sensorMgr;
    for (int i = 0; i < (int)mTypeList.size(); ++i) {
        //PrintSensorInfo(mTypeList[i], i);
        if (mTypeList[i]->isWakeUp() == wakeup) {
            mSensor = mTypeList[i];
            break;
        }
    }

    if (mSensor == NULL) {
        mSensor = sensorMgr->getPrefSensorInfo(sensorType);
    }

    if (mSensor == NULL) {
        ERROR("No Such Sensor!");
        goto exit;
    }

    if (!sensorMgr->startSensor(mSensor, mTrackor, samplingPeriod, maxReportLatency)) {
        ERROR("registerListener for sensor type:%d failed", sensorType);
        goto exit;
    }

    EXIT1();

exit:
    mSensorLooper->requestExitAndWait();
    EXIT1();
}

void VrViewControllerImpl::destroyOrientationTracking() {
    ENTER1();

    if (mSensorMgr)
        mSensorMgr->stopSensor(mSensor, mTrackor);

    mSensorMgr.reset();

    EXIT1();
}

bool VrViewControllerImpl::setStartRMatrix(vector<float> &s) {
    ENTER1();
    MMAutoLock lock(mLock);

    if (s.size() != 9)
        return false;

    mStartR.resize(9);
    mStartR = s;
    mUpdateStartR = true;

    return true;
}

bool VrViewControllerImpl::setPrevRMatrix(vector<float> &s) {
    ENTER();
    MMAutoLock lock(mLock);

    if (s.size() != 9)
        return false;

    //mPrevR.resize(9);
    mPrevR = s;

    return true;
}

vector<float>& VrViewControllerImpl::getStartRMatrix() {
    MMAutoLock lock(mLock);
    return mStartR;
}

vector<float>& VrViewControllerImpl::getPrevRMatrix() {
    MMAutoLock lock(mLock);
    return mPrevR;
}

void VrViewControllerImpl::setAngleChangeMatrix(vector<float> &vec) {

    bool update = false;
    {
        MMAutoLock lock(mLock);
        if (vec.size() != 9) {
            ERROR("angle change matrix is invalid");
            return;
        }

        mRotDiffMatrix = vec;
        update = mUpdatePending;
    }

    if (mListener && update)
        mListener->onHeadPoseUpdate();
}

vector<float> VrViewControllerImpl::getAngleChangeMatrix() {
    MMAutoLock lock(mLock);
    if (mRotDiffMatrix.size() != 9)
        WARNING("rotation diff matrix looks odd");

    return mRotDiffMatrix;
}

bool VrViewControllerImpl::hasHeadPoseUpdate() {
    MMAutoLock lock(mLock);
    bool r = mUpdatePending;
    if (mUpdatePending)
        mUpdatePending = false;

    return r;
}

void VrViewControllerImpl::setListener( VrViewControllerListener* listener) {
    MMAutoLock lock(mLock);
    mListener = listener;
}

void VrViewControllerImpl::checkHeadPoseChange(float pitch, float roll, float yaw) {
    MMAutoLock lock(mLock);
    checkHeadPoseChange_l(pitch, roll, yaw);
}

void VrViewControllerImpl::checkHeadPoseChange_l(float pitch, float roll, float yaw) {
    if (mUpdatePending)
        return;

    if (fabsf(pitch) + fabsf(roll) + fabsf(yaw) > HEAD_POSE_UPDATE_THRESHOLD) {
        DEBUG("angle change is larger than threshold");
        mUpdatePending = true;
    }
}

/*static*/ int64_t VrViewControllerImpl::getTimeUs()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000LL + t.tv_nsec / 1000LL;
}

void VrViewControllerImpl::checkHeadPoseTrack(float pitch, float roll, float yaw) {
    if (mUpdateStartR || mTrackStart)
        return;

    if (mTrackStartUs != -1) {
        if (getTimeUs() < (mTrackStartUs + mTrackStartOffsetUs))
            return;

        mTrackStart = true;
        return;
    }

    if (pitch > HEAD_POSE_TRACKING_THRESHOLD) {
        INFO("device is not in landscape orientation");
        mTrackStart = false;
        return;
    }

    INFO("start head track");
    mTrackStartUs = getTimeUs();
}

}; // end of namespace yvr
}; // end of namespace yunos
