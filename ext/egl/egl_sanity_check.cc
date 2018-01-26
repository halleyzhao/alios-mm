#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>

#include <linux/input.h>
#include <wayland-egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <drm_fourcc.h>

#include <os-compatibility.h>
#include "native_surface_help.h"
#include <pthread.h>

#include <xf86drm.h>
#include <omap_drm.h>
#include <omap_drmif.h>
#include <gbm.h>
#include <libdce.h>
#include <WaylandConnection.h>

using namespace yunos::wpc;

#define ASSERT(expr) do {                       \
        if (!(expr)) {                          \
            printf("err %d\n", __LINE__);       \
            assert(0 && (expr));                \
        }                                       \
    } while(0)

struct omap_device *omapDevice = NULL;
struct gbm_device *gbmDevice = NULL;
struct gbm_surface *gbmSurface = NULL;
//static WaylandConnection * g_conn = NULL;
int drmFd = -1;

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    fprintf(stderr, "GL %s = %s\n", name, v);
}

static EGLDisplay eglDisplay = NULL;
static EGLConfig eglConfig = NULL; // choosed EGLConfig
static EGLConfig* configs;         // all possible EGLConfigs
static EGLContext eglContext;
static EGLSurface eglSurface;

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

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

static bool ensureEglGlobal()
{
    EGLint major, minor;
    EGLBoolean ret = EGL_FALSE;
    EGLint count = -1, n = 0, i = 0, size = 0;
    if (eglDisplay && eglConfig)
        return true;

    drmFd = dce_get_fd();
    if (drmFd < 0)
        drmFd = drmOpen("omapdrm", NULL);

    printf("drm fd is %d\n", drmFd);
    omapDevice = omap_device_new(drmFd);
    //gbmDevice = gbm_create_device(drmFd);
    //gbmSurface =  gbm_surface_create(gbmDevice, 960, 1280, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    //g_conn = new WaylandConnection(NULL);

    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    //eglDisplay = eglGetDisplay((EGLNativeDisplayType)gbmDevice);
    //eglDisplay = eglGetDisplay((EGLNativeDisplayType)g_conn->get_wayland_display());
    wl_display *disp = wl_display_connect(NULL);
    eglDisplay = eglGetDisplay((EGLNativeDisplayType)disp);
    ASSERT(eglDisplay != NULL);
    ret = eglInitialize(eglDisplay, &major, &minor);
    ASSERT(ret == EGL_TRUE);
    ret = eglBindAPI(EGL_OPENGL_ES_API);
    ASSERT(ret == EGL_TRUE);

    if (!eglGetConfigs(eglDisplay, NULL, 0, &count) || count < 1)
        return false;

    configs = (EGLConfig*)malloc(sizeof(EGLConfig) * count);
    memset(configs, 0, sizeof(EGLConfig) * count);

    ret = eglChooseConfig(eglDisplay, config_attribs, configs, count, &n);
    for (i = 0; i < n; i++) {
        eglGetConfigAttrib(eglDisplay, configs[i], EGL_BUFFER_SIZE, &size);
        if (32 == size) {
            eglConfig = configs[i];
            break;
        }
    }

    return true;
}

int main(int argc, char** argv) {
    if (!ensureEglGlobal())
        return 0;

    //EGLBoolean ret;
    //GLint status = GL_TRUE;

    eglContext = eglCreateContext(eglDisplay, eglConfig, NULL, context_attribs);
    //eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, (EGLNativeWindowType)gbmSurface, NULL);

    //ret = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    const char *extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    printf("\nEGL Extensions: %s\n", extensions);

    int width = 1408, height = 816, offset = 0;
    int pitch = width;
    struct omap_bo *bo = omap_bo_new(omapDevice, width * height *3 /2, OMAP_BO_WC);
    omap_bo_handle(bo);
    uint32_t name;
    omap_bo_get_name(bo, &name);
    int dfd = omap_bo_dmabuf(bo);
    printf("bo is %p, dfd is %d\n", bo, dfd);

#ifdef __USE_DMA_BUF__
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
    EGLImageKHR img = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
#else
    EGLint attribs[] = {
     EGL_GL_VIDEO_FOURCC_TI,      FOURCC('N','V','1','2'),
     EGL_GL_VIDEO_WIDTH_TI,       width,
     EGL_GL_VIDEO_HEIGHT_TI,      height,
     EGL_GL_VIDEO_BYTE_SIZE_TI,   (int)omap_bo_size(bo),
     EGL_GL_VIDEO_BYTE_STRIDE_TI, pitch,
     // TODO: pick proper YUV flags..
     EGL_GL_VIDEO_YUV_FLAGS_TI,   EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE | EGLIMAGE_FLAGS_YUV_BT601,
     EGL_NONE
    };
    EGLImageKHR img = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_RAW_VIDEO_TI_DMABUF, (EGLClientBuffer)dfd, attribs);
#endif
    EGLint err = eglGetError();
    printf("eglCreateImageKHR: err is %d, img is %p\n", err, img);
    return 0;
}
