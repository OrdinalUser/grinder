#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <engine/scene_api.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <engine/engine.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

typedef void (*scene_func)();
typedef void (*scene_func_pf)(float);

int main() {
    // Set the signal handler for segmentation faults
    engine_initialize();
    Engine::Log::info("Main started");

    // volatile int a = 0; // simulated error 1
    // int num = 10 / a;

    // int* paddr = 0; // simulater error 2
    // int value = *paddr;

    // throw std::exception("Exception message", 42);
    // auto e = ENGINE_EXCEPTION_CODE("Hopefully caught exception", 42);
    // e.log(); // This works?
    try {
        throw ENGINE_EXCEPTION_CODE("Hopefully caught exception", 42);
    }
    catch (const Engine::Exception& e) {
        e.log(); // this doesn't log?
    }

    throw ENGINE_EXCEPTION_CODE("Uncaught exception", 42);

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
        Engine::Log::error("Failed to create temporary GLFW window");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(tempWindow);

    // Load OpenGL functions
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Engine::Log::error("Failed to initialize GLAD");
        return -1;
    }

    // Now release the temporary window
    glfwDestroyWindow(tempWindow);

    // Create the real window
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(640, 480, "My OpenGL Application", NULL, NULL);
    if (!window) {
        Engine::Log::error("Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // ==== Asset test
    int w, h, channels;
    unsigned char* data = stbi_load("texture.png", &w, &h, &channels, 4);
    if (!data) Engine::Log::error("Failed to load image");

    // ==== Load scene
    HMODULE scene = LoadLibraryA("C:\\Users\\Skyte\\Desktop\\grinder\\build\\apps\\demo\\Debug\\scene_demod.dll");
    if (!scene) {
        Engine::Log::error("Failed to load scene DLL.\n");
        return 1;
    }

    auto init = (scene_func)GetProcAddress(scene, "scene_init");
    auto update = (scene_func_pf)GetProcAddress(scene, "scene_update");
    auto render = (scene_func)GetProcAddress(scene, "scene_render");
    auto shutdown = (scene_func)GetProcAddress(scene, "scene_shutdown");

    if (!init || !update || !render || !shutdown) {
        Engine::Log::error("Invalid scene API.\n");
        return 1;
    }

    // ==== Run scene and orchestrate input
    init();
    for (int i = 0; i < 300; ++i) {
        update(0.016f);
        render();
    }
    shutdown();

    FreeLibrary(scene);
    return 0;
}
