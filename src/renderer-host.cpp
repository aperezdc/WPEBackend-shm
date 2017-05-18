#include "interfaces.h"
#include "ws.h"

struct wpe_renderer_host_interface shm_renderer_host = {
    // create
    []() -> void*
    {
        return nullptr;
    },

    // destroy
    [](void* data)
    { },

    // create_client
    [](void* data) -> int
    {
        return WS::Instance::singleton().createClient();
    },
};
