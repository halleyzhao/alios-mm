/*
 * Copyright (C) 2017 Alibaba Group Holding Limited. All Rights Reserved.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <map>

#ifdef __MM_BUILD_VPU_SURFACE__
#define GL_GLEXT_PROTOTYPES
#endif

#include <linux/input.h>

#include <wayland-egl.h>

// #define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <vector>

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
#include <drm_fourcc.h>
#else
#include <NativeWindowBuffer.h>
#endif

#include <os-compatibility.h>
//#include "egl-helper.h"
#include "native_surface_help.h"
#include <pthread.h>
#include "multimedia/mm_debug.h"
#include "multimedia/mm_cpp_utils.h"

MM_LOG_DEFINE_MODULE_NAME("elg-texture")
#if 0
#undef DEBUG
#undef INFO
#undef WARNING
#undef ERROR
#define DEBUG(format, ...)  printf("[D] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define INFO(format, ...)  printf("[I] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define WARNING(format, ...)  printf("[W] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#define ERROR(format, ...)  printf("[E] %s, line: %d:" format "\n", __func__, __LINE__, ##__VA_ARGS__)
#endif

// FIXME, not necessary for us; no dedicate internal drawing thread
EGLBoolean running = EGL_TRUE;
bool td = false;

// FIXME, use the above screen resolution instead (do some calculation for rotated window)
#define WINDOW_WIDTH   720
#define WINDOW_HEIGHT  405

static void printGLString(const char *name, GLenum s) {
    // fprintf(stderr, "printGLString %s, %d\n", name, s);
    const char *v = (const char *) glGetString(s);
    // int error = glGetError();
    // fprintf(stderr, "glGetError() = %d, result of glGetString = %x\n", error,
    //        (unsigned int) v);
    // if ((v < (const char*) 0) || (v > (const char*) 0x10000))
    //    fprintf(stderr, "GL %s = %s\n", name, v);
    // else
    //    fprintf(stderr, "GL %s = (null) 0x%08x\n", name, (unsigned int) v);
    fprintf(stderr, "GL %s = %s\n", name, v);
}
#define CHECK_GL_ERROR() do {                   \
        int error = glGetError();               \
        if (error != GL_NO_ERROR)               \
            ERROR("glGetError() = %d", error);  \
    } while (0)

// ##################################################
// boilplate to draw video texture
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
#define SHADER(Src) #Src
static const char* vs =SHADER(
        attribute vec2 position;
        attribute vec2 texcoord;
        varying vec2 v_texcoord;
        void main() {
           gl_Position = vec4(position, 0.0, 1.0);
           v_texcoord = texcoord;
        }
        );

static const char* fs = "#extension GL_OES_EGL_image_external : require \n" SHADER(
        precision mediump float;
        varying vec2 v_texcoord;
        uniform samplerExternalOES sampler;
        void main() {
           gl_FragColor = texture2D(sampler, v_texcoord);
        }
        );
#undef SHADER

// #######################################################
// singleton/global context
static EGLDisplay eglDisplay = NULL;
static EGLConfig eglConfig = NULL; // choosed EGLConfig
static EGLConfig* configs;         // all possible EGLConfigs
#ifndef GL_GLEXT_PROTOTYPES
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
#endif

// create shader program for gles2
unsigned LoadShader(unsigned type, const std::string& source) {
    DEBUG();
    unsigned shader = glCreateShader(type);
    const char* src = source.data();
    if (shader) {
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);

        int compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            char logInfo[4096];
            glGetShaderInfoLog(shader, sizeof(logInfo), NULL, logInfo);
            ERROR("compile shader logInfo = %s\n", logInfo);
            ASSERT(0 && "compile fragment shader failed");
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

unsigned LoadProgram(const std::string& vertex_source, const std::string& fragment_source) {
    DEBUG();
    unsigned vertex_shader = LoadShader(GL_VERTEX_SHADER, vertex_source);
    unsigned fragment_shader = LoadShader(GL_FRAGMENT_SHADER, fragment_source);
    unsigned program = glCreateProgram();
    if (vertex_shader && fragment_shader && program) {
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        int linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            ASSERT(0 && "link shader program failed");
            glDeleteProgram(program);
            program = 0;
        }
    }
    if (vertex_shader)
        glDeleteShader(vertex_shader);
    if (fragment_shader)
        glDeleteShader(fragment_shader);

    DEBUG();
    return program;
}

// destroyEgl() should be done before wayland destroyConnection
static void destroyEglGlobal() __attribute__((destructor(202)));
static void destroyEglGlobal() {
    if (!eglDisplay)
        return;

    printf("%s, about to call elgTerminate\n", __func__);
    eglTerminate(eglDisplay);
    free(configs);
    configs = NULL;
    eglDisplay= NULL;
    eglConfig = NULL;
    printf("%s, done\n", __func__);
}

// init the global env for all surface/eglContext
static bool ensureEglGlobal()
{
    EGLint major, minor;
    EGLBoolean ret = EGL_FALSE;
    EGLint count = -1, n = 0, i = 0, size = 0;
    if (eglDisplay && eglConfig)
        return true;

    ASSERT(!eglDisplay && !eglConfig);

#ifndef GL_GLEXT_PROTOTYPES
    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    ASSERT(eglCreateImageKHR != NULL);
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    ASSERT(eglDestroyImageKHR != NULL);
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    ASSERT(glEGLImageTargetTexture2DOES != NULL);
#endif

#ifdef YUNOS_ENABLE_UNIFIED_SURFACE
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#elif PLUGIN_HAL
    eglDisplay = eglGetDisplay(getWaylandDisplay());
#else
    //eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglDisplay = eglGetDisplay((EGLNativeDisplayType)getWaylandDisplay());
    //eglDisplay = eglGetDisplay((EGLNativeDisplayType)gOMAPDevice);
#endif

    ASSERT(eglDisplay != NULL);
    ret = eglInitialize(eglDisplay, &major, &minor);
    ASSERT(ret == EGL_TRUE);

    ret = eglBindAPI(EGL_OPENGL_ES_API);
    ASSERT(ret == EGL_TRUE);

    if (!eglGetConfigs(eglDisplay, NULL, 0, &count) || count < 1)
        ASSERT(0);

    configs = (EGLConfig*)malloc(sizeof(EGLConfig) * count);
    ASSERT(configs);
    memset(configs, 0, sizeof(EGLConfig) * count);

    ret = eglChooseConfig(eglDisplay, config_attribs, configs, count, &n);
    ASSERT(ret && n >= 1);

    for (i = 0; i < n; i++) {
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_BUFFER_SIZE, &size);
        if (32 == size) {
            eglConfig = configs[i];
            break;
        }
    }

    DEBUG("ensureEglGlobal done");
    return true;
}

// ################################################################
// EGL context per window
class EglWindowContext {
    #define MAX_BUFFER_NUM 32
  public:
    EglWindowContext(int width, int height, int x, int y, int win)
        : mWidth(width), mHeight (height), mX(x), mY(y), mWin(win)
        , mWindowSurface(NULL), mEglSurface(EGL_NO_SURFACE), mEglContext(EGL_NO_CONTEXT)
        , mProgramId(0), mVertexPointerHandle(-1), mVertexTextureHandle(-1), mSamplerHandle(-1)
        , mCurrTexId(0), mUseSingleTexture(true), mForceDump(false) {

        int i=0;
        for (i=0; i<MAX_BUFFER_NUM; i++) {
            mBufferInfo[i].anb = NULL;
            mBufferInfo[i].texId = 0;
            mBufferInfo[i].img = EGL_NO_IMAGE_KHR;
        }
        mForceDump = YUNOS_MM::mm_check_env_str("mm.mc.force.dump.output", NULL, "1");
        mUseSingleTexture = YUNOS_MM::mm_check_env_str("mm.test.use.single.texture", "MM_TEST_USE_SINGLE_TEXTURE", "1", true);
    }
    // destruction doesn't run in waylang/egl thread, there will be crash if we delete WindowSurface here
    // now we destroy WindowSurface in egl thread, the unique one as player's listener thread (for the last loop upon MSG_STOPPED)
    ~EglWindowContext() { /* deinit(true); */ }

    bool init();
    bool deinit(bool destroyWindow);
    bool drawBuffer(MMNativeBuffer* anb);
    int win() { return mWin; }
    WindowSurface* windowSurface() { return mWindowSurface; }

  private:
    int32_t mWidth;
    int32_t mHeight;
    int32_t mX;
    int32_t mY;
    int mWin; // index
    WindowSurface* mWindowSurface;
    EGLSurface mEglSurface;
    EGLContext mEglContext;
    GLuint mProgramId;
    GLint mVertexPointerHandle;
    GLint mVertexTextureHandle;
    GLint mSamplerHandle;

    typedef struct{
        MMNativeBuffer* anb;
        GLuint texId;
        EGLImageKHR img;
    }  BufferInfo;
    BufferInfo mBufferInfo[MAX_BUFFER_NUM ];
    GLuint mCurrTexId;
    bool mUseSingleTexture;
    bool mForceDump;

    bool updateTexture(MMNativeBuffer* anb);
};

