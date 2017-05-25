#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/view-backend.h>

struct wl_resource;
struct wl_shm_buffer;
struct wpe_view_backend_exportable_shm;

struct wpe_view_backend_exportable_shm_buffer {
    struct wl_resource* buffer_resource;
    struct wl_shm_buffer* buffer;

    void* data;
    uint32_t format;
    int32_t width;
    int32_t height;
    int32_t stride;
};

struct wpe_view_backend_exportable_shm_client {
    void (*export_buffer)(void*, struct wpe_view_backend_exportable_shm_buffer*);
};

struct wpe_view_backend_exportable_shm*
wpe_view_backend_exportable_shm_create(struct wpe_view_backend_exportable_shm_client*, void*);

void
wpe_view_backend_exportable_shm_destroy(struct wpe_view_backend_exportable_shm*);

struct wpe_view_backend*
wpe_view_backend_exportable_shm_get_view_backend(struct wpe_view_backend_exportable_shm*);

void
wpe_view_backend_exportable_shm_dispatch_frame_complete(struct wpe_view_backend_exportable_shm*);

void
wpe_view_backend_exportable_shm_dispatch_release_buffer(struct wpe_view_backend_exportable_shm*, struct wpe_view_backend_exportable_shm_buffer*);


#ifdef __cplusplus
}
#endif
