#include <engine/scene_api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>
#include <engine/vfs.hpp>
#include <engine/resource.hpp>

#include <engine/types.hpp>
#include <engine/application.hpp>

#include <fstream>
#include <iostream>

using namespace Engine;
extern "C" {
    void scene_init(scene_data_t scene_data) {
        Engine::Log::info("[dev] scene_init()");
        Application& app = Application::Get();
        Ref<VFS> vfs = app.GetVFS();
        Ref<ResourceSystem> rs = app.GetResourceSystem();

        auto module_name = string(scene_data.module_name);
        auto img = rs->load<Image>(vfs->GetResourcePath(module_name, "assets/hourglass.png"));
        Log::info("{} : {} {}", img->channels, img->width, img->height);
        /*auto path_to_resource = vfs.GetResourcePath(module_name, "main.cpp");
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
        std::cout << std::endl;*/
    }

    void scene_update(float deltaTime) {
        // Engine::Log::info("[dev] scene_update({})", dt);
        static vec3 start{ 0 };
        static vec3 end{ 10 };
        static f32 t = 0;
        vec3 interp = Math::lerp(start, end, t);
        t += deltaTime / 32.0f;
        t = min(t, 1.1f);
        
        if (t <= 1.0f) Log::info("{} {} {} - {}", interp.x, interp.y, interp.z, t);
    }

    void scene_render() {
        // Engine::Log::info("[dev] scene_render()");
    }
    void scene_shutdown() {
        Engine::Log::info("[dev] scene_shutdown()");
    }
}
