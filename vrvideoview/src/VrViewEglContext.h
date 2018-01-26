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

#ifndef __vr_view_egl_context_h
#define __vr_view_egl_context_h

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <os-compatibility.h>

//#include <WindowSurface.h>
#include <native_surface_help.h>
#include <vector>

//#include "egl-helper.h"
//#include "WindowSurfaceTestWindow.h"

class Surface;

namespace yunos {
namespace yvr {

class VrTransformMatrix;
class VrVideoView;

class VrViewSphereProjection {
public:
    VrViewSphereProjection()
        : mStack(20),
          mSlice(20),
          mLeftVertices(NULL),
          mLeftTextCoords(NULL),
          mRightVertices(NULL),
          mRightTextCoords(NULL) {
    }
    ~VrViewSphereProjection() {
        if (mLeftVertices)
            delete [] mLeftVertices;

        if (mLeftTextCoords)
            delete [] mLeftTextCoords;

        if (mRightVertices)
            delete [] mRightVertices;

        if (mRightTextCoords)
            delete [] mRightTextCoords;
    }

    int getStack() { return mStack; }
    int getSlice() { return mSlice; }

    // display mode
    enum Display {
        MONO,
        STEREO
    };

    // content stereo mode
    enum Stereo {
        NONE,
        LEFT_RIGHT,
        BOTTOM_DOWN
    };

    bool initVerticesAndTextCoords(Display d, Stereo s);

    const float *getLeftVertices(int index);
    const float *getLeftTextCoords(int index);
    const float *getRightVertices(int index);
    const float *getRightTextCoords(int index);

private:
    int mStack;
    int mSlice;

    float *mLeftVertices;
    float *mLeftTextCoords;

    float *mRightVertices;
    float *mRightTextCoords;
};


class VrViewEglContext {
    #define MAX_BUFFER_NUM 32
    //#define PI (3.1415926)
public:
    VrViewEglContext(int width, int height, int x, int y, VrVideoView* v);
    ~VrViewEglContext() { /* deinit(true); */ }

    bool init();
    bool deinit(bool destroyWindow);
    bool drawBuffer(MMNativeBuffer* anb, float pose[], float *rotMatrix);
    WindowSurface* windowSurface() { return mWindowSurface; }
    bool setRenderTarget(WindowSurface *surface, int rotation = 0);

private:
    bool ensureEglContext();
    void destroyEglContext();
    GLuint createEglImageTexture(MMNativeBuffer* anb);
    void createProgram();

private:
    EGLDisplay mEglDisplay;
    EGLConfig mEglConfig;
    EGLConfig* mConfigs;
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

    // view port
    int32_t mWidth;
    int32_t mHeight;
    int32_t mX;
    int32_t mY;
    int mRotation;

    WindowSurface* mWindowSurface;
    EGLSurface mEglSurface;
    EGLContext mEglContext;
    GLuint mVtxId;
    GLuint mFragId;
    GLuint mProgramId;
    GLint mMVPMatrixHandle;
    GLint mTexMatrixHandle;
    GLint mSamplerHandle;
    GLint mTexCoordsArray;
    GLint mVerticesArray;

    // gles2 transform matrix
    VrTransformMatrix *mViewMatrix;
    VrTransformMatrix *mProjectionMatrix;
    VrTransformMatrix *mMVPMatrix;
    VrTransformMatrix *mTexMatrix;

    typedef struct {
        MMNativeBuffer* anb;
        GLuint texId;
        EGLImageKHR img;
    }  BufferInfo;

    BufferInfo mBufferInfo[MAX_BUFFER_NUM];

    VrViewSphereProjection mSphereProjection;
    VrVideoView* mVrView;

private:
    VrViewEglContext(const VrViewEglContext&);
    VrViewEglContext& operator=(const VrViewEglContext&);
};

} // end of namespace yvr
} // end of namespace yunos
#endif // __vr_view_egl_context_h
