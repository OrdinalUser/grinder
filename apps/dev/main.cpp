#include <engine/scene_api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>

extern "C" {
    void scene_init() {
        Engine::Log::info("[dev] scene_init()");
    }
    void scene_update(float dt) {
        Engine::Log::info("[dev] scene_update({})", dt);
        ENGINE_THROW_CODE("Uncaught exception", 42);
    }
    void scene_render() {
        Engine::Log::info("[dev] scene_render()");
    }
    void scene_shutdown() {
        Engine::Log::info("[dev] scene_shutdown()");
    }
}
