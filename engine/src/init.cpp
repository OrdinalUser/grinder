#include <engine/log.hpp>

extern "C" ENGINE_API void engine_destroy() {
    Engine::Log::destroy_logging();
}

extern "C" ENGINE_API void engine_initialize() {
    Engine::Log::setup_logging();
    atexit(engine_destroy);
}

namespace Engine {
    
}