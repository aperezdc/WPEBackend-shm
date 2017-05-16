#include "interfaces.h"

struct wpe_view_backend_interface shm_view_backend = {
    // create
    [](void*, struct wpe_view_backend*) -> void* { return nullptr; },
    // destroy
    [](void*) { },
    // initialize
    [](void*) { },
    // get_renderer_host_fd
    [](void*) -> int { return -1; },
};
