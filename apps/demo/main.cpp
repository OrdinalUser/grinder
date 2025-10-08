#include <iostream>
#include <engine/scene_api.hpp>

extern "C" {
    void scene_init() {
        std::cout << "[demo] scene_init()\n";
    }
    void scene_update(float dt) {
        std::cout << "[demo] scene_update(" << dt << ")\n";
    }
    void scene_render() {
        std::cout << "[demo] scene_render()\n";
    }
    void scene_shutdown() {
        std::cout << "[demo] scene_shutdown()\n";
    }
}
