#include "interfaces.h"

#include <cstdio>
#include <glib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-server.h>

namespace {

struct Source {
    static GSourceFuncs s_sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
};

GSourceFuncs Source::s_sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);
        *timeout = -1;
        wl_display_flush_clients(source.display);
        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);
        return !!source.pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto& source = *reinterpret_cast<Source*>(base);

        if (source.pfd.revents & G_IO_IN) {
            struct wl_event_loop* eventLoop = wl_display_get_event_loop(source.display);
            wl_event_loop_dispatch(eventLoop, -1);
            wl_display_flush_clients(source.display);
        }

        if (source.pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        source.pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

class Host {
public:
    Host()
    {
        m_display = wl_display_create();
        wl_display_init_shm(m_display);

        m_compositor = wl_global_create(m_display, &wl_compositor_interface, 3, this,
            [](struct wl_client* client, void*, uint32_t version, uint32_t id)
            {
                struct wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, version, id);
                if (!resource) {
                    wl_client_post_no_memory(client);
                    return;
                }

                wl_resource_set_implementation(resource, &s_compositorInterface, nullptr, nullptr);
            });

        m_source = g_source_new(&Source::s_sourceFuncs, sizeof(Source));
        auto& source = *reinterpret_cast<Source*>(m_source);

        struct wl_event_loop* eventLoop = wl_display_get_event_loop(m_display);
        source.pfd.fd = wl_event_loop_get_fd(eventLoop);
        source.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
        source.pfd.revents = 0;
        source.display = m_display;

        g_source_add_poll(m_source, &source.pfd);
        g_source_set_name(m_source, "WPEBackend-shm::Host");
        g_source_set_priority(m_source, -70);
        g_source_set_can_recurse(m_source, TRUE);
        g_source_attach(m_source, g_main_context_get_thread_default());
    }

    ~Host()
    {
        if (m_source)
            g_source_unref(m_source);

        if (m_display)
            wl_display_destroy(m_display);
    }

    int createClient()
    {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) < 0)
            return -1;

        int clientFd = dup(pair[1]);
        close(pair[1]);

        wl_client_create(m_display, pair[0]);
        return clientFd;
    }

private:
    static const struct wl_compositor_interface s_compositorInterface;
    static const struct wl_surface_interface s_surfaceInterface;

    struct wl_display* m_display;
    struct wl_global* m_compositor;
    GSource* m_source;
};

const struct wl_compositor_interface Host::s_compositorInterface = {
    // create_surface
    [](struct wl_client* client, struct wl_resource* resource, uint32_t id)
    {
        struct wl_resource* surfaceResource = wl_resource_create(client, &wl_surface_interface,
            wl_resource_get_version(resource), id);
        if (!surfaceResource) {
            wl_resource_post_no_memory(resource);
            return;
        }

        wl_resource_set_implementation(surfaceResource, &s_surfaceInterface, nullptr, nullptr);
    },
    // create_region
    [](struct wl_client*, struct wl_resource*, uint32_t) { },
};

const struct wl_surface_interface Host::s_surfaceInterface = {
    // destroy
    [](struct wl_client*, struct wl_resource*) { },
    // attach
    [](struct wl_client*, struct wl_resource*, struct wl_resource*, int32_t, int32_t) { },
    // damage
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t) { },
    // frame
    [](struct wl_client*, struct wl_resource*, uint32_t) { },
    // set_opaque_region
    [](struct wl_client*, struct wl_resource*, struct wl_resource*) { },
    // set_input_region
    [](struct wl_client*, struct wl_resource*, struct wl_resource*) { },
    // commit
    [](struct wl_client*, struct wl_resource*) { },
    // set_buffer_transform
    [](struct wl_client*, struct wl_resource*, int32_t) { },
    // set_buffer_scale
    [](struct wl_client*, struct wl_resource*, int32_t) { },
    // damage_buffer
    [](struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t) { },
};

}

struct wpe_renderer_host_interface shm_renderer_host = {
    // create
    []() -> void*
    {
        return new Host;
    },

    // destroy
    [](void* data)
    {
        auto* rendererHost = reinterpret_cast<Host*>(data);
        delete rendererHost;
    },

    // create_client
    [](void* data) -> int
    {
        auto& rendererHost = *static_cast<Host*>(data);
        return rendererHost.createClient();
    },
};