bool EglWindowContext::init() {
    if (!ensureEglGlobal())
        return false;

    EGLBoolean ret;

    if (!mWindowSurface) {
        mWindowSurface = createSimpleSurface(mWidth, mHeight);
        DEBUG("create WindowSurface: %p", mWindowSurface);
        mEglContext = eglCreateContext(eglDisplay, eglConfig, NULL, context_attribs);
        mEglSurface = eglCreateWindowSurface(eglDisplay, eglConfig,
                (EGLNativeWindowType)mWindowSurface->get_wl_egl_window(), NULL);
    }
    ASSERT(mWindowSurface && mEglContext != EGL_NO_CONTEXT && mEglSurface != EGL_NO_SURFACE);

    ret = eglMakeCurrent(eglDisplay, mEglSurface, mEglSurface, mEglContext);
    ASSERT(ret == EGL_TRUE);

    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    const char *extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    printf("EGL Extensions: %s\n", extensions);

    if (!mProgramId) {
        mProgramId = LoadProgram(vs, fs);
        ASSERT(mProgramId);
        glUseProgram(mProgramId);
        // sampler/tex handle
        ASSERT(mSamplerHandle == -1);
        mSamplerHandle = glGetUniformLocation(mProgramId, "sampler");
        INFO("glGetUniformLocation(\"Sampler\") = %d\n",mSamplerHandle);

        mVertexPointerHandle = glGetAttribLocation(mProgramId, "position");
        mVertexTextureHandle = glGetAttribLocation(mProgramId, "texcoord");
        ASSERT(GLenum(GL_NO_ERROR) == glGetError());
        ASSERT(mVertexPointerHandle != -1 && mVertexTextureHandle != -1 && mSamplerHandle != -1);
    }

    glViewport(0, 0, mWidth, mHeight);
    //mWindowSurface->setSurfaceResize(mWidth, mHeight);
    //native_set_buffers_offset(mWindowSurface, mX, mY);
#ifndef __MM_YUNOS_LINUX_BSP_BUILD__
    ((yunos::libgui::Surface*)mWindowSurface)->setOffset(mX, mY);
#else
    ((Surface*)mWindowSurface)->setOffset(mX, mY);
#endif

    if (mUseSingleTexture)
        glGenTextures(1, &mCurrTexId);

    INFO("init egl texture done\n");
    return true;

}
bool EglWindowContext::deinit(bool destroyWindow) {
    DEBUG("destroyWindow: %d", destroyWindow);

    if (mUseSingleTexture && mCurrTexId)
        glDeleteTextures(1, &mCurrTexId);

    int i = 0;
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mBufferInfo[i].anb == NULL)
            break;
        if (mBufferInfo[i].texId)
            glDeleteTextures(1, &mBufferInfo[i].texId);
        eglDestroyImageKHR(eglDisplay, mBufferInfo[i].img);

        mBufferInfo[i].anb = NULL;
        mBufferInfo[i].texId = 0;
        mBufferInfo[i].img = EGL_NO_IMAGE_KHR;
    }

    if (!destroyWindow) {
        return true;
    }

    // Free GL resource.
    if (mProgramId) {
        glDeleteProgram(mProgramId);
        mProgramId = 0;
        mVertexPointerHandle = -1;
        mVertexTextureHandle = -1;
        mSamplerHandle = -1;
    }

    if (mWindowSurface) {
        ASSERT(eglDisplay != EGL_NO_DISPLAY && mEglContext != EGL_NO_CONTEXT && mEglSurface != EGL_NO_SURFACE);
        eglDestroyContext(eglDisplay, mEglContext);
        eglDestroySurface(eglDisplay, mEglSurface);
        eglMakeCurrent(eglDisplay, NULL, NULL, NULL);
        destroySimpleSurface(mWindowSurface);

        mEglContext = EGL_NO_CONTEXT;
        mEglSurface = EGL_NO_SURFACE;
        mWindowSurface = NULL;
    }

    return true;
}

