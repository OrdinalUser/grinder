#include <engine/log.hpp>
#include <engine/exception.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

ENGINE_API void engine_destroy() {
    Engine::Log::destroy_logging();
    glfwTerminate();
}

ENGINE_API void engine_initialize() {
    Engine::Log::setup_logging();
    atexit(engine_destroy);
    
    // Prepare glfw and opengl
    // ==== Initialize Window
    if (!glfwInit()) {
        Engine::Log::error("Failed to initialize GLFW");
    }
}

namespace Engine {
    
}