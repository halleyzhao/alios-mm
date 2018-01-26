/*
 * Copyright 2011 Benjamin Franzke
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

// no dependency on weston config.h, remove it
// #include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h> // for GL_TEXTURE_EXTERNAL_OES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "xdg-shell-client-protocol.h"
#include "mediacodecPlay.h"
/*
 when compile/link simple-egl with other c++ source files,
 there is error during link 'undefined xdg_shell_interface.
 it is strange, include the xdg-shell-protocol.c to avoid this
 */
#include "xdg-shell-protocol.c"

#define USE_GL_TEXTURE_EXTERNAL_OES     1
#if USE_GL_TEXTURE_EXTERNAL_OES
    #define GL_TEXTURE_TARGET_TYPE  GL_TEXTURE_EXTERNAL_OES
#else
    #define GL_TEXTURE_TARGET_TYPE  GL_TEXTURE_2D
#endif

#include "multimedia/mm_debug.h"
MM_LOG_DEFINE_MODULE_NAME("Simple-egl")
MM_LOG_DEFINE_LEVEL(MM_LOG_DEBUG)

#ifndef EGL_EXT_swap_buffers_with_damage
#define EGL_EXT_swap_buffers_with_damage 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
#endif

#ifndef EGL_EXT_buffer_age
#define EGL_EXT_buffer_age 1
#define EGL_BUFFER_AGE_EXT          0x313D
#endif

struct window;
struct seat;

struct display {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_shell *shell;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_touch *touch;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor *default_cursor;
    struct wl_surface *cursor_surface;
    struct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLConfig conf;
    } egl;
    struct window *window;

    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;
};

struct geometry {
    int width, height;
};

struct window {
    struct display *display;
    struct geometry geometry, window_size;
    struct {
        GLuint rotation_uniform;
        GLuint pos;
        GLuint tex;
        GLint  uniformTex[3];
    } gl;

    uint32_t benchmark_time, frames;
    struct wl_egl_window *native;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    EGLSurface egl_surface;
    struct wl_callback *callback;
    int fullscreen, opaque, buffer_size, frame_sync;
};

static const char *vert_shader_text =
    "uniform mat4 rotation;\n"
    "attribute vec4 pos;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   gl_Position = rotation * pos;\n"
    "   v_texcoord  = texcoord;\n"
    "}\n";

static const char *frag_shader_text =
#if USE_GL_TEXTURE_EXTERNAL_OES
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES tex0;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   gl_FragColor = texture2D(tex0, v_texcoord);\n"
    "   gl_FragColor.a = 1.0;\n"
    "}\n";
#else
    "precision mediump float;\n"
    "uniform sampler2D tex0;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   gl_FragColor = texture2D(tex0, v_texcoord);\n"
    "   gl_FragColor.a = 1.0;\n"
    "}\n";
#endif
static int running = 1;
extern int g_exit_after_draw_times;
static GLuint g_texture_id = 0;
#define VIDEO_TEXTURE_COUNT     1
static int g_with_rotation = 1;

