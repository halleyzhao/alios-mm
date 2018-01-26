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

#include "VrVideoViewImpl.h"
#include "VrViewEglContext.h"

#include <multimedia/mm_debug.h>

using YUNOS_MM::MMAutoLock;
using YunOSMediaCodec::MediaSurfaceTexture;

namespace yunos {

namespace yvr {

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

const char * VrVideoViewImpl::MM_LOG_TAG = "VrView";
const char * MstListener::MM_LOG_TAG = "VrView";

VrVideoViewImpl::VrVideoViewImpl()
    : YUNOS_MM::MMThread("VrRender"),
      mInitialized(false),
      mProjectionMode(NONE),
      mStereoMode(MONOSCOPIC),
      mDisplayMode(MONO),
      mBqConsumer(NULL),
      mProducerFd(-1),
      mMst(NULL),
      mOwnMst(false),
      mMstListener(NULL),
      mRenderTarget(NULL),
      mOwnRender(false),
      mViewX(-1),
      mViewY(-1),
      mViewW(-1),
      mViewH(-1),
      mRotation(0),
      mContinue(false),
      mCondition(mLock),
      mVrViewEglContext(NULL),
      mVrViewControllerApp(NULL),
      mVrViewController(NULL),
      mCurrentBuffer(NULL) {
    ENTER1();

    EXIT1();
}

VrVideoViewImpl::~VrVideoViewImpl() {
    ENTER1();

    {
        MMAutoLock lock(mLock);
        mContinue = false;
        mCondition.broadcast();
    }

    destroy();

    if (mMst) {
        if (mOwnMst)
            MM_RELEASE(mMst);
        else {
            mMst->setListener(NULL);
        }
        MM_RELEASE(mMstListener);
    }

    MM_RELEASE(mVrViewControllerApp);
    MM_RELEASE(mVrViewController);
    MM_RELEASE(mVrViewEglContext);

    if (mOwnRender && mRenderTarget) {
        // created internally from surface name
        mRenderTarget->unrefMe();
    }

    EXIT1();
}

bool VrVideoViewImpl::init() {
    ENTER1();

    if (mInitialized)
        EXIT_AND_RETURN1(true);

    if ((mProducerFd < 0) && (mMst == NULL)) {
        ERROR("Cannot init VrView as producer is not created");
        ERROR("producer fd is %d and mst is %p", mProducerFd, mMst);
        EXIT_AND_RETURN1(false);
    }

    if (!mRenderTarget) {
        //INFO("Render target is not set, create default");
        //mRenderTarget = createSimpleSurface(720, 1280, NST_PagewindowSub);
        //mViewX = mViewY = 0;
        //mViewW = 720;
        //mViewH = 1280;
        //mOwnRender = true;
        ERROR("render target is not set");
        EXIT_AND_RETURN1(false);
    }

    if (mMst) {
        mMstListener = new MstListener(this);
        mMst->setListener(mMstListener);
    }

    mContinue = true;
    int ret = create();

    mInitialized = !ret;
    EXIT_AND_RETURN1(mInitialized);
}

void* VrVideoViewImpl::createSurface() {
    ENTER1();
    MMAutoLock lock(mLock);
    if (!mMst) {
        mMst = new MediaSurfaceTexture();
    }
    mOwnMst = true;
    EXIT_AND_RETURN1(mMst);
}

bool VrVideoViewImpl::setSurfaceTexture(void* st) {
    ENTER1();
    MMAutoLock lock(mLock);

    if (!st || mMst) {
        ERROR("param st is %p, cur st is %p", st, mMst);
        EXIT_AND_RETURN1(false);
    }

    mMst = static_cast<MediaSurfaceTexture*>(st);
    mOwnMst = false;

    EXIT_AND_RETURN1(true);
}

int VrVideoViewImpl::createProducerFd() {
    ENTER1();
    ERROR("producer fd is not supported");
    EXIT_AND_RETURN1(-1);
}

bool VrVideoViewImpl::setDisplayTarget(void *displayTarget, int x, int y, int w, int h) {
    ENTER1();
    mRenderTarget = static_cast<WindowSurface*>(displayTarget);
    mViewX = x;
    mViewY = y;
    mViewW = w;
    mViewH = h;

    mOwnRender = false;

    EXIT_AND_RETURN1(true);
}

bool VrVideoViewImpl::setDisplayTarget(const char *surfaceName, int x, int y, int w, int h) {
    ENTER1();

    if (!surfaceName) {
        ERROR("render target name is null");
        EXIT_AND_RETURN1(false);
    }

    INFO("surface name: %s", surfaceName);
    mRenderTarget = new yunos::libgui::Surface(surfaceName);

    // FIXME set texture buffer dimensions, no need to do that, it's bq bug
    int ret;
    ret = native_set_buffers_dimensions(mRenderTarget, w, h);
    if (ret != 0) {
        mRenderTarget->unrefMe();
        mRenderTarget = NULL;
        ERROR("fail to set buffers dimension %d x %d", w, h);
        EXIT_AND_RETURN1(false);
    }

    Rect crop;
    crop.left = 0;
    crop.right = w;
    crop.top = 0;
    crop.bottom = h;
    ret = native_set_crop(mRenderTarget, &crop);

    if (ret != 0) {
        mRenderTarget->unrefMe();
        mRenderTarget = NULL;
        ERROR("fail to set buffers crop %d x %d", w, h);
        EXIT_AND_RETURN1(false);
    }

    mOwnRender = true;

    EXIT_AND_RETURN1(true);
}

bool VrVideoViewImpl::setVideoRotation(int degree) {
    ENTER1();

    if (degree % 90)
        EXIT_AND_RETURN1(false);

    mRotation = degree;

    EXIT_AND_RETURN1(true);
}

bool VrVideoViewImpl::setProjectionMode(PanoramicProjection mode) {
    ENTER1();
    mProjectionMode = mode;
    EXIT_AND_RETURN1(true);
}

void VrVideoViewImpl::getProjectionMode(PanoramicProjection &mode) {
    ENTER1();
    mode = mProjectionMode;
    EXIT1();
}

bool VrVideoViewImpl::setStereoMode(StereoMode mode) {
    ENTER1();
    mStereoMode = mode;
    EXIT_AND_RETURN1(true);
}

void VrVideoViewImpl::getStereoMode(StereoMode &mode) {
    ENTER1();
    mode = mStereoMode;
    EXIT1();
}

bool VrVideoViewImpl::setDisplayMode(DisplayMode mode) {
    ENTER1();
    mDisplayMode = mode;
    EXIT_AND_RETURN1(true);
}

void VrVideoViewImpl::getDisplayMode(DisplayMode &mode) {
    ENTER1();
    mode = mDisplayMode;
    EXIT1();
}

void VrVideoViewImpl::main() {
    ENTER1();

    MMNativeBuffer *anb;
    VrViewController *controllerAuto;
    VrViewController *controllerApp;

    if (!mVrViewEglContext) {
        INFO("create vr view egl context");
        mVrViewEglContext = new VrViewEglContext(mViewW, mViewH, mViewX, mViewY, this);
        if (!mVrViewEglContext) {
            ERROR("fail to creat vr view context, exit VrRender thread");
            EXIT1();
        }
        if (mRenderTarget) {
            mVrViewEglContext->setRenderTarget(mRenderTarget, mRotation);
        }
        if (!mVrViewEglContext->init()) {
            ERROR("fail to init vr view context, exit VrRender thread");
            MM_RELEASE(mVrViewEglContext);
            EXIT1();
        }
    }

restart:
    if (!mContinue || !mInitialized) {
        INFO("vr looper is going to be ended");
        if (mMst)
            mMst->returnAcquiredBuffers();
        if (!mVrViewEglContext) {
            mVrViewEglContext->deinit(true);
            MM_RELEASE(mVrViewEglContext);
        }

        if (mVrViewControllerApp)
            mVrViewControllerApp->stop();

        if (mVrViewController)
            mVrViewController->stop();

        EXIT1();
    }

    {
        MMAutoLock lock(mLock);
        controllerApp = controllerAuto = NULL;

        if (mVrViewControllerApp) {
            controllerApp = mVrViewControllerApp;
        }

        if (!mVrViewController) {
            INFO("create auto controller which tracks phone orientation");
            mVrViewController = VrViewController::create(0.f, 0.f, 0.f, true);
            VrViewControllerImpl *p = dynamic_cast<VrViewControllerImpl*>(mVrViewController);
            p->setListener(this);
            mVrViewController->start();
        }
        controllerAuto = mVrViewController;
    }

    bool poseUpdate = false;
    if (controllerApp && controllerApp->hasHeadPoseUpdate())
        poseUpdate = true;
    if (controllerAuto && controllerAuto->hasHeadPoseUpdate())
        poseUpdate = true;

    anb = NULL;

    int idx = -1;
    {
        MMAutoLock lock(mLock);

        if (mPendingBufferIdx.empty() && !poseUpdate) {
            mCondition.wait();
            goto restart;
        }

        if (!mPendingBufferIdx.empty()) {
            idx = mPendingBufferIdx.front();
            mPendingBufferIdx.pop_front();
        }
    }

    if (idx > -1) {
        anb = mMst->acquireBuffer(idx);

        if (!anb) {
            WARNING("acquire NULL buffer, index is %d", idx);
        } else {
            mMst->returnBuffer(mCurrentBuffer);
            mCurrentBuffer = anb;
        }
    }

    if (!anb) {
        DEBUG("repeat previous buffer with a different pose");
        anb = mCurrentBuffer;
    }

    if (!anb) {
        WARNING("no buffer to draw");
        goto restart;
    }

    float pose[3] = {0.f, 0.f, 0.f};
    float *p = NULL;
    float mtx[9];

    if (controllerApp) {
        controllerApp->getHeadPoseInStartSpace(pose[0], pose[1], pose[2]);
        p = pose;
    }

    if (controllerAuto) {
        std::vector<float> vec;
        vec = controllerAuto->getAngleChangeMatrix();
        for (int i = 0; i < 9; i++)
            mtx[i] = vec[i];
    }

    if (!mVrViewEglContext->drawBuffer(anb, p, mtx))
        WARNING("draw buffer fail");

goto restart;

    EXIT1();
}

void VrVideoViewImpl::getVrViewController(VrViewController** controller) {
    ENTER1();

    MMAutoLock lock(mLock);
    if (!mVrViewControllerApp) {
        mVrViewControllerApp = VrViewController::create(0.f, 0.f, 0.f);
        //mVrViewControllerApp->setListener(this);
        VrViewControllerImpl *p = dynamic_cast<VrViewControllerImpl*>(mVrViewControllerApp);
        p->setListener(this);
    }

    if (controller)
        *controller = mVrViewControllerApp;

    EXIT1();
}

// VrViewControllerListener::onHeadPoseUpdate
void VrVideoViewImpl::onHeadPoseUpdate() {
    MMAutoLock lock(mLock);
    mCondition.signal();
}

void MstListener::onMessage(int msg, int param1, int param2) {
    ENTER();

    if (!mOwner || !mOwner->mMst) {
        ERROR("abnormal listener whose owner is invalid");
        EXIT1();
    }

    {
        MMAutoLock lock(mOwner->mLock);
        mOwner->mPendingBufferIdx.push_back(param1);
        mOwner->mCondition.signal();
    }

    EXIT();
}

}; // end of namespace yvr
}; // end of namespace yunos
