#include <engine/scene_api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>
#include <engine/vfs.hpp>

#include <fstream>
#include <iostream>
#include <string>

extern "C" {
    void scene_init(scene_data_t scene_data) {
        Engine::Log::info("[dev] scene_init()");
        Engine::VFS& vfs = Engine::VFS::get();
        auto module_name = std::string(scene_data.module_name);
        auto path_to_resource = vfs.GetResourcePath(module_name, "main.cpp");
        auto path_to_engine_resource = vfs.GetEngineResourcePath("src/vfs.cpp");
        std::ifstream f(path_to_resource);
        
        std::string line;
        while (std::getline(f, line)) {
            std::cout << line << '\n';
        }
        std::cout << std::endl;

        std::ifstream f1(path_to_engine_resource);
        while (std::getline(f1, line)) {
            std::cout << line << '\n';
        }
        std::cout << std::endl;
    }

    void scene_update(float dt) {
        Engine::Log::info("[dev] scene_update({})", dt);
    }
    void scene_render() {
        Engine::Log::info("[dev] scene_render()");
    }
    void scene_shutdown() {
        Engine::Log::info("[dev] scene_shutdown()");
    }
}