static GLuint
createTestTexture( )
{
    GLuint textureId;
    // 2x2 Image, 4 bytes per pixel (R, G, B, A)
    GLubyte pixels[4 * 4] =
    {
      255,   0,   0, 255,   // Red
        0, 255,   0, 255,   // Green
        0,   0, 255, 255,   // Blue
      255, 255, 255, 255,   // White
    };

    glGenTextures(1, &textureId );
    glBindTexture(GL_TEXTURE_TARGET_TYPE, textureId);
    glTexImage2D(GL_TEXTURE_TARGET_TYPE, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_TARGET_TYPE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_TARGET_TYPE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_texture_id = textureId;
    return textureId;
}

static void
init_egl(struct display *display, struct window *window)
{
    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    const char *extensions;

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint major, minor, n, count, i, size;
    EGLConfig *configs;
    EGLBoolean ret;

    if (window->opaque || window->buffer_size == 16)
        config_attribs[9] = 0;

    display->egl.dpy = eglGetDisplay(display->display);
    assert(display->egl.dpy);

    ret = eglInitialize(display->egl.dpy, &major, &minor);
    assert(ret == EGL_TRUE);
    ret = eglBindAPI(EGL_OPENGL_ES_API);
    assert(ret == EGL_TRUE);

    if (!eglGetConfigs(display->egl.dpy, NULL, 0, &count) || count < 1)
        assert(0);

    configs = calloc(count, sizeof *configs);
    assert(configs);

    ret = eglChooseConfig(display->egl.dpy, config_attribs,
                  configs, count, &n);
    assert(ret && n >= 1);

    for (i = 0; i < n; i++) {
        eglGetConfigAttrib(display->egl.dpy,
                   configs[i], EGL_BUFFER_SIZE, &size);
        if (window->buffer_size == size) {
            display->egl.conf = configs[i];
            break;
        }
    }
    free(configs);
    if (display->egl.conf == NULL) {
        ERROR("did not find config with buffer size %d\n",
            window->buffer_size);
        exit(EXIT_FAILURE);
    }

    display->egl.ctx = eglCreateContext(display->egl.dpy,
                        display->egl.conf,
                        EGL_NO_CONTEXT, context_attribs);
    assert(display->egl.ctx);

    display->swap_buffers_with_damage = NULL;
    extensions = eglQueryString(display->egl.dpy, EGL_EXTENSIONS);
    if (extensions &&
        strstr(extensions, "EGL_EXT_swap_buffers_with_damage") &&
        strstr(extensions, "EGL_EXT_buffer_age"))
        display->swap_buffers_with_damage =
            (PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
            eglGetProcAddress("eglSwapBuffersWithDamageEXT");

    if (display->swap_buffers_with_damage)
        WARNING("has EGL_EXT_buffer_age and EGL_EXT_swap_buffers_with_damage\n");

}

static void
fini_egl(struct display *display)
{
    eglTerminate(display->egl.dpy);
    eglReleaseThread();
}

static GLuint
create_shader(struct window *window, const char *source, GLenum shader_type)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(shader_type);
    assert(shader != 0);

    glShaderSource(shader, 1, (const char **) &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, 1000, &len, log);
        ERROR("Error: compiling %s: %*s\n",
            shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
            len, log);
        exit(1);
    }

    return shader;
}

static void
init_gl(struct window *window)
{
    GLuint frag, vert;
    GLuint program;
    GLint status;

    frag = create_shader(window, frag_shader_text, GL_FRAGMENT_SHADER);
    vert = create_shader(window, vert_shader_text, GL_VERTEX_SHADER);

    program = glCreateProgram();
    glAttachShader(program, frag);
    glAttachShader(program, vert);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(program, 1000, &len, log);
        ERROR("Error: linking:\n%*s\n", len, log);
        exit(1);
    }

    glUseProgram(program);

    window->gl.pos = 0;
    window->gl.tex = 1;
#if !USE_GL_TEXTURE_EXTERNAL_OES
    createTestTexture();
#else
    glGenTextures(1, &g_texture_id );
#endif
    glBindAttribLocation(program, window->gl.pos, "pos");
    glBindAttribLocation(program, window->gl.tex, "texcoord");
    window->gl.uniformTex[0] = glGetUniformLocation(program, "tex0");
    glLinkProgram(program);

    window->gl.rotation_uniform =
        glGetUniformLocation(program, "rotation");
}

static void
handle_surface_configure(void *data, struct xdg_surface *surface,
             int32_t width, int32_t height,
             struct wl_array *states, uint32_t serial)
{
    struct window *window = data;
    uint32_t *p;

    window->fullscreen = 0;
    wl_array_for_each(p, states) {
        uint32_t state = *p;
        switch (state) {
        case XDG_SURFACE_STATE_FULLSCREEN:
            window->fullscreen = 1;
            break;
        }
    }

    if (width > 0 && height > 0) {
        if (!window->fullscreen) {
            window->window_size.width = width;
            window->window_size.height = height;
        }
        window->geometry.width = width;
        window->geometry.height = height;
    } else if (!window->fullscreen) {
        window->geometry = window->window_size;
    }

    if (window->native)
        wl_egl_window_resize(window->native,
                     window->geometry.width,
                     window->geometry.height, 0, 0);

    // xdg_surface_ack_configure(surface, serial);
}

static void
handle_surface_delete(void *data, struct xdg_surface *xdg_surface)
{
    running = 0;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    handle_surface_configure,
    handle_surface_delete,
};

