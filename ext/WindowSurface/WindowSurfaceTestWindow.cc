// #include "config.h"
#include "WindowSurfaceTestWindow.h"
#include <vector>
#include <string>
#include <map>
#include <stdio.h>
#include "multimedia/mm_cpp_utils.h"
#include "multimedia/mm_debug.h"
#if defined(__MM_YUNOS_CNTRHAL_BUILD__)  || defined(__MM_YUNOS_YUNHAL_BUILD__) // for raw BufferQueue test
    #include <memory/MemoryUtils.h>
    #include <cutils/graphics.h>
#endif
#if defined(YUNOS_BOARD_intel)
    // #include <yalloc_drm_formats.h>
#endif

MM_LOG_DEFINE_MODULE_NAME("MM_SURFACE");
using namespace yunos::wpc;
#define FUNC_TRACK() FuncTracker tracker(MM_LOG_TAG, __FUNCTION__, __LINE__)
// #define FUNC_TRACK()

const struct wl_registry_listener TestWindow::registry_listener = {
        TestWindow::registry_handle_global,
        TestWindow::registry_handle_global_remove
};

const wl_shell_surface_listener TestWindow::shell_surface_listener = {
        TestWindow::HandlePing,
        TestWindow::HandleConfigure,
        TestWindow::HandlePopupDone
};

static WaylandConnection * g_conn = NULL;
static TestWindow * g_window = NULL;
// static wl_surface* g_wl_surface = NULL;
static bool g_useNestedSurface = false;

static void destroyWaylandConnection() __attribute__((destructor(201)));
static void destroyWaylandConnection() {
    FUNC_TRACK();
    if (g_window) {
        delete g_window;
        g_window = NULL;
    }
#if 1
        fprintf(stderr, "FIXME: simple return to avoid co-re du-mp (%s, %d)\n", __func__, __LINE__);
        return;
#endif

    if (g_conn) {
        g_conn->dec_ref();
        g_conn = NULL;
    }
    printf("%s, %d\n", __func__, __LINE__);
}

static bool ensureWaylandDisplay()
{
    FUNC_TRACK();

    if (!g_conn) {
        const char* displayName = NULL;
        std::string displayNameStr = YUNOS_MM::mm_get_env_str("mm.wayland.nested.display", "MM_WAYLAND_NESTED_DISPLAY");
        if(displayNameStr.size()) {
            g_useNestedSurface = true;
            displayName = displayNameStr.c_str();
        }

        if (displayName)
            DEBUG("displayName: %s", displayName);
        else
            DEBUG("displayName is NULL");
        g_conn = new WaylandConnection(displayName);
    }

    if (g_conn)
        return true;

    return false;
}
void waylandDispatch()
{
    g_conn->dispatch(false);
}

struct wl_display* getWaylandDisplay()
{
    if (!ensureWaylandDisplay())
        return NULL;
    return g_conn->get_wayland_display();
}
static bool ensureTestWindow()
{
    if (!g_window)
        g_window = new TestWindow(g_conn);

    if (!g_window)
        return false;

    return true;
}

#ifdef __MM_YUNOS_LINUX_BSP_BUILD__
Surface* createWaylandWindow(int width, int height)
{
    //g_conn = new WaylandConnection(NULL);
    FUNC_TRACK();
    if (!ensureWaylandDisplay())
        return NULL;

    g_window = new TestWindow(g_conn);
    Surface* win_surface = new Surface(g_conn, g_window, width, height);
    return win_surface;
}

void destroyWaylandWindow(Surface *surface)
{
    FUNC_TRACK();
    delete surface;
    delete g_window;
    g_conn->dec_ref();
    delete g_conn;
}
#else
using namespace yunos::libgui;

typedef struct {
    SurfaceController* controller;
    std::shared_ptr<BufferPipeProducer> producer;
    } SurfaceInfo;
