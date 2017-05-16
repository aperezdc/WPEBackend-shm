#include <wpe/loader.h>

#include <cstdio>
#include <cstring>

#include "interfaces.h"

extern "C" {

__attribute__((visibility("default")))
struct wpe_loader_interface _wpe_loader_interface = {
    [](const char* object_name) -> void* {
        fprintf(stderr, "WPEBackend-shm: %s\n", object_name);

        if (!std::strcmp(object_name, "_wpe_renderer_host_interface"))
            return &shm_renderer_host;
        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &shm_view_backend;

        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &shm_renderer_backend_egl;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &shm_renderer_backend_egl_target;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &shm_renderer_backend_egl_offscreen_target;

        return nullptr;
    },
};

}
