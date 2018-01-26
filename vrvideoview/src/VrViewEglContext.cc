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

#include "VrViewEglContext.h"
#include "VrTransformMatrix.h"
#include "VrEularAngleMatrix.h"
#include "VrVideoView.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <multimedia/mm_debug.h>
#include <multimedia/mm_cpp_utils.h>
#include <WindowSurfaceTestWindow.h>

namespace yunos {
namespace yvr {

MM_LOG_DEFINE_MODULE_NAME("yvrEglContext")

//#define VERBOSE INFO

#define ENTER() VERBOSE(">>>\n")
#define EXIT() do {VERBOSE(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN(_code) do {VERBOSE("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define ENTER1() INFO(">>>\n")
#define EXIT1() do {INFO(" <<<\n"); return;}while(0)
#define EXIT_AND_RETURN1(_code) do {INFO("<<<(status: %d)\n", (_code)); return (_code);}while(0)

#define VERTEX_POSITION 0
#define PI (3.1415926)

static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
};

static EGLint config_attribs[] = {
       EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
       EGL_RED_SIZE,        1,
       EGL_GREEN_SIZE,      1,
       EGL_BLUE_SIZE,       1,
       EGL_ALPHA_SIZE,      1,
       EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
       EGL_NONE
};

#if 0
static const char* vs = "                                   \n\
        attribute vec2 position;                            \n\
        varying vec2 texCoords;                       	\n\
        void main()                                         \n\
        {                                                   \n\
           gl_Position = vec4(position, 0.0, 1.0);          \n\
           texCoords =  (position.xy + vec2(1.0, 1.0))*0.5;          \n\
        }";

static const char* fs = "\n\
        #extension GL_OES_EGL_image_external : require\n\
        precision mediump float;\n\
        varying vec2 texCoords;\n\
        uniform samplerExternalOES sampler;\n\
        void main()\n\
        {\n\
           gl_FragColor = texture2D(sampler, texCoords);\n\
        }";
#endif

#if 0
const char *vs =
        "attribute vec4 position;\n"
        "varying vec2 texCoords;\n"
        "uniform mat4 texMatrix;\n"
        "void main() {\n"
        "  vec2 vTexCoords = 0.5 * (position.xy + vec2(1.0, 1.0));\n"
        "  texCoords = (texMatrix * vec4(vTexCoords, 0.0, 1.0)).xy;\n"
        "  gl_Position = position;\n"
        "}\n";

const char *fs =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "uniform samplerExternalOES sampler;\n"
        "varying vec2 texCoords;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(sampler, texCoords);\n"
        "}\n";


#else
const char *vs =
        "attribute vec4 position;\n"
        "varying vec2 texCoords;\n"
        "attribute vec2 texCoordsArray;\n"
        "uniform mat4 MVPMatrix;\n"
        "uniform mat4 texMatrix;\n"
        "void main() {\n"
        //"  texCoords = (texMatrix * vec4(texCoordsArray, 0.0, 1.0)).xy;\n"
        "  texCoords = texCoordsArray;\n"
        "  gl_Position = MVPMatrix * position;\n"
        "}\n";

const char *fs =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "uniform samplerExternalOES sampler;\n"
        "varying vec2 texCoords;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(sampler, texCoords);\n"
        "}\n";
#endif

static const GLfloat vtx[] = {-1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

VrViewEglContext::VrViewEglContext(int width, int height, int x, int y, VrVideoView *v)
    : mEglDisplay(NULL), mEglConfig(NULL), mConfigs(NULL),
      eglCreateImageKHR(NULL),eglDestroyImageKHR(NULL),glEGLImageTargetTexture2DOES(NULL),
      mWidth(width), mHeight (height), mX(x), mY(y), mRotation(0),
      mWindowSurface(NULL), mEglSurface(EGL_NO_SURFACE), mEglContext(EGL_NO_CONTEXT),
      mVtxId(0), mFragId(0), mProgramId(0), mMVPMatrixHandle(-1),mTexMatrixHandle(-1),mSamplerHandle(-1),mTexCoordsArray(-1), mVerticesArray(-1),
      mViewMatrix(NULL), mProjectionMatrix(NULL), mMVPMatrix(NULL), mTexMatrix(NULL),
      mVrView(v) {

    int i=0;
    for (i=0; i<MAX_BUFFER_NUM; i++) {
        mBufferInfo[i].anb = NULL;
        mBufferInfo[i].texId = -1;
        mBufferInfo[i].img = EGL_NO_IMAGE_KHR;
    }

    int crop[4];
    crop[0] = mX;
    crop[1] = mY;
    crop[2] = mWidth;
    crop[3] = mHeight;

    mViewMatrix = new VrTransformMatrix(crop);
    //mMatrix->setTransform(mRotation);
    mProjectionMatrix = new VrTransformMatrix(crop);
    mMVPMatrix = new VrTransformMatrix(crop);
    mTexMatrix = new VrTransformMatrix(crop);
}

bool VrViewEglContext::setRenderTarget(WindowSurface *surface, int rotation) {
    ENTER1();

    if (mEglDisplay || mEglConfig) {
        ERROR("cannot set render target after context initialized");
        EXIT_AND_RETURN1(false);
    }

    //mWindowSurface = static_cast<WindowSurface*>(surface);
    mWindowSurface = surface;
    mRotation = rotation;

    INFO("render target %p, rotation %d", mWindowSurface, mRotation);
    EXIT_AND_RETURN1(true);
}

void VrViewEglContext::destroyEglContext() {
    ENTER1();
    if (!mEglDisplay)
        EXIT1();

    eglTerminate(mEglDisplay);
    free(mConfigs);
    mConfigs = NULL;
    mEglDisplay= NULL;
    mEglConfig = NULL;
    EXIT1();
}

bool VrViewEglContext::ensureEglContext() {
    ENTER1();

    EGLint major, minor;
    EGLBoolean ret = EGL_FALSE;
    EGLint count = -1, n = 0, i = 0, size = 0;

    if (mEglDisplay && mEglConfig)
        EXIT_AND_RETURN1(true);

    ASSERT(!mEglDisplay && !mEglConfig);

    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    ASSERT(eglCreateImageKHR != NULL);
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    ASSERT(eglDestroyImageKHR != NULL);
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    ASSERT(glEGLImageTargetTexture2DOES != NULL);

    // FIXME don't use getWaylandDisplay from WindowSurfaceTestWindow.cc

#ifdef YUNOS_ENABLE_UNIFIED_SURFACE
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#else
    mEglDisplay = eglGetDisplay(getWaylandDisplay());
#endif

    ASSERT(mEglDisplay != NULL);
    ret = eglInitialize(mEglDisplay, &major, &minor);
    ASSERT(ret == EGL_TRUE);

    ret = eglBindAPI(EGL_OPENGL_ES_API);
    ASSERT(ret == EGL_TRUE);

    if (!eglGetConfigs(mEglDisplay, NULL, 0, &count) || count < 1)
        ASSERT(0);

    mConfigs = (EGLConfig*)malloc(sizeof(EGLConfig) * count);
    ASSERT(mConfigs);
    memset(mConfigs, 0, sizeof(EGLConfig) * count);

    ret = eglChooseConfig(mEglDisplay, config_attribs, mConfigs, count, &n);
    ASSERT(ret && n >= 1);

    for (i = 0; i < n; i++) {
        eglGetConfigAttrib(mEglDisplay, mConfigs[i], EGL_BUFFER_SIZE, &size);
        if (32 == size) {
            mEglConfig = mConfigs[i];
            break;
        }
    }

    DEBUG("ensureEglContext done");
    EXIT_AND_RETURN1(true);
}

bool VrViewEglContext::init() {
    ENTER1();

    if (!ensureEglContext())
        EXIT_AND_RETURN1(false);

    EGLBoolean ret;

    if (!mWindowSurface) {
        ERROR("there is not render target");
        return false;
    }

    mEglContext = eglCreateContext(mEglDisplay, mEglConfig, NULL, context_attribs);

    mEglSurface = eglCreateWindowSurface(mEglDisplay, mEglConfig,
            (EGLNativeWindowType)mWindowSurface->get_wl_egl_window(), NULL);

    ASSERT(mWindowSurface && mEglContext != EGL_NO_CONTEXT && mEglSurface != EGL_NO_SURFACE);

    ret = eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext);
    ASSERT(ret == EGL_TRUE);

    createProgram();

    //glViewport(mX, mY, mWidth, mHeight);
    //mWindowSurface->setSurfaceResize(mWidth, mHeight);
    //mWindowSurface->setOffset(mX, mY);

    INFO("init egl texture done\n");
    EXIT_AND_RETURN1(true);
}

bool VrViewEglContext::deinit(bool destroyWindow) {
    ENTER1();
    DEBUG("destroyWindow: %d", destroyWindow);
    int i = 0;
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mBufferInfo[i].anb == NULL)
            break;
        glDeleteTextures(1, &mBufferInfo[i].texId);
        eglDestroyImageKHR(mEglDisplay, mBufferInfo[i].img);

