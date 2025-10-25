#include <engine/engine.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>
#include <engine/vfs.hpp>
#include <engine/scene.hpp>
#include <engine/application.hpp>
#include <engine/types.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static void walk_cwd_to_project_root() {
    const std::string root_name = "grinder";
    std::filesystem::path cwd = std::filesystem::current_path();
    while (cwd.stem().filename() != root_name) {
        cwd = cwd.parent_path();
    }
    std::filesystem::current_path(cwd);
}

int main() {
    using namespace Engine;

    // Set cwd to project root - #hack
    walk_cwd_to_project_root();

    // Run engine initializations
    engine_initialize();

    // ==== Log test
    int w, h, channels;
    unsigned char* data = stbi_load("texture.png", &w, &h, &channels, 4);
    if (!data) Engine::Log::error("Failed to load image");

    // ==== Exception log test
    try {
        ENGINE_THROW("Exception throw test");
    }
    catch (Engine::Exception& e) {
        e.log();
    }

    const WindowProps props{
        "Grinder Engine",
        1600, 900, false
    };

    // ==== Start loading shit
    // Initialize application
    Ref<VFS> vfs = MakeRef<VFS>();
    Ref<ResourceSystem> rs = MakeRef<ResourceSystem>();

    vfs->AddResourcePath(Engine::VFS::GetCurrentModuleName(), "engine"); // add engine resources
    {
        Engine::Application app(std::make_unique<Window>(props), vfs, rs);
        #ifdef _DEBUG
            // Push debug layer as an overlay
            app.PushLayer( reinterpret_cast<ILayer*>(  new DebugLayer()  ) );
        #endif
        // load our scene as a layer
        app.PushLayer(reinterpret_cast<ILayer*>(new SceneLayer(new Scene(
            std::filesystem::path("build/bin/Debug/scene_devd.dll"),
            std::filesystem::path("apps/dev")
        ))));

        app.Run();
    }

    return 0;
}
