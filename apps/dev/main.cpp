#include <engine/scene_api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>
#include <engine/vfs.hpp>
#include <engine/resource.hpp>

#include <engine/types.hpp>
#include <engine/application.hpp>

#include <glad/glad.h>

#include <fstream>
#include <iostream>

using namespace Engine;

Ref<Shader> shader = nullptr;
GLuint VBO = 0, VAO = 0;

extern "C" {
    void scene_init(scene_data_t scene_data) {
        Engine::Log::info("[dev] scene_init()");
        Application& app = Application::Get();
        Ref<VFS> vfs = app.GetVFS();
        Ref<ResourceSystem> rs = app.GetResourceSystem();

        auto module_name = string(scene_data.module_name);
        auto img = rs->load<Image>(vfs->GetResourcePath(module_name, "assets/hourglass.png"));
        Log::info("{} : {} {}", img->channels, img->width, img->height);

        shader = rs->load<Shader>(vfs->Resolve(module_name, "assets/rgb_triangle"));
        shader = rs->load<Shader>(vfs->Resolve(module_name, "assets/rgb_triangle"));

        float verts[] = {
             0.0f,  0.5f, 0.0f,   1, 0, 0,  // top (red)
            -0.5f, -0.5f, 0.0f,   0, 1, 0,  // left (green)
             0.5f, -0.5f, 0.0f,   0, 0, 1   // right (blue)
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        // Binding the Vertex Array Object
        glBindVertexArray(VAO);

        // Binding the Vertex Buffer Object
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        // Set vertex positions
        constexpr u32 stride = 6 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0); // Positions
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    }

    void scene_update(float deltaTime) {
        // Engine::Log::info("[dev] scene_update({})", dt);
    }

    void scene_render() {
        // Engine::Log::info("[dev] scene_render()");
        glUseProgram(shader->program);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
    }
    void scene_shutdown() {
        Engine::Log::info("[dev] scene_shutdown()");
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }
}
