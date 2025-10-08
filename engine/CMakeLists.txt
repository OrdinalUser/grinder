add_library(engine SHARED
    src/log.cpp
    src/init.cpp
    # src/window.cpp
    # src/renderer.cpp
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Glad (OpenGL loader)
add_library(glad STATIC libs/glad/src/glad.c)
target_include_directories(glad PUBLIC libs/glad/include)

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

# Export definitions for clients (like scenes)
target_compile_definitions(engine PRIVATE ENGINE_BUILD)
