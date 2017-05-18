#pragma once

#include <glib.h>
#include <unordered_map>
#include <wayland-server.h>

class wpe_view_backend;

namespace WS {

struct Surface;

struct ExportableClient {
    virtual void exportBufferResource(struct wl_resource*) = 0;
};

class Instance {
public:
    static Instance& singleton();
    ~Instance();

    int createClient();

    void createSurface(uint32_t, Surface*);
    void registerViewBackend(uint32_t, ExportableClient&);

private:
    Instance();

    struct wl_display* m_display;
    struct wl_global* m_compositor;
    GSource* m_source;

    std::unordered_map<uint32_t, Surface*> m_viewBackendMap;
};

} // namespace WS
