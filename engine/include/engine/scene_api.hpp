#pragma once

#define ENGINE_API __declspec(dllexport)

extern "C" {
    ENGINE_API void scene_init();
    ENGINE_API void scene_update(float dt);
    ENGINE_API void scene_render();
    ENGINE_API void scene_shutdown();
}
