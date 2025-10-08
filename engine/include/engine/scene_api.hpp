#pragma once

#include <engine/api.hpp>

extern "C" {
    ENGINE_API void scene_init();
    ENGINE_API void scene_update(float dt);
    ENGINE_API void scene_render();
    ENGINE_API void scene_shutdown();
}