bool EglWindowContext::updateTexture(MMNativeBuffer* anb)
{
    int i;
    bool needBindTexture = false;

    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mBufferInfo[i].anb == anb)
            break;

        if (mBufferInfo[i].anb == NULL) {
            EGLClientBuffer clientBuffer = (EGLClientBuffer)anb;
            EGLint *tmp = NULL;
        #ifdef PLUGIN_HAL
            EGLenum target = 0x3140;
        #elif defined (__MM_YUNOS_YUNHAL_BUILD__)
           EGLenum target = EGL_NATIVE_BUFFER_YUNOS;
        #elif defined(__MM_BUILD_DRM_SURFACE__) //__MM_YUNOS_LINUX_BSP_BUILD__
            /*
            EGLenum target = EGL_WAYLAND_BUFFER_WL;
            EGLint attribs[3];
            attribs[0] = EGL_WAYLAND_PLANE_WL;
            attribs[1] = 0;
            attribs[2] = EGL_NONE;
            tmp = attribs;
            clientBuffer = (EGLClientBuffer)anb->wl_buf;
            */
            clientBuffer = NULL;

            //int dfd = omap_bo_dmabuf(anb->bo[0]);
            //INFO("dfd %d fd[0] %d", dfd, anb->fd[0]);
            int dfd = anb->fd[0];
            #if 1
            #if 0
            EGLenum target = EGL_LINUX_DMA_BUF_EXT;
            EGLint attribs[] = {
              EGL_WIDTH, (int)anb->width,
              EGL_HEIGHT, (int)anb->height,
              EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,
              EGL_DMA_BUF_PLANE0_FD_EXT, dfd,
              EGL_DMA_BUF_PLANE0_OFFSET_EXT, anb->offsets[0],
              EGL_DMA_BUF_PLANE0_PITCH_EXT, anb->pitches[0],
              EGL_DMA_BUF_PLANE1_FD_EXT, dfd,
              EGL_DMA_BUF_PLANE1_OFFSET_EXT, anb->offsets[0],
              EGL_DMA_BUF_PLANE1_PITCH_EXT, anb->pitches[0],
              EGL_NONE
            };
            #else
            clientBuffer = (EGLClientBuffer)dfd;
            EGLenum target = EGL_RAW_VIDEO_TI_DMABUF;
            EGLint attribs[] = {
             EGL_GL_VIDEO_FOURCC_TI,      FOURCC('N','V','1','2'),
             EGL_GL_VIDEO_WIDTH_TI,       (int)anb->width,
             EGL_GL_VIDEO_HEIGHT_TI,      (int)anb->height,
             EGL_GL_VIDEO_BYTE_SIZE_TI,   anb->size, //(int)omap_bo_size(anb->bo[0]),
             EGL_GL_VIDEO_BYTE_STRIDE_TI, anb->pitches[0],
             // TODO: pick proper YUV flags..
             EGL_GL_VIDEO_YUV_FLAGS_TI,   EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE | EGLIMAGE_FLAGS_YUV_BT601,
             EGL_NONE
            };
            INFO("raw video ti dmabuf");

            #endif
            #else

            struct omap_device *device = (struct omap_device*)dce_init();
            int width = 512, height = 512, offset = 0;
            int pitch = width;
            struct omap_bo *bo = NULL;
            bo = omap_bo_new(device, width * height *3 /2, OMAP_BO_WC);
            omap_bo_handle(bo);
            INFO("bo is %p", bo);
            dfd = omap_bo_dmabuf(bo);

            EGLint attribs[] = {
              EGL_WIDTH, width,
              EGL_HEIGHT, height,
              EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,
              EGL_DMA_BUF_PLANE0_FD_EXT, dfd,
              EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
              EGL_DMA_BUF_PLANE0_PITCH_EXT, pitch,
              EGL_DMA_BUF_PLANE1_FD_EXT, dfd,
              EGL_DMA_BUF_PLANE1_OFFSET_EXT, offset,
              EGL_DMA_BUF_PLANE1_PITCH_EXT, pitch,
              EGL_NONE
            };
            #endif
            INFO("%dx%d, fd %d, offset %d, pitch %d",
                    anb->width, anb->height, anb->fd[0], anb->offsets[0], anb->pitches[0]);
            tmp = attribs;
            //mBufferInfo[i].img = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        #elif defined(__MM_BUILD_VPU_SURFACE__)
            EGLenum target = 0;
        #endif
            mBufferInfo[i].img = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, target, clientBuffer, tmp);
            EGLint err = eglGetError();
            INFO("err is %d, img is %p", err, mBufferInfo[i].img);
            ASSERT(mBufferInfo[i].img != EGL_NO_IMAGE_KHR);

            // increase ref count; since eglDestroyImageKHR will decrease one refcount for mali driver
            VERBOSE("(%s, %d): eglCreateImageKHR\n", __func__, __LINE__);

            if (!mUseSingleTexture)
                glGenTextures(1, &mBufferInfo[i].texId);

            mBufferInfo[i].anb = anb;
            needBindTexture = true;
            break;
        }
    }

    if (i >= MAX_BUFFER_NUM) {
        ERROR("run out of buffer, mBufferInfo is full\n");
        return false;
    }

    if (!mUseSingleTexture)
        mCurrTexId = mBufferInfo[i].texId;
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mCurrTexId);