        mBufferInfo[i].anb = NULL;
        mBufferInfo[i].texId = 0;
        mBufferInfo[i].img = EGL_NO_IMAGE_KHR;
    }

    if (!destroyWindow) {
        EXIT_AND_RETURN1(true);
    }

    // Free GL resource.
    if (mVtxId > 0) {
        ASSERT(mFragId >0 && mProgramId > 0);
        glDeleteShader(mVtxId);
        glDeleteShader(mFragId);
        glDeleteProgram(mProgramId);

        mVtxId = 0;
        mFragId = 0;
        mProgramId = 0;
        mSamplerHandle = -1;
    }

    if (mWindowSurface) {
        ASSERT(mEglDisplay != EGL_NO_DISPLAY && mEglContext != EGL_NO_CONTEXT && mEglSurface != EGL_NO_SURFACE);
        eglDestroyContext(mEglDisplay, mEglContext);
        eglDestroySurface(mEglDisplay, mEglSurface);
        eglMakeCurrent(mEglDisplay, NULL, NULL, NULL);

        mEglContext = EGL_NO_CONTEXT;
        mEglSurface = EGL_NO_SURFACE;
        mWindowSurface = NULL;
    }

    destroyEglContext();

    MM_RELEASE(mViewMatrix);
    MM_RELEASE(mProjectionMatrix);
    MM_RELEASE(mMVPMatrix);
    MM_RELEASE(mTexMatrix);

    EXIT_AND_RETURN1(true);
}