static void
create_surface(struct window *window)
{
    struct display *display = window->display;
    EGLBoolean ret;

    window->surface = wl_compositor_create_surface(display->compositor);
    window->xdg_surface = xdg_shell_get_xdg_surface(display->shell,
                            window->surface);

    xdg_surface_add_listener(window->xdg_surface,
                 &xdg_surface_listener, window);

    window->native =
        wl_egl_window_create(window->surface,
                     window->geometry.width,
                     window->geometry.height);
    window->egl_surface =
        eglCreateWindowSurface(display->egl.dpy,
                       display->egl.conf,
                       window->native, NULL);

    xdg_surface_set_title(window->xdg_surface, "simple-egl");

    ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
                 window->egl_surface, window->display->egl.ctx);
    assert(ret == EGL_TRUE);

    if (!window->frame_sync)
        eglSwapInterval(display->egl.dpy, 0);

    if (window->fullscreen)
        ; // xdg_surface_set_fullscreen(window->xdg_surface, NULL);
}

static void
destroy_surface(struct window *window)
{
    /* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
     * on eglReleaseThread(). */
    eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
               EGL_NO_CONTEXT);

    eglDestroySurface(window->display->egl.dpy, window->egl_surface);
    wl_egl_window_destroy(window->native);

    xdg_surface_destroy(window->xdg_surface);
    wl_surface_destroy(window->surface);

    if (window->callback)
        wl_callback_destroy(window->callback);
}

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
    struct window *window = data;
    struct display *display = window->display;
    static const GLfloat verts[4][2] = {
        { -0.9, -0.9 },
        {  0.9, -0.9 },
        {  0.9,  0.9 },
        { -0.9,  0.9 }
    };
    static const GLfloat texcoords[4][2] = {
        {  0.0f,  1.0f },
        {  1.0f,  1.0f },
        {  1.0f,  0.0f },
        {  0.0f,  0.0f }
    };
    GLfloat angle;
    GLfloat rotation[4][4] = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
    static const uint32_t speed_div = 5, benchmark_interval = 5;
    struct wl_region *region;
    EGLint rect[4];
    EGLint buffer_age = 0;
    struct timeval tv;

    wl_display_dispatch_pending(display->display);

    assert(window->callback == callback);
    window->callback = NULL;

    if (callback)
        wl_callback_destroy(callback);

    gettimeofday(&tv, NULL);
    time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (window->frames == 0)
        window->benchmark_time = time;
    if (time - window->benchmark_time > (benchmark_interval * 1000)) {
        DEBUG("%d frames in %d seconds: %f fps\n",
               window->frames,
               benchmark_interval,
               (float) window->frames / benchmark_interval);
        window->benchmark_time = time;
        window->frames = 0;
    }
    if (g_with_rotation) {
        angle = (time / speed_div) % 360 * M_PI / 180.0;
        rotation[0][0] =  cos(angle);
        rotation[0][2] =  sin(angle);
        rotation[2][0] = -sin(angle);
        rotation[2][2] =  cos(angle);
    }

    if (display->swap_buffers_with_damage)
        eglQuerySurface(display->egl.dpy, window->egl_surface,
                EGL_BUFFER_AGE_EXT, &buffer_age);

    glViewport(0, 0, window->geometry.width, window->geometry.height);

    glUniformMatrix4fv(window->gl.rotation_uniform, 1, GL_FALSE,
               (GLfloat *) rotation);

    glClearColor(0.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);

    glVertexAttribPointer(window->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(window->gl.tex, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glEnableVertexAttribArray(window->gl.pos);
    glEnableVertexAttribArray(window->gl.tex);

    int i;
    for (i = 0; i < VIDEO_TEXTURE_COUNT; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_TARGET_TYPE, g_texture_id);
        glUniform1i(window->gl.uniformTex[i], i);
    }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(window->gl.pos);
    glDisableVertexAttribArray(window->gl.tex);

    if (window->opaque || window->fullscreen) {
        region = wl_compositor_create_region(window->display->compositor);
        wl_region_add(region, 0, 0,
                  window->geometry.width,
                  window->geometry.height);
        wl_surface_set_opaque_region(window->surface, region);
        wl_region_destroy(region);
    } else {
        wl_surface_set_opaque_region(window->surface, NULL);
    }

    if (display->swap_buffers_with_damage && buffer_age > 0) {
        rect[0] = window->geometry.width / 4 - 1;
        rect[1] = window->geometry.height / 4 - 1;
        rect[2] = window->geometry.width / 2 + 2;
        rect[3] = window->geometry.height / 2 + 2;
        display->swap_buffers_with_damage(display->egl.dpy,
                          window->egl_surface,
                          rect, 1);
    } else {
        eglSwapBuffers(display->egl.dpy, window->egl_surface);
    }
    window->frames++;
}

