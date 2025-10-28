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

    // Create a hidden 1x1 window to load OpenGL functions
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* tempWindow = glfwCreateWindow(1, 1, "Hidden", NULL, NULL);
    if (!tempWindow) {
        glfwTerminate();
        ENGINE_THROW("Failed to create temporary GLFW window");
    }

    glfwMakeContextCurrent(tempWindow);

    // Load OpenGL functions
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        ENGINE_THROW("Failed to initialize GLAD");
    }

    // Now release the temporary window
    glfwDestroyWindow(tempWindow);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
}

namespace Engine {
    
}