GLuint VrViewEglContext::createEglImageTexture(MMNativeBuffer* anb) {
    ENTER1();

    DEBUG("glGetError %d\n", glGetError());

    int i;
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mBufferInfo[i].anb == anb)
            EXIT_AND_RETURN(mBufferInfo[i].texId);
        if (mBufferInfo[i].anb == NULL)
            break;
    }

    if (i >= MAX_BUFFER_NUM) {
        ERROR("run out of buffer, mBufferInfo is full\n");
        EXIT_AND_RETURN1((GLuint)(-1));
    }

    mBufferInfo[i].anb = anb;

    EGLClientBuffer clientBuffer = (EGLClientBuffer)anb;
    mBufferInfo[i].img = eglCreateImageKHR(mEglDisplay, EGL_NO_CONTEXT, 0x3140, clientBuffer, 0);
    ASSERT(mBufferInfo[i].img != EGL_NO_IMAGE_KHR);
    // increase ref count; since eglDestroyImageKHR will decrease one refcount for mali driver
    VERBOSE("(%s, %d): eglCreateImageKHR\n", __func__, __LINE__);
    glGenTextures(1, &mBufferInfo[i].texId);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mBufferInfo[i].texId);
    //glBindTexture(GL_TEXTURE_2D, mBufferInfo[i].texId);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, mBufferInfo[i].img);

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    INFO("generate texture[%d] %d for anb %p\n", i, mBufferInfo[i].texId, anb);
    //return mBufferInfo[i].texId;
    EXIT_AND_RETURN(mBufferInfo[i].texId);
}

