#include "interfaces.h"

#include "ws.h"
#include <cairo.h>
#include <cstdio>
#include <gio/gio.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace {

class ViewBackend : public WS::ExportableClient {
public:
    ViewBackend(struct wpe_view_backend* backend)
        : m_backend(backend)
    {
        fprintf(stderr, "ViewBackend() %p wpe_view_backend %p\n", this, backend);
    }

    ~ViewBackend()
    {
        if (m_clientFd != -1)
            close(m_clientFd);

        if (m_source)
            g_source_destroy(m_source);
        if (m_socket)
            g_object_unref(m_socket);
    }

    void initialize()
    {
        int sockets[2];
        int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
        if (ret == -1)
            return;

        m_socket = g_socket_new_from_fd(sockets[0], nullptr);
        if (!m_socket) {
            close(sockets[0]);
            close(sockets[1]);
            return;
        }

        m_source = g_socket_create_source(m_socket, G_IO_IN, nullptr);
        g_source_set_callback(m_source, reinterpret_cast<GSourceFunc>(s_socketCallback), this, nullptr);
        g_source_set_priority(m_source, -70);
        g_source_set_can_recurse(m_source, TRUE);
        g_source_attach(m_source, g_main_context_get_thread_default());

        m_clientFd = sockets[1];
    }

    int clientFd()
    {
        return dup(m_clientFd);
    }

    void frameCallback(struct wl_resource*) override { }

    void exportBufferResource(struct wl_resource* bufferResource) override
    {
        struct wl_shm_buffer* shmBuffer = wl_shm_buffer_get(bufferResource);
        if (!shmBuffer) {
            fprintf(stderr, "exportBufferResource(): did not receive a wl_shm buffer\n");
            return;
        }

        wl_shm_buffer_begin_access(shmBuffer);

        void* data = wl_shm_buffer_get_data(shmBuffer);
        int32_t stride = wl_shm_buffer_get_stride(shmBuffer);
        uint32_t format = wl_shm_buffer_get_format(shmBuffer);
        int32_t width = wl_shm_buffer_get_width(shmBuffer);
        int32_t height = wl_shm_buffer_get_height(shmBuffer);

        fprintf(stderr, "exportBufferResource(): received buffer %p, its data %p\n",
            bufferResource, data);
        fprintf(stderr, "    stride %d format %u size (%d,%d)\n",
            stride, format, width, height);

        char* png_image_path = getenv("WPE_DUMP_PNG_PATH");
        if (png_image_path) {
            char filename[128];
            static int files = 0;
            cairo_surface_t* surface = cairo_image_surface_create_for_data(static_cast<unsigned char*>(data),
                                                                           CAIRO_FORMAT_ARGB32,
                                                                           width,
                                                                           height,
                                                                           stride);
            sprintf(filename, "%sdump_%d.png", png_image_path, files++);
            cairo_surface_write_to_png(surface, filename);
            cairo_surface_destroy(surface);
        }

        wl_shm_buffer_end_access(shmBuffer);
    }

private:
    static gboolean s_socketCallback(GSocket*, GIOCondition, gpointer);

    struct wpe_view_backend* m_backend;

    GSocket* m_socket;
    GSource* m_source;
    int m_clientFd { -1 };
};

gboolean ViewBackend::s_socketCallback(GSocket* socket, GIOCondition condition, gpointer data)
{
    if (!(condition & G_IO_IN))
        return TRUE;

    uint32_t message[2];
    gssize len = g_socket_receive(socket, reinterpret_cast<gchar*>(message), sizeof(uint32_t) * 2,
        nullptr, nullptr);
    if (len == -1)
        return FALSE;

    if (len == sizeof(uint32_t) * 2 && message[0] == 0x42) {
        fprintf(stderr, "ViewBackend: registered to surface id %u\n", message[1]);

        auto& viewBackend = *static_cast<ViewBackend*>(data);
        WS::Instance::singleton().registerViewBackend(message[1], viewBackend);
    }

    fprintf(stderr, "ViewBackend::s_socketCallback() read %d bytes: { %u, %u }\n",
        len, message[0], message[1]);

    return TRUE;
}

}

struct wpe_view_backend_interface shm_view_backend = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new ViewBackend(backend);
    },
    // destroy
    [](void* data)
    {
        auto* viewBackend = reinterpret_cast<ViewBackend*>(data);
        delete viewBackend;
    },
    // initialize
    [](void* data)
    {
        auto& viewBackend = *reinterpret_cast<ViewBackend*>(data);
        viewBackend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& viewBackend = *reinterpret_cast<ViewBackend*>(data);
        return viewBackend.clientFd();
    },
};