typedef std::map<WindowSurface*, SurfaceInfo> SurfaceControllerMap;
static SurfaceControllerMap s_SurfaceAndControllerMap;
static YUNOS_MM::Lock s_SurfaceAndControllerMapLock;

WindowSurface* createWaylandWindow2(int width, int height)
{
    FUNC_TRACK();
    if (!ensureWaylandDisplay())
        return NULL;

    if (!g_window)
        g_window = new TestWindow(g_conn);

    ASSERT(g_window);
    DEBUG("Surface V2");
    SurfaceController *surfaceController= YUNOS_NEW(yunos::libgui::SurfaceController, g_conn, g_window, width, height);
    ASSERT(surfaceController != NULL);
    int ret = surfaceController->checkInit();
    ASSERT(ret);
    std::string socketName = surfaceController->getProducerSocketName().c_str();
    DEBUG("socketName = %s\n", socketName.c_str());

    std::shared_ptr<BufferPipeProducer> producer;
    WindowSurface* surface = NULL;
    bool use_bq_of_surface = false;
#if defined(__USING_SURFACE_FROM_BQ__)
    use_bq_of_surface = true;
#endif
    use_bq_of_surface = YUNOS_MM::mm_check_env_str("mm.use.bq.surface", "MM_USE_BQ_SURFACE", "1", use_bq_of_surface /* default value */);

    if (use_bq_of_surface) {
        producer = std::make_shared<BufferPipeProducer>(socketName.c_str());
        ASSERT(producer.get() != NULL);
        ret = producer->checkInit();
        ASSERT(ret);
        surface = new Surface(producer);
        INFO("create-surface-from-bq finally");
    }else {
        surface = YUNOS_NEW(yunos::libgui::Surface, socketName.c_str());
        INFO("NOT create-surface-from-bq finally");
    }

    surfaceController->resize(width, height);

    native_set_buffers_dimensions(surface, width, height);

    YUNOS_MM::MMAutoLock locker(s_SurfaceAndControllerMapLock);
    SurfaceInfo info;
    info.controller = surfaceController;
    info.producer = producer;
    s_SurfaceAndControllerMap[surface] = info;

    return surface;
}

std::shared_ptr<BufferPipeProducer>  getBQProducer(WindowSurface* surface)
{
    FUNC_TRACK();
    std::shared_ptr<BufferPipeProducer> producer;
    YUNOS_MM::MMAutoLock locker(s_SurfaceAndControllerMapLock);
    SurfaceControllerMap::iterator it = s_SurfaceAndControllerMap.find(surface);

    if (it != s_SurfaceAndControllerMap.end()) {
        producer = it->second.producer;
    }

    return producer;
}

SurfaceController* getSurfaceController(WindowSurface* surface)
{
    FUNC_TRACK();
    SurfaceController* surfaceController = NULL;
    YUNOS_MM::MMAutoLock locker(s_SurfaceAndControllerMapLock);
    SurfaceControllerMap::iterator it = s_SurfaceAndControllerMap.find(surface);

    if (it != s_SurfaceAndControllerMap.end()) {
        surfaceController = it->second.controller;
    }

    return surfaceController;
}

void destroyWaylandWindow2(WindowSurface *surface)
{
    FUNC_TRACK();
    if (!surface)
        return;
    surface->unrefMe();

    YUNOS_MM::MMAutoLock locker(s_SurfaceAndControllerMapLock);
    SurfaceControllerMap::iterator it = s_SurfaceAndControllerMap.find(surface);
    if (it != s_SurfaceAndControllerMap.end()) {
        SurfaceController* surfaceController  = it->second.controller;
        if (surfaceController) {
            YUNOS_DELETE(surfaceController);
            surfaceController = nullptr;
        }
        std::shared_ptr<BufferPipeProducer> producer = it->second.producer;
        if (producer) {
            // producer->disconnect(); // surface desconstruction will calls producer->disconnect()
            producer = nullptr;
        }
        s_SurfaceAndControllerMap.erase(it);
    }
    surface = nullptr;
}
#endif
