add_library(engine SHARED
    src/init.cpp # contains initialization logic
    src/log.cpp # spdlog singleton wrapper
    src/exception.cpp # prettier exception recovery, uses log
    src/vfs.cpp # mapping for modules and their roots to real fs for asset lookup
    src/resource.cpp # resource tracking and loading
    src/renderer.cpp # hehe ^2 ; immediate mode drawing and ecs+scene_graph integration
    src/ecs.cpp # hehe ^3 ; entities and their data
    src/scene_graph.cpp # hehe ; scene graph
    src/scene.cpp # internal - loads scene modules and their callbacks
    src/layer_scene.cpp # no comment for now, run scene callbacks
    src/layer_debug.cpp # debug layer, overlays of the system
    src/layer.cpp # internal - layer stack implementation
    src/window.cpp # wraps glfw window minimally
    src/application.cpp # provdides application singleton
    # These are necessary for imgui, we are not graded for these!
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Glad (OpenGL loader)
add_library(glad STATIC libs/glad/src/glad.c)
target_include_directories(glad PUBLIC libs/glad/include)

# Add imgui backend headers
target_include_directories(engine PRIVATE ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR})

target_link_libraries(engine
    PUBLIC
        glad
        glfw
        glm
        spdlog::spdlog
        assimp
        opengl32
        backward
)

target_compile_definitions(engine PRIVATE ENGINE_BUILD)
