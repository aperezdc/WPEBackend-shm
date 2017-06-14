#include <wpe-fdo/view-backend-exportable.h>

#include "ws.h"
#include <cstdio>
#include <gio/gio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>

namespace {

class ViewBackend;

struct ClientBundle {
    struct wpe_view_backend_exportable_shm_client* client;
    void* data;
    ViewBackend* viewBackend;
};

class ViewBackend : public WS::ExportableClient {
public:
    ViewBackend(ClientBundle* clientBundle, struct wpe_view_backend* backend)
        : m_clientBundle(clientBundle)
        , m_backend(backend)
    {
        m_clientBundle->viewBackend = this;
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
        auto fd = dup(m_clientFd);
        if (fd == -1)
            perror("ViewBackend::clientFd: dup");
        return fd;
    }

    void frameCallback(struct wl_resource* callbackResource) override
    {
        m_callbackResources.push_back(callbackResource);
    }

    void exportBufferResource(struct wl_resource* bufferResource) override
    {
        struct wl_shm_buffer* shmBuffer = wl_shm_buffer_get(bufferResource);
        if (!shmBuffer) {
            fprintf(stderr, "exportBufferResource(): did not receive a wl_shm buffer\n");
            return;
        }

        wl_shm_buffer_begin_access(shmBuffer);

        auto* buffer = new struct wpe_view_backend_exportable_shm_buffer;
        buffer->buffer_resource = bufferResource;
        buffer->buffer = shmBuffer;
        buffer->data = wl_shm_buffer_get_data(shmBuffer);
        buffer->format = wl_shm_buffer_get_format(shmBuffer);
        buffer->width = wl_shm_buffer_get_width(shmBuffer);
        buffer->height = wl_shm_buffer_get_height(shmBuffer);
        buffer->stride = wl_shm_buffer_get_stride(shmBuffer);

        m_clientBundle->client->export_buffer(m_clientBundle->data, buffer);
    }

    void dispatchFrameCallback()
    {
        for (auto* resource : m_callbackResources)
            wl_callback_send_done(resource, 0);
        m_callbackResources.clear();
    }

    void releaseBuffer(struct wpe_view_backend_exportable_shm_buffer* buffer)
    {
        wl_shm_buffer_end_access(buffer->buffer);
        wl_buffer_send_release(buffer->buffer_resource);

        delete buffer;
    }

private:
    static gboolean s_socketCallback(GSocket*, GIOCondition, gpointer);

    ClientBundle* m_clientBundle;
    struct wpe_view_backend* m_backend;

    std::vector<struct wl_resource*> m_callbackResources;

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

static struct wpe_view_backend_interface view_backend_exportable_shm_interface = {
    // create
    [](void* data, struct wpe_view_backend* backend) -> void*
    {
        auto* clientBundle = static_cast<ClientBundle*>(data);
        return new ViewBackend(clientBundle, backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto& backend = *static_cast<ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& backend = *static_cast<ViewBackend*>(data);
        return backend.clientFd();
    },
};

}

extern "C" {

struct wpe_view_backend_exportable_shm {
    ClientBundle* clientBundle;
    struct wpe_view_backend* backend;
};

__attribute__((visibility("default")))
struct wpe_view_backend_exportable_shm*
wpe_view_backend_exportable_shm_create(struct wpe_view_backend_exportable_shm_client* client, void* data)
{
    auto* clientBundle = new ClientBundle{ client, data, nullptr };

    struct wpe_view_backend* backend = wpe_view_backend_create_with_backend_interface(&view_backend_exportable_shm_interface, clientBundle);

    auto* exportable = new struct wpe_view_backend_exportable_shm;
    exportable->clientBundle = clientBundle;
    exportable->backend = backend;

    return exportable;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_shm_destroy(struct wpe_view_backend_exportable_shm* exportable)
{
    wpe_view_backend_destroy(exportable->backend);
    delete exportable;
}

__attribute__((visibility("default")))
struct wpe_view_backend*
wpe_view_backend_exportable_shm_get_view_backend(struct wpe_view_backend_exportable_shm* exportable)
{
    return exportable->backend;
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_shm_dispatch_frame_complete(struct wpe_view_backend_exportable_shm* exportable)
{
    exportable->clientBundle->viewBackend->dispatchFrameCallback();
}

__attribute__((visibility("default")))
void
wpe_view_backend_exportable_shm_dispatch_release_buffer(struct wpe_view_backend_exportable_shm* exportable, struct wpe_view_backend_exportable_shm_buffer* buffer)
{
    exportable->clientBundle->viewBackend->releaseBuffer(buffer);
}

}
