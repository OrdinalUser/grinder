#ifndef GRINDER_API_H
#define GRINDER_API_H

#include <engine/api.hpp>

typedef struct scene_data {
    char module_name[256]; // C-string
} scene_data_t;

typedef void (*scene_init_f)(scene_data_t);
typedef void (*scene_update_f)(float);
typedef void (*scene_render_f)(void);
typedef void (*scene_shutdown_f)(void);

extern "C" {
    ENGINE_API void scene_init(scene_data_t scene_data);
    ENGINE_API void scene_update(float dt);
    ENGINE_API void scene_render();
    ENGINE_API void scene_shutdown();
}

#endif