#ifdef __PHONE_BOARD_MTK__
    needBindTexture = true;
#endif
    if (needBindTexture || mUseSingleTexture)
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, mBufferInfo[i].img);

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES,GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    INFO("bind texture[%d]: %d for anb %p\n", i, mCurrTexId, anb);
    return true;
}

static void dumpTexBuffer(MMNativeBuffer* anb) {

#ifndef __MM_YUNOS_LINUX_BSP_BUILD__
    NativeWindowBuffer *bnb = static_cast<NativeWindowBuffer*>(anb);
    void *ptr = NULL;
    if (bnb) {
        Rect rect;
        rect.top = 0;
        rect.left = 0;
        rect.right = anb->width;
        rect.bottom = anb->height;
        bnb->lock(ALLOC_USAGE_PREF(SW_READ_OFTEN), rect, &ptr);
    }

    if (!ptr) {
        INFO("fail to map buffer");
        return;
    }

    static FILE *fp = NULL;
    if (!fp) {
        char name[128];
        sprintf(name, "/data/tex_dump_%dx%d.YUV420", anb->stride, anb->height);
        fp = fopen(name, "wb");
    }

    if (fp) {
        fwrite(ptr, anb->stride, anb->height, fp);
        fwrite(ptr, anb->stride, anb->height / 2, fp);
    }

    if (bnb)
        bnb->unlock();
#endif
}

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
void calculateTexCoord(MMNativeBuffer* anb, GLfloat texcoords[][2]) {

    texcoords[0][0] = anb->cropX*1.0f / anb->width;
    texcoords[0][1] = anb->cropY*1.0f / anb->height;

    texcoords[1][0] = texcoords[0][0];
    texcoords[1][1] = (anb->cropY + anb->cropH) * 1.0f / anb->height;

    texcoords[2][0] = (anb->cropX + anb->cropW) * 1.0f / anb->width;
    texcoords[2][1] = (anb->cropY + anb->cropH) * 1.0f / anb->height;

    texcoords[3][0] = (anb->cropX + anb->cropW) * 1.0f / anb->width;
    texcoords[3][1] = texcoords[0][1];
}
#endif