bool VrViewEglContext::drawBuffer(MMNativeBuffer* anb, float pose[], float *rotMatrix) {
    GLuint texId = 0;
    texId = createEglImageTexture(anb);

    if (texId < 0) {
        ERROR("get invalid texture id", texId);
        return false;
    }

    //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Suppose roll pitch yaw are inputs from user
     * and rotMatrix is input from SensorManager.
     * User input only takes pitch and yaw, discard roll,
     * and user input are applied after rotMatrix
     */

    float eye[3] = {0.0f, 0.0f, -1.0f};
    float headY[3] = {0.0f, 1.0f, 0.0f};

    float pitch = 0.0f;
    float roll = 0.0f;
    float yaw = 0.0f;

    if (pose) {
       pitch = pose[0];
       roll = pose[1];
       yaw = pose[2];
    }

    float *mvp;
    float *mtx;

    DEBUG("roll %f pitch %f yaw %f\n", roll, pitch, yaw);

    float altitude = (float) (pitch);
    float azimuth = (float)((+PI / 2) +  yaw);

    //eye[0] = (float)(cos(altitude) * cos(azimuth));
    //eye[1] = (float)sin(altitude);
    //eye[2] = (float) - (cos(altitude) * sin(azimuth));

    VrEularAngleMatrix eularMatrix;

    eularMatrix.applyXRotation(altitude);
    eularMatrix.applyYRotation(azimuth);

    mtx = eularMatrix.getMatrix();

    if (rotMatrix) {
        DEBUG("apply rotation diff matrix");
        DEBUG("%f %f %f", rotMatrix[0], rotMatrix[1], rotMatrix[2]);
        DEBUG("%f %f %f", rotMatrix[3], rotMatrix[4], rotMatrix[5]);
        DEBUG("%f %f %f", rotMatrix[6], rotMatrix[7], rotMatrix[8]);
        VrEularAngleMatrix::matrixMultiply(mtx, mtx, rotMatrix);
    }

    if (mtx) {
        float temp[3];

        temp[0] = mtx[0]*eye[0] + mtx[1]*eye[1] + mtx[2]*eye[2];
        temp[1] = mtx[3]*eye[0] + mtx[4]*eye[1] + mtx[5]*eye[2];
        temp[2] = mtx[6]*eye[0] + mtx[7]*eye[1] + mtx[8]*eye[2];

        eye[0] = temp[0];
        eye[1] = temp[1];
        eye[2] = temp[2];

        temp[0] = mtx[0]*headY[0] + mtx[1]*headY[1] + mtx[2]*headY[2];
        temp[1] = mtx[3]*headY[0] + mtx[4]*headY[1] + mtx[5]*headY[2];
        temp[2] = mtx[6]*headY[0] + mtx[7]*headY[1] + mtx[8]*headY[2];

        headY[0] = temp[0];
        headY[1] = temp[1];
        headY[2] = temp[2];
    }

    mViewMatrix->setLookAtMatrix(0.0f, 0.0f, 0.0f
            //, 0.0f, 0.0f, -1.0f
            , eye[0], eye[1], eye[2]
            //, 0.0f, 1.0f, 0.0f);
            , headY[0], headY[1], headY[2]);


    mvp = mMVPMatrix->getMatrix();
    VrTransformMatrix::matrixMultiply(mvp, mProjectionMatrix->getMatrix(), mViewMatrix->getMatrix());

    glUseProgram(mProgramId);

    glUniform1i(mSamplerHandle, 0);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texId);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glEnableVertexAttribArray(VERTEX_POSITION);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glEnableVertexAttribArray(mTexCoordsArray);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    int line = 0;
    int index = (mSphereProjection.getSlice() + 1) * 6 * line;
    glVertexAttribPointer(VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, 0, mSphereProjection.getLeftVertices(index));

    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    index = (mSphereProjection.getSlice() + 1) * 4 * line;
    glVertexAttribPointer(mTexCoordsArray, 2, GL_FLOAT, GL_FALSE, 0, mSphereProjection.getLeftTextCoords(index));

    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    //mMVPMatrix->loadIdentity();
    GLfloat *matrix = mMVPMatrix->getMatrix();
    glUniformMatrix4fv(mMVPMatrixHandle, 1, GL_FALSE, matrix);

    //mTexMatrix->loadIdentity();
    //matrix = mTexMatrix->getMatrix();

    glUniformMatrix4fv(mTexMatrixHandle, 1, GL_FALSE, matrix);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 * (mSphereProjection.getSlice() + 1) * (mSphereProjection.getStack() + 1));

    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    DEBUG("before swap buffer\n");
    eglSwapBuffers(mEglDisplay, mEglSurface);
    DEBUG("after swap buffer\n");

    return true;
}

void VrViewEglContext::createProgram() {

    ENTER1();
    GLint status = GL_TRUE;
    mVtxId = glCreateShader(GL_VERTEX_SHADER);
    ASSERT(mVtxId != 0);

    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glShaderSource(mVtxId, 1, (const char**)&vs, NULL);

    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glCompileShader(mVtxId);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glGetShaderiv(mVtxId, GL_COMPILE_STATUS, &status);
    if(!status)
        ASSERT(0 && "compile vertex shader failed");

    // fragment shader.
    ASSERT(!mFragId);

    mFragId = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(mFragId, 1, (const char**)&fs, NULL);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glCompileShader(mFragId);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glGetShaderiv(mFragId, GL_COMPILE_STATUS, &status);
    if(!status)
        ASSERT(0 && "compile fragment shader failed");

    // program.
    ASSERT(!mProgramId);
    mProgramId = glCreateProgram();
    ASSERT(mProgramId != 0);

    glAttachShader(mProgramId, mVtxId);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glAttachShader(mProgramId, mFragId);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glBindAttribLocation(mProgramId, VERTEX_POSITION, "position");
    glLinkProgram(mProgramId);
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    glGetProgramiv(mProgramId, GL_LINK_STATUS, &status);
    if(!status)
        ASSERT(0 && "link vertex and fragment shader failed");

    // sampler/tex handle
    ASSERT(mSamplerHandle == -1);
    mSamplerHandle = glGetUniformLocation(mProgramId, "sampler");
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());
    ASSERT(mSamplerHandle != -1);
    INFO("glGetUniformLocation(\"Sampler\") = %d\n",mSamplerHandle);

    ASSERT(mMVPMatrixHandle == -1);
    mMVPMatrixHandle = glGetUniformLocation(mProgramId, "MVPMatrix");
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    ASSERT(mTexMatrixHandle == -1);
    mTexMatrixHandle = glGetUniformLocation(mProgramId, "TexMatrix");
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    //mVerticesArray = glGetAttribLocation(mProgramId, "position");
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    mTexCoordsArray = glGetAttribLocation(mProgramId, "texCoordsArray");
    ASSERT(GLenum(GL_NO_ERROR) == glGetError());

    // init sphere vertices and texCoords
    mSphereProjection.initVerticesAndTextCoords(VrViewSphereProjection::MONO, VrViewSphereProjection::NONE);

    // projection
    float fovy = 75.0f;
    float zNear = 0.1f, zFar = 100.0f;
    float aspect = 16.0f/9.0f; // w/h

    float top = zNear * (float)tan(fovy * (PI / 360.0));
    float bottom = -top;
    float left = bottom * aspect;
    float right = top * aspect;

    mProjectionMatrix->setFrustumMatrix(left, right, bottom, top, zNear, zFar);

    EXIT1();
}

