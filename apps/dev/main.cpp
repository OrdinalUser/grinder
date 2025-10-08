#include <iostream>
#include <engine/scene_api.hpp>

extern "C" {
    void scene_init() {
        std::cout << "[dev] scene_init()\n";
    }
    void scene_update(float dt) {
        std::cout << "[dev] scene_update(" << dt << ")\n";
    }
    void scene_render() {
        std::cout << "[dev] scene_render()\n";
    }
    void scene_shutdown() {
        std::cout << "[dev] scene_shutdown()\n";
    }
}