void export_redraw(void* data)
{
    PRINTF("+");
    redraw(data, NULL, 0);
}

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
             uint32_t serial, struct wl_surface *surface,
             wl_fixed_t sx, wl_fixed_t sy)
{
    struct display *display = data;
    struct wl_buffer *buffer;
    struct wl_cursor *cursor = display->default_cursor;
    struct wl_cursor_image *image;

    if (display->window->fullscreen)
        wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
    else if (cursor) {
        image = display->default_cursor->images[0];
        buffer = wl_cursor_image_get_buffer(image);
        if (!buffer)
            return;
        wl_pointer_set_cursor(pointer, serial,
                      display->cursor_surface,
                      image->hotspot_x,
                      image->hotspot_y);
        wl_surface_attach(display->cursor_surface, buffer, 0, 0);
        wl_surface_damage(display->cursor_surface, 0, 0,
                  image->width, image->height);
        wl_surface_commit(display->cursor_surface);
    }
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
             uint32_t serial, struct wl_surface *surface)
{
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
              uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
              uint32_t serial, uint32_t time, uint32_t button,
              uint32_t state)
{
    struct display *display = data;

    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
        xdg_surface_move(display->window->xdg_surface,
                 display->seat, serial);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
            uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void
touch_handle_down(void *data, struct wl_touch *wl_touch,
          uint32_t serial, uint32_t time, struct wl_surface *surface,
          int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    struct display *d = (struct display *)data;

    xdg_surface_move(d->window->xdg_surface, d->seat, serial);
}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch,
        uint32_t serial, uint32_t time, int32_t id)
{
}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch,
            uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
}

