#ifndef __WINDOW_SURFACE_TEST_WINDOW_H__
#define __WINDOW_SURFACE_TEST_WINDOW_H__
#include "WaylandConnection.h"
#include "IInterface.h"
#include <assert.h>

#include "xdg-shell-client-protocol.h"
#define TYPE_APPLICATION 2

#include "multimedia/mm_surface_compat.h"

#ifndef __MM_YUNOS_LINUX_BSP_BUILD__
#include <SurfaceController.h>
WindowSurface* createWaylandWindow2(int width, int height);
void destroyWaylandWindow2(WindowSurface* surface);
yunos::libgui::SurfaceController* getSurfaceController(WindowSurface* surface);
std::shared_ptr<yunos::libgui::BufferPipeProducer>  getBQProducer(WindowSurface* surface);
#else
#include <WindowSurface.h>
Surface* createWaylandWindow(int width, int height);
void destroyWaylandWindow(Surface* surface);
#endif
/* - display, aka 'struct wl_display*' can be got by getWaylandDisplay(). the real WaylandConnecttion is maintained internally by static/global.
 * - surface, aka 'struct wl_surface*' can be got by WaylandConnection->compositor_create_surface()
 * -shell surface, aka 'struct wl_shell_surface*', it depends on TestWindow to do so, to set curren wl_surface as top level window
 */
void waylandDispatch();
struct wl_display* getWaylandDisplay();

class TestWindow : public IWindow
{
public:
    TestWindow(yunos::wpc::WaylandConnection* connection)
    {
        this->m_connection = connection;
        this->m_shell_surface = NULL;
        this->m_shell = NULL;
    }

    virtual ~TestWindow()
    {
        this->m_connection = NULL;

        if(this->m_shell_surface)
        {
            wl_shell_surface_destroy(this->m_shell_surface);
            this->m_shell_surface = NULL;
        }

        if(this->m_shell)
        {
            wl_shell_destroy(this->m_shell);
            this->m_shell = NULL;
        }
    }

    virtual void createShell(struct wl_surface *surface)
    {
        struct wl_registry* registry;
        registry = m_connection->get_wayland_registry();
        wl_registry_add_listener(registry, &registry_listener, this);
        this->m_connection->wayland_roundtrip(this->m_connection);

        assert(this->m_shell);

        if(this->m_shell && surface)
        {
            this->m_shell_surface =
                wl_shell_wm_get_shell_surface(this->m_shell, surface, 2008, 0, 0, 0);

            if(this->m_shell_surface)
            {
                wl_shell_surface_add_listener(this->m_shell_surface, &shell_surface_listener, this);
                wl_shell_surface_set_toplevel(this->m_shell_surface);
                wl_shell_surface_set_visibility(this->m_shell_surface, 1);
            }
            else
            {
                assert(0);
            }
        }
    }

    static void registry_handle_global(void *data, struct wl_registry *registry,
                                            uint32_t name, const char *interface, uint32_t version)
    {
        TestWindow* win = static_cast<TestWindow*>(data);

        // printf("window registry_handle_global\n");

        if(strcmp(interface, "wl_shell") == 0)
        {
            win->m_shell = static_cast<struct wl_shell*>(wl_registry_bind(registry, name,
                &wl_shell_interface, 1));
        }
    }

    static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
    {
    }

    static void HandleConfigure(void* data,struct wl_shell_surface* surface,
                    uint32_t edges,int32_t width,int32_t height)
    {
        printf("handleconfigure\n");
    }


    static void HandlePopupDone(void* data,struct wl_shell_surface* surface)
    {
        printf("handlepopudone\n");
    }

    static void HandlePing(void* data,struct wl_shell_surface* shell_surface,
                    uint32_t serial)
    {
        wl_shell_surface_pong(shell_surface, serial);
    }

    static const struct wl_registry_listener registry_listener;
    static const wl_shell_surface_listener shell_surface_listener;

private:
    yunos::wpc::WaylandConnection* m_connection;
    struct wl_shell* m_shell;
    struct wl_shell_surface* m_shell_surface;
};

#endif
