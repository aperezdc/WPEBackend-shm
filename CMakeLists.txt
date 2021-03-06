cmake_minimum_required(VERSION 3.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

find_package(Cairo REQUIRED)
find_package(EGL REQUIRED)
find_package(GLIB REQUIRED COMPONENTS gio)
find_package(Wayland REQUIRED)
find_package(WaylandEGL REQUIRED)
find_package(WPEBackend REQUIRED)

set(WPEBACKEND_SHM_INCLUDE_DIRECTORIES
    "include"
    ${CAIRO_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
    ${WPEBACKEND_INCLUDE_DIRS}
)

set(WPEBACKEND_SHM_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${EGL_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${WPEBACKEND_LIBRARIES}
)

set(WPEBACKEND_SHM_PUBLIC_HEADERS
    include/wpe-fdo/view-backend-exportable.h
)

set(WPEBACKEND_SHM_SOURCES
    src/renderer-backend-egl.cpp
    src/renderer-host.cpp
    src/view-backend.cpp
    src/view-backend-exportable-shm.cpp
    src/shm.cpp
    src/ws.cpp
)

add_library(WPEBackend-shm SHARED ${WPEBACKEND_SHM_SOURCES})
target_include_directories(WPEBackend-shm PRIVATE ${WPEBACKEND_SHM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-shm ${WPEBACKEND_SHM_LIBRARIES})

install(TARGETS WPEBackend-shm
    EXPORT WPEBackend-shmTargets
    LIBRARY DESTINATION lib
)
install(
    FILES ${WPEBACKEND_SHM_PUBLIC_HEADERS}
    DESTINATION include/wpe-fdo
)