static const struct wl_touch_listener touch_listener = {
    touch_handle_down,
    touch_handle_up,
    touch_handle_motion,
    touch_handle_frame,
    touch_handle_cancel,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
               uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
              uint32_t serial, struct wl_surface *surface,
              struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
              uint32_t serial, struct wl_surface *surface)
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
            uint32_t serial, uint32_t time, uint32_t key,
            uint32_t state)
{
    struct display *d = data;
        DEBUG("keyboard_handle_key key:%d , state:%d \n", key,state );
    if (key == KEY_F11 && state) {
        if (d->window->fullscreen)
            ; // xdg_surface_unset_fullscreen(d->window->xdg_surface);
        else
            ; // xdg_surface_set_fullscreen(d->window->xdg_surface, NULL);
    } else if (key == KEY_ESC && state)
        running = 0;
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
              uint32_t serial, uint32_t mods_depressed,
              uint32_t mods_latched, uint32_t mods_locked,
              uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
             enum wl_seat_capability caps)
{
    struct display *d = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
        d->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(d->pointer, &pointer_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
        wl_pointer_destroy(d->pointer);
        d->pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
        d->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
        wl_keyboard_destroy(d->keyboard);
        d->keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !d->touch) {
        d->touch = wl_seat_get_touch(seat);
        wl_touch_set_user_data(d->touch, d);
        wl_touch_add_listener(d->touch, &touch_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && d->touch) {
        wl_touch_destroy(d->touch);
        d->touch = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

static void
xdg_shell_ping(void *data, struct xdg_shell *shell, uint32_t serial)
{
    xdg_shell_pong(shell, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
    xdg_shell_ping,
};

#define XDG_VERSION 4 /* The version of xdg-shell that we implement */
#ifdef static_assert
static_assert(XDG_VERSION == XDG_SHELL_VERSION_CURRENT,
          "Interface version doesn't match implementation version");
#endif

static void
registry_handle_global(void *data, struct wl_registry *registry,
               uint32_t name, const char *interface, uint32_t version)
{
    struct display *d = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor =
            wl_registry_bind(registry, name,
                     &wl_compositor_interface, 1);
    } else if (strcmp(interface, "xdg_shell") == 0) {
        d->shell = wl_registry_bind(registry, name,
                        &xdg_shell_interface, 1);
        xdg_shell_add_listener(d->shell, &xdg_shell_listener, d);
        xdg_shell_use_unstable_version(d->shell, XDG_VERSION);
    } else if (strcmp(interface, "wl_seat") == 0) {
        d->seat = wl_registry_bind(registry, name,
                       &wl_seat_interface, 1);
        wl_seat_add_listener(d->seat, &seat_listener, d);
    } else if (strcmp(interface, "wl_shm") == 0) {
        d->shm = wl_registry_bind(registry, name,
                      &wl_shm_interface, 1);
        d->cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
        if (!d->cursor_theme) {
            fprintf(stderr, "unable to load default theme\n");
            return;
        }
        d->default_cursor =
            wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
        if (!d->default_cursor) {
            ERROR("unable to load default left pointer\n");
            // TODO: abort ?
        }
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
                  uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void
signal_int(int signum)
{
    running = 0;
}

static void
usage(int error_code)
{
    PRINTF("Usage: simple-egl [OPTIONS]\n\n"
        "  -f\tRun in fullscreen mode\n"
        "  -o\tCreate an opaque surface\n"
        "  -s\tUse a 16 bpp EGL config\n"
        "  -b\tDon't sync to compositor redraw (eglSwapInterval 0)\n"
        "  -m\tpath to media content\n"
        "  -n\tquit after draw # times\n"
        "  -w\twindow width\n"
        "  -h\twindow height\n"
        "  -r\tdisable roration if 0 is given\n"
        "  -?\tThis help text\n\n");

    exit(error_code);
}

int
main(int argc, char **argv)
{
    struct sigaction sigint;
    struct display display = { 0 };
    struct window  window  = { 0 };
    int i;//, ret = 0;
    const char* media_file_name = NULL;

    window.display = &display;
    display.window = &window;
    window.geometry.width  = 1920;
    window.geometry.height = 1080;
    window.buffer_size = 32;
    window.frame_sync = 1;
    setenv("HYBRIS_EGLPLATFORM", MP_HYBRIS_EGLPLATFORM,1);
    mm_log_set_level(LOG_LEVEL);

    for (i = 1; i < argc; i++) {
        if (strcmp("-f", argv[i]) == 0)
            window.fullscreen = 1;
        else if (strcmp("-o", argv[i]) == 0)
            window.opaque = 1;
        else if (strcmp("-s", argv[i]) == 0)
            window.buffer_size = 16;
        else if (strcmp("-b", argv[i]) == 0)
            window.frame_sync = 0;
        else if (strcmp("-n", argv[i]) == 0)
            g_exit_after_draw_times = atoi(argv[++i]);
        else if (strcmp("-m", argv[i]) == 0)
            media_file_name = argv[++i];
        else if (strcmp("-w", argv[i]) == 0)
            window.geometry.width  = atoi(argv[++i]);
        else if (strcmp("-h", argv[i]) == 0)
            window.geometry.height = atoi(argv[++i]);
        else if (strcmp("-r", argv[i]) == 0)
            g_with_rotation = atoi(argv[++i]);
        else if (strcmp("-?", argv[i]) == 0)
            usage(EXIT_SUCCESS);
        else
            usage(EXIT_FAILURE);
    }

    window.window_size = window.geometry;
    display.display = wl_display_connect(NULL);
    assert(display.display);

    display.registry = wl_display_get_registry(display.display);
    wl_registry_add_listener(display.registry,
                 &registry_listener, &display);

    wl_display_dispatch(display.display);

    init_egl(&display, &window);
    create_surface(&window);
    init_gl(&window);

    if (display.compositor)
        display.cursor_surface =
            wl_compositor_create_surface(display.compositor);

    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);

#if 0
    /* The mainloop here is a little subtle.  Redrawing will cause
     * EGL to read events so we can just call
     * wl_display_dispatch_pending() to handle any events that got
     * queued up as a side effect. */
    while (running && ret != -1) {
        redraw(&window, NULL, 0);
        if(g_exit_after_draw_times-- <=0)
            running = 0;
    }
#else
    playVideoTexture(media_file_name, g_texture_id, export_redraw, &window, NULL, NULL);
    // make sure redraw call wl_display_dispatch_pending();
#endif
    PRINTF("simple-egl exiting\n");

    destroy_surface(&window);
    fini_egl(&display);

    wl_surface_destroy(display.cursor_surface);
    if (display.cursor_theme)
        wl_cursor_theme_destroy(display.cursor_theme);

    if (display.shell)
        xdg_shell_destroy(display.shell);

    if (display.compositor)
        wl_compositor_destroy(display.compositor);

    wl_registry_destroy(display.registry);
    wl_display_flush(display.display);
    wl_display_disconnect(display.display);

    return 0;
}