bool EglWindowContext::drawBuffer(MMNativeBuffer* anb)
{
    if (! updateTexture(anb))
        return false;

    glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static const GLfloat vtx[] = {
        -1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f};
    static GLfloat texcoords[4][2] = {
        {  0.0f,  0.0f },
        {  0.0f,  1.0f },
        {  1.0f,  1.0f },
        {  1.0f,  0.0f },
    };

    glUseProgram(mProgramId);
    glVertexAttribPointer(mVertexPointerHandle, 2, GL_FLOAT, GL_FALSE, 0, vtx);
    glEnableVertexAttribArray(mVertexPointerHandle);
    glEnableVertexAttribArray(mVertexTextureHandle);
#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
    calculateTexCoord(anb, texcoords);
#endif
    glVertexAttribPointer(mVertexTextureHandle, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glUniform1i(mSamplerHandle, 0);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (mForceDump)
        dumpTexBuffer(anb);

    VERBOSE("before swap buffer\n");
    eglSwapBuffers(eglDisplay, mEglSurface);
    VERBOSE("after swap buffer\n");

    return true;
}


// ##################################################
// global interface to external
using namespace YUNOS_MM;
typedef MMSharedPtr< EglWindowContext > EglWindowContextPtr;
// to do replace win as mWindowSurface
typedef std::map<int /* win */, EglWindowContextPtr> EglWindowContextMap;
static EglWindowContextMap s_eglWindowContextMap;
static YUNOS_MM::Lock s_contextMapLock;

EglWindowContextPtr _findEglWindowContextFromWin(int win)
{
    EglWindowContextPtr eglWindowContext;
    YUNOS_MM::MMAutoLock locker(s_contextMapLock);
    EglWindowContextMap::iterator it = s_eglWindowContextMap.find(win);

    if (it != s_eglWindowContextMap.end()) {
        eglWindowContext = it->second;
        ASSERT(win == eglWindowContext->win());
    }

    return eglWindowContext;
}

void _eraseEglWindowContextFromWin(int win)
{
    YUNOS_MM::MMAutoLock locker(s_contextMapLock);
    EglWindowContextMap::iterator it = s_eglWindowContextMap.find(win);

    if (it != s_eglWindowContextMap.end()) {
        s_eglWindowContextMap.erase(it);
    }
}

WindowSurface* initEgl(int width, int height, int x, int y, int win)
{
    EglWindowContextPtr eglWindowContext = _findEglWindowContextFromWin(win);

    if (!eglWindowContext) {
        YUNOS_MM::MMAutoLock locker(s_contextMapLock);
        eglWindowContext.reset(new EglWindowContext(width, height, x, y, win));
        s_eglWindowContextMap[win] = eglWindowContext;
    }
    ASSERT(eglWindowContext);
    bool ret = eglWindowContext->init();
    ASSERT(ret);

    return eglWindowContext->windowSurface();

}

void eglUninit(int win, bool destroySurface)
{
    EglWindowContextPtr eglWindowContext = _findEglWindowContextFromWin(win);

    ASSERT(eglWindowContext);
    bool ret = eglWindowContext->deinit(destroySurface);
    ASSERT(ret);
    _eraseEglWindowContextFromWin(win);
    // since player always uses index number to initEgl/eglUninit, it is not important to remove the EglWindowContextPtr from map

    {
        YUNOS_MM::MMAutoLock locker(s_contextMapLock);
        if (s_eglWindowContextMap.empty() && destroySurface)
            destroyEglGlobal();
    }
}


bool drawBuffer(MMNativeBuffer* anb, int win)
{
    EglWindowContextPtr eglWindowContext = _findEglWindowContextFromWin(win);
    ASSERT(eglWindowContext);
    bool ret = eglWindowContext->drawBuffer(anb);
    ASSERT(ret);
    return ret;
}
