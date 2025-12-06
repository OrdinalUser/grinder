set(ENGINE_SOURCES
    src/init.cpp # contains initialization logic
    src/log.cpp # spdlog singleton wrapper
    src/exception.cpp # prettier exception recovery, uses log
    src/vfs.cpp # mapping for modules and their roots to real fs for asset lookup
    src/resource.cpp # resource tracking and loading
    src/renderer.cpp # hehe ^2 ; immediate mode drawing and ecs+scene_graph integration #todo
    src/ecs.cpp # the data layer of the entire system
    src/scene.cpp # internal - loads scene modules and their callbacks
    src/layer_scene.cpp # no comment for now, run scene callbacks
    src/layer_debug.cpp # debug layer, overlays of the system
    src/layer.cpp # internal - layer stack implementation
    src/window.cpp # wraps glfw window minimally
    src/application.cpp # provdides application singleton
)

set(ENGINE_SOURCES_INCLUDES
    include/engine/api.hpp
    include/engine/application.hpp
    include/engine/ecs.hpp
    include/engine/engine.hpp
    include/engine/exception.hpp
    include/engine/layer.hpp
    include/engine/log.hpp
    include/engine/perf_profiler.hpp
    include/engine/renderer.hpp
    include/engine/resource.hpp
    include/engine/scene_api.hpp
    include/engine/scene.hpp
    include/engine/types.hpp
    include/engine/vfs.hpp
    include/engine/window.hpp
    include/engine/particle.hpp
)

set(LIBRARY_SOURCES
    # These are necessary for imgui, we are not graded for these!
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    # These are also necessary
    libs/glad/src/glad.c
)

add_library(engine SHARED
    ${ENGINE_SOURCES}
    ${ENGINE_SOURCES_INCLUDES}
    ${LIBRARY_SOURCES}
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${stb_SOURCE_DIR}
        libs/glad/include
)

# Add imgui backend headers
target_include_directories(engine PRIVATE ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR})
target_compile_definitions(engine PRIVATE ENGINE_BUILD GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD)

target_link_libraries(engine
    PUBLIC
        glfw
        glm
        spdlog::spdlog
        assimp
        opengl32
        backward
)

find_package(OpenMP REQUIRED)

if(OpenMP_CXX_FOUND)
    target_link_libraries(engine PUBLIC OpenMP::OpenMP_CXX)
endif()