bool VrViewSphereProjection::initVerticesAndTextCoords(Display d, Stereo s) {

    float altitude;
    float altitudeDelta;
    float azimuth;
    float ex;
    float ey;
    float ez;
    float radius = 1.0f;

    float xOffset = 0.0f;
    float yOffset = 0.0f;
    float zOffset = 0.0f;

    // only implement mono display on mono content
    // two 3D vertices take 6 floats
    mLeftVertices = new float[6 * (mStack + 1) * (mSlice + 1)];

    // two 2D vertices take 4 floats
    mLeftTextCoords = new float [4 * (mStack + 1) * (mSlice + 1)];

    for(int i = 0; i <= mStack; i++) {
        altitude      = (float) (PI/2.0 -    i    * (PI) / mStack);
        altitudeDelta = (float) (PI/2.0 - (i + 1) * (PI) / mStack);

        float* vertices = &mLeftVertices[6 * (mStack + 1) * i];

        for(int j = 0; j <= mSlice; j++) {
            azimuth = (float)(j * (PI*2) / mSlice);

            ex = (float) (cos(altitude) * cos(azimuth));
            ey = (float)  sin(altitude);
            ez = (float) - (cos(altitude) * sin(azimuth));

            int idx = mSlice - j;

            vertices[6*idx+0] = radius * ex + xOffset;
            vertices[6*idx+1] = radius * ey + yOffset;
            vertices[6*idx+2] = radius * ez + zOffset;

            ex = (float) (cos(altitudeDelta) * cos(azimuth));
            ey = (float) sin(altitudeDelta);
            ez = (float) -(cos(altitudeDelta) * sin(azimuth));

            vertices[6*idx+3] = radius * ex + xOffset;
            vertices[6*idx+4] = radius * ey + yOffset;
            vertices[6*idx+5] = radius * ez + zOffset;
        }

    }

    for(int i = 0; i <= mStack; i++) {
        float* texCoords = &mLeftTextCoords[4 * (mStack + 1) * i];
        for(int j = 0; j <= mSlice; j++) {
            texCoords[4*j+0] = j/(float)mSlice;
            texCoords[4*j+1] = i/(float)mSlice;

            texCoords[4*j+2] = j/(float)mSlice;
            texCoords[4*j+3] = (i + 1) / (float)mSlice;
        }
    }

    return true;
}

const float* VrViewSphereProjection::getLeftVertices(int index) {
    if (index >= 6 * (mStack + 1) * (mSlice + 1))
        return NULL;

    return &mLeftVertices[index];
}

const float* VrViewSphereProjection::getLeftTextCoords(int index) {
    if (index >= 4 * (mStack + 1) * (mSlice + 1))
        return NULL;

    return &mLeftTextCoords[index];
}

const float* VrViewSphereProjection::getRightVertices(int index) {
    if (index >= 6 * (mStack + 1) * (mSlice + 1))
        return NULL;

    return &mRightVertices[index];
}

const float* VrViewSphereProjection::getRightTextCoords(int index) {
    if (index >= 4 * (mStack + 1) * (mSlice + 1))
        return NULL;

    return &mRightTextCoords[index];
}

} // end of namespace yvr
} // end of namespace yunos
