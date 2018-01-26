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

#ifndef __vr_video_view_impl_h
#define __vr_video_view_impl_h

#include <multimedia/mm_cpp_utils.h>
#include <multimedia/mmthread.h>
#include <native_surface_help.h>
#include <media_surface_texture.h>

#include <VrVideoView.h>
#include <VrViewControllerImpl.h>


#include <BufferQueue.h>

#include <list>

namespace yunos {

namespace yvr {

class VrViewEglContext;

class VrVideoViewImpl : public YUNOS_MM::MMThread,
                        public VrVideoView,
                        public VrViewControllerListener {

public:

    VrVideoViewImpl();
    virtual ~VrVideoViewImpl();

    virtual bool init();

    virtual void* createSurface();
    virtual bool setSurfaceTexture(void *st);
    virtual int createProducerFd();

    virtual bool setDisplayTarget(void *displayTarget, int x, int y, int w, int h);
    virtual bool setDisplayTarget(const char *surfaceName, int x, int y, int w, int h);

    virtual bool setVideoRotation(int degree);

    virtual bool setProjectionMode(PanoramicProjection mode);
    virtual void getProjectionMode(PanoramicProjection &mode);

    virtual bool setStereoMode(StereoMode mode);
    virtual void getStereoMode(StereoMode &mode);

    virtual bool setDisplayMode(DisplayMode mode);
    virtual void getDisplayMode(DisplayMode &mode);

    virtual void getVrViewController(VrViewController** controller);

    virtual void onHeadPoseUpdate();

friend class MstListener;

protected:
    virtual void main();

private:
    bool mInitialized;

    PanoramicProjection mProjectionMode;
    StereoMode mStereoMode;
    DisplayMode mDisplayMode;

    yunos::libgui::BufferPipeConsumer *mBqConsumer;
    int mProducerFd;

    YunOSMediaCodec::MediaSurfaceTexture *mMst;
    bool mOwnMst;
    YunOSMediaCodec::SurfaceTextureListener *mMstListener;

    WindowSurface *mRenderTarget;
    bool mOwnRender;

    int mViewX;
    int mViewY;
    int mViewW;
    int mViewH;
    int mRotation;

    bool mContinue;
    YUNOS_MM::Lock mLock;
    YUNOS_MM::Condition mCondition;
    std::list<int> mPendingBufferIdx;

    VrViewEglContext* mVrViewEglContext;

    VrViewController* mVrViewControllerApp;
    VrViewController* mVrViewController;

    MMNativeBuffer *mCurrentBuffer;

private:
    static const char * MM_LOG_TAG;

    VrVideoViewImpl(const VrVideoViewImpl&);
    VrVideoViewImpl& operator=(const VrVideoViewImpl&);
};

class MstListener : public YunOSMediaCodec::SurfaceTextureListener {
public:
    MstListener(VrVideoViewImpl *vr)
        : mOwner(vr) {
    }

    virtual void onMessage(int msg, int param1, int param2);

private:
    static const char * MM_LOG_TAG;
    VrVideoViewImpl *mOwner;
};

}; // end of namespace yvr
}; // end of namespace yunos

#endif // __vr_video_view_impl_h
