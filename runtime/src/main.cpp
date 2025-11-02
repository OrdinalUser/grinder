#include <engine/engine.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>
#include <engine/vfs.hpp>
#include <engine/scene.hpp>
#include <engine/application.hpp>
#include <engine/types.hpp>
#include <engine/ecs.hpp>

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

    // Window properties
    const WindowProps props{
        "Grinder Engine",
        1600, 900, false
    };

    // ==== Start loading shit
    // Initialize subsystems, enforce ordering using scopes
    Ref<Window> window = MakeRef<Window>(props); // window must get destroyed last as it holds the active gl context
    {
        // Asset subsystems and data depend on gl context
        Ref<VFS> vfs = MakeRef<VFS>();
        vfs->AddResourcePath(Engine::VFS::GetCurrentModuleName(), "engine"); // add engine resources
        Ref<ECS> ecs = MakeRef<ECS>();
        Ref<ResourceSystem> rs = MakeRef<ResourceSystem>();
        {
            // Application depends on its subsystems

            // Create our application
            Engine::Application app(window, vfs, rs, ecs);
            
            // load our scene as a layer
            #if _DEBUG
            // Thanks CMake for marking my lib with 'd'
            const std::string dllName = "scene_devd.dll";
            const std::filesystem::path dllPath = std::filesystem::path("build/bin") / "Debug" / dllName;
            #else
            const std::string dllName = "scene_dev.dll";
            const std::filesystem::path dllPath = std::filesystem::path("build/bin") / "Release" / dllName;
            #endif // _DEBUG
            app.PushLayer(static_cast<ILayer*>(new SceneLayer(new Scene(
                dllPath,
                std::filesystem::path("apps/dev")
            ))));
            
            #ifdef _DEBUG
            // Push debug layer as an overlay
            app.PushLayer(static_cast<ILayer*>(new DebugLayer()));
            #endif
            
            app.Run();
        }
    }

    return 0;
}
