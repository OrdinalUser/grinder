add_library(engine SHARED
    src/init.cpp
    src/log.cpp
    src/exception.cpp
    src/vfs.cpp
    src/resource.cpp # not implemented yet
    src/graphics.cpp # hehe
    src/scene.cpp
    src/layer_scene.cpp
    src/layer_gui.cpp
    src/layer.cpp
    src/window.cpp
    src/application.cpp
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
