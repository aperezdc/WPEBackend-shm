#include <wpe/renderer-backend-egl.h>
#include <wpe/renderer-host.h>
#include <wpe/view-backend.h>

extern struct wpe_renderer_host_interface shm_renderer_host;
extern struct wpe_view_backend_interface shm_view_backend;

extern struct wpe_renderer_backend_egl_interface shm_renderer_backend_egl;
extern struct wpe_renderer_backend_egl_target_interface shm_renderer_backend_egl_target;
extern struct wpe_renderer_backend_egl_offscreen_target_interface shm_renderer_backend_egl_offscreen_target;
