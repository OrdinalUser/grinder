#include <engine/log.hpp>
#include <engine/exception.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#ifdef _OPENMP
#include <omp.h>
#endif

ENGINE_API void engine_destroy() {
    #ifdef _OPENMP
    // Wait for all OpenMP threads to finish
    #pragma omp barrier

    // Set number of threads to 1 to prevent new parallel regions
    omp_set_num_threads(1);
    #endif

    Engine::Log::destroy_logging();
    glfwTerminate();
}

ENGINE_API void engine_initialize() {
    Engine::Log::setup_logging();
    atexit(engine_destroy);

    #ifdef _OPENMP
    // Optional: Set default thread count for the application
    omp_set_num_threads(4);
    Engine::Log::info("OpenMP initialized with {} threads", omp_get_max_threads());
    #endif
    
    // Prepare glfw and opengl
    // ==== Initialize Window
    if (!glfwInit()) {
        Engine::Log::error("Failed to initialize GLFW");
    }
}
