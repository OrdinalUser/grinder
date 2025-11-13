#include <engine/renderer.hpp>
#include <engine/resource.hpp>
#include <engine/application.hpp>
#include <engine/perf_profiler.hpp>
#include <algorithm>

namespace Engine {

    Renderer::ComputeShader::ComputeShader(const std::filesystem::path& filepath) {
        GLuint cullShader = glCreateShader(GL_COMPUTE_SHADER);

        std::string src = ReadFile(filepath);
        const char* csrc = src.c_str();
        glShaderSource(cullShader, 1, &csrc, nullptr);
        glCompileShader(cullShader);

        // check compile log
        GLint ok = 0;
        glGetShaderiv(cullShader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096];
            glGetShaderInfoLog(cullShader, sizeof(log), nullptr, log);
            fprintf(stderr, "Compute shader error:\n%s\n", log);
            ENGINE_THROW("Compute shader compilation failed");
        }

        program = glCreateProgram();
        glAttachShader(program, cullShader);
        glLinkProgram(program);
        glDeleteShader(cullShader);
    }

    Renderer::ComputeShader::~ComputeShader() {
        glDeleteProgram(program);
    }

    // ========== Public API ==========

    ENGINE_API Renderer::Renderer() {
        Ref<VFS> vfs = Engine::Application::Get().GetVFS();

        glGenBuffers(1, &m_ssbo); // Prepare our reusable ssbo for instancing
        glGenBuffers(1, &m_instancesSSBO);
        // Load our cull shader
        m_cullShader = new ComputeShader(vfs->GetEngineResourcePath("assets/shaders/culling.glsl"));
        glGenBuffers(1, &m_visibilitySSBO);
        glGenBuffers(1, &m_frustumUBO);
    }

    ENGINE_API Renderer::~Renderer() {
        glDeleteBuffers(1, &m_ssbo);
        delete m_cullShader;
    }

    void Renderer::SetCamera(Transform* transform, Camera* camera) {
        m_cameraTransform = transform;
        m_camera = camera;
        m_hasCameraSet = (transform != nullptr && camera != nullptr);

        if (m_hasCameraSet) {
            m_projViewMatrix = camera->projectionMatrix * camera->viewMatrix;
            m_cameraPosition = transform->modelMatrix * vec4(transform->position, 1.0f); // TODO
            ExtractFrustumPlanes();
        }
    }

    void Renderer::Queue(Transform* transform, Mesh* mesh, Material* material) {
        if (!mesh || !material || !material->shader) return;

        // Enqueue for culling
        m_gpuInstanceData.emplace_back(transform->modelMatrix, mesh->bsphere);
        m_gpuInstances.emplace_back(transform, mesh, material);
    }

    void Renderer::QueueDrawable3D(Transform* transform, Component::Drawable3D* drawable) {
        if (!drawable || !drawable->model) return;

        //// Frustum culling using model's bounding box - moved to render stage
        //if (m_hasCameraSet) {
        //    if (!IsBoxInFrustum(drawable->model->bounds, transform->modelMatrix)) {
        //        m_stats.culledObjects++;
        //        return; // Object is outside frustum, skip
        //    }
        //}

        // Queue all mesh entries in the collection
        const auto& collection = drawable->GetCollection();
        for (const auto& entry : collection) {
            Queue(transform, entry.mesh, entry.material);
        }
    }

    void Renderer::QueueLight(Transform* transform, Light* light) {
        if (!transform || !light) return;
        m_queuedLights.emplace_back(transform, light);
    }

    void Renderer::ProcessQueue() {
        // No camera? No drawing
        if (!m_camera) return;

        PERF_BEGIN("Renderer_Culling");
        // Upload our queued stuff
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instancesSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_gpuInstanceData.size() * sizeof(GPU_InstanceData), m_gpuInstanceData.data(), GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_visibilitySSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_gpuInstanceData.size() * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_UNIFORM_BUFFER, m_frustumUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Frustum), &m_frustum, GL_DYNAMIC_DRAW);

        // Bind and dispatch computer shader
        glUseProgram(m_cullShader->program);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_instancesSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_visibilitySSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_frustumUBO);
        glDispatchCompute((m_gpuInstanceData.size() + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        PERF_END("Renderer_Culling");

        PERF_BEGIN("Renderer_Cmd");
        // Now we build the draw batches
        // Fetch data from gpu
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_visibilitySSBO);
        uint32_t* visibleFlags = (uint32_t*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, m_gpuInstanceData.size() * sizeof(uint32_t), GL_MAP_READ_BIT);

        // Construct batches themselves
        for (size_t i = 0; i < m_gpuInstances.size(); i++) {
            if (!visibleFlags[i]) {
                m_stats.culledObjects++;
                continue;
            }

            // Determine if transparent
            const GPUInstance& instance = m_gpuInstances[i];
            if (instance.material->isTransparent) {
                // Calculate distance to camera for sorting
                float distance = 0.0f;
                if (m_hasCameraSet) {
                    vec3 objPos = instance.transform->position;
                    distance = glm::length(m_cameraPosition - objPos);
                }

                DrawCommand cmd;
                cmd.transform = instance.transform;
                cmd.mesh = instance.mesh;
                cmd.material = instance.material;
                cmd.distanceToCamera = distance;
                m_transparentQueue.push_back(cmd);
            }
            else {
                // Batch opaque objects
                BatchKey key{ instance.mesh, instance.material, instance.material->shader.get() };

                auto& batch = m_opaqueBatches[key];
                batch.mesh = instance.mesh;
                batch.material = instance.material;
                batch.modelMatrices.push_back(instance.transform->modelMatrix);
            }
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        PERF_END("Renderer_Cmd");
    }

    void Renderer::Draw() {
        if (!m_hasCameraSet) return;

        // Reset stats
        m_stats = Stats{};
        m_stats.totalObjects = 0;
        for (const auto& [key, batch] : m_opaqueBatches) {
            m_stats.totalObjects += batch.modelMatrices.size();
        }
        m_stats.totalObjects += m_transparentQueue.size();

        // Run global culling
        ProcessQueue();

        // Process lights into world space
        ProcessLights(); // TODO!

        // Optional: Render to framebuffer for post-processing
        if (m_useFramebuffer) {
            BeginFramebufferPass();
        }

        // Setup context
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // cleared outside of renderer!
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Render opaque geometry
        DrawOpaque();

        // Render transparent geometry
        if (!m_transparentQueue.empty()) {
            DrawTransparent();
        }

        // Shadows, here?

        // Optional: Apply post-processing
        if (m_useFramebuffer) {
            EndFramebufferPass(); // TODO
        }
        auto stats = m_stats;
        int a = 2;
    }

    void Renderer::Clear() {
        m_opaqueBatches.clear();
        m_transparentQueue.clear();
        m_queuedLights.clear();
        m_processedLights.clear();
        m_gpuInstanceData.clear();
        m_gpuInstances.clear();
        m_stats = Stats{};
    }

    // ========== Light Processing ==========

    void Renderer::ProcessLights() {
        m_processedLights.clear();
        m_processedLights.reserve(m_queuedLights.size());

        for (const auto& [transform, light] : m_queuedLights) {
            LightData data;
            data.type = light->type;
            data.color = light->color;
            data.intensity = light->intensity;

            // Extract position and direction from transform
            data.position = transform->position;
            data.direction = transform->Forward();

            // Copy light-specific properties
            data.range = light->range;
            data.innerCutoff = glm::cos(glm::radians(light->innerCutoffDegrees));
            data.outerCutoff = glm::cos(glm::radians(light->outerCutoffDegrees));

            m_processedLights.push_back(data);
        }
    }

    void Renderer::SetLightUniforms(Shader* shader) {
        // TODO: Light integration
        /*shader->SetUniform("uLightCount", static_cast<int>(m_processedLights.size()));

        for (size_t i = 0; i < m_processedLights.size(); ++i) {
            std::string base = "uLights[" + std::to_string(i) + "].";
            const LightData& light = m_processedLights[i];

            shader->SetUniform(base + "type", static_cast<int>(light.type));
            shader->SetUniform(base + "color", light.color);
            shader->SetUniform(base + "intensity", light.intensity);
            shader->SetUniform(base + "position", light.position);
            shader->SetUniform(base + "direction", light.direction);
            shader->SetUniform(base + "range", light.range);
            shader->SetUniform(base + "innerCutoff", light.innerCutoff);
            shader->SetUniform(base + "outerCutoff", light.outerCutoff);
        }*/
    }

    void Renderer::SetCommonUniforms(Shader* shader) {
        shader->SetUniform("uProjView", m_projViewMatrix);
        shader->SetUniform("uViewPos", m_cameraPosition);
        SetLightUniforms(shader);
    }

    void Renderer::SetMaterialUniforms(Material* material) {
        Shader* shader = material->shader.get();

        // Set material properties
        if (material->renderType == Material::RenderType::LIT) {
            shader->SetUniform("uMaterial.diffuseColor", material->diffuseColor);
            shader->SetUniform("uMaterial.specularColor", material->specularColor);
            shader->SetUniform("uMaterial.shininess", material->shininess);
            if (material->isTransparent)
                shader->SetUniform("uMaterial.opacity", material->opacity);
        }
        
        // Set textures (only for textured materials)
        if (material->renderType == Material::RenderType::TEXTURED) {
            if (material->diffuse && material->diffuse->id) {
                shader->SetUniform("uMaterial.diffuseMap", *material->diffuse, Shader::TextureSlot::DIFFUSE);
            }
            if (material->specular && material->specular->id) {
                shader->SetUniform("uMaterial.specularMap", *material->specular, Shader::TextureSlot::SPECULAR);
            }
            if (material->normal && material->normal->id) {
                shader->SetUniform("uMaterial.normalMap", *material->normal, Shader::TextureSlot::NORMAL);
            }
            shader->SetUniform("uMaterial.shininess", material->shininess);
        }
    }

    // ========== Drawing ==========

    void Renderer::DrawOpaque() {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        m_stats.batchCount = m_opaqueBatches.size();

        for (const auto& [key, batch] : m_opaqueBatches) {
            Shader* shader = key.shader;
            shader->Enable();

            shader->SetUniform("uLightPos", m_cameraPosition);
            shader->SetUniform("uLightColor", vec3(1, 1, 1));

            // Set common uniforms once per batch
            SetCommonUniforms(shader);
            SetMaterialUniforms(key.material);

            if (batch.modelMatrices.size() == 1) {
                // Single object - standard draw
                shader->SetUniform("uModel", batch.modelMatrices[0]);
                shader->SetUniform("uUseInstancing", false);
                key.mesh->Draw();
                m_stats.drawCalls++;
            }
            else {
                /// Multiple objects - prepare an SSBO and do an instanced draw
                // Fill SSBO with data
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
                glBufferData(GL_SHADER_STORAGE_BUFFER, batch.modelMatrices.size() * sizeof(mat4), batch.modelMatrices.data(), GL_DYNAMIC_DRAW);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ssbo);

                // Draw our stuff
                shader->SetUniform("uUseInstancing", true);
                key.mesh->Bind(); // set base mesh
                glDrawElementsInstanced(GL_TRIANGLES, key.mesh->indicesCount, GL_UNSIGNED_INT, 0, batch.modelMatrices.size());

                m_stats.instancedDrawCalls++;
            }
        }
    }

    void Renderer::DrawTransparent() {
        // Sort back-to-front (furthest first)
        std::sort(m_transparentQueue.begin(), m_transparentQueue.end(),
            [](const DrawCommand& a, const DrawCommand& b) {
                return a.distanceToCamera > b.distanceToCamera;
        });

        // Enable blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);  // Don't write to depth buffer

        for (const DrawCommand& cmd : m_transparentQueue) {
            Shader* shader = cmd.material->shader.get();
            shader->Enable();

            // Set uniforms
            SetCommonUniforms(shader);
            SetMaterialUniforms(cmd.material);
            shader->SetUniform("uModel", cmd.transform->modelMatrix);
            shader->SetUniform("uUseInstancing", false);

            shader->SetUniform("uLightPos", m_cameraPosition);
            shader->SetUniform("uLightColor", vec3(1, 1, 1));

            // Draw
            cmd.mesh->Draw();
            m_stats.drawCalls++;
        }

        // Restore state
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // ========== Framebuffer Management ==========

    void Renderer::InitializeFramebuffer(int width, int height) {
        if (m_framebuffer != 0) {
            CleanupFramebuffer();
        }

        m_framebufferWidth = width;
        m_framebufferHeight = height;

        // Create framebuffer
        glGenFramebuffers(1, &m_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

        // Create color texture
        glGenTextures(1, &m_colorTexture);
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

        // Create depth texture (useful for shadow mapping later)
        glGenTextures(1, &m_depthTexture);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);

        // Check completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ENGINE_THROW("Framebuffer not complete!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Create screen quad for post-processing
        CreateScreenQuad();
    }

    void Renderer::ResizeFramebuffer(int width, int height) {
        if (width == m_framebufferWidth && height == m_framebufferHeight) return;
        InitializeFramebuffer(width, height);
    }

    void Renderer::CleanupFramebuffer() {
        if (m_colorTexture) glDeleteTextures(1, &m_colorTexture);
        if (m_depthTexture) glDeleteTextures(1, &m_depthTexture);
        if (m_framebuffer) glDeleteFramebuffers(1, &m_framebuffer);
        if (m_screenQuadVAO) glDeleteVertexArrays(1, &m_screenQuadVAO);
        if (m_screenQuadVBO) glDeleteBuffers(1, &m_screenQuadVBO);

        m_colorTexture = 0;
        m_depthTexture = 0;
        m_framebuffer = 0;
        m_screenQuadVAO = 0;
        m_screenQuadVBO = 0;
    }

    void Renderer::CreateScreenQuad() {
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &m_screenQuadVAO);
        glGenBuffers(1, &m_screenQuadVBO);
        glBindVertexArray(m_screenQuadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_screenQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    void Renderer::BeginFramebufferPass() {
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    }

    void Renderer::EndFramebufferPass() {
        // Bind default framebuffer and render the color texture to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        // TODO: Use a post-process shader here
        // For now, just blit the texture to screen
        // You'll need a simple passthrough shader for this

        glBindVertexArray(m_screenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        // shader->Enable();
        // shader->SetUniform("screenTexture", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
    }

    // ========== Frustum Culling ==========

    void Renderer::ExtractFrustumPlanes() {
        // Extract frustum planes from projection * view matrix
        // Planes are in the form: Ax + By + Cz + D = 0
        const mat4& m = m_projViewMatrix;

        // Left plane
        m_frustum.planes[0] = vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]);

        // Right plane
        m_frustum.planes[1] = vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]);

        // Bottom plane
        m_frustum.planes[2] = vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]);

        // Top plane
        m_frustum.planes[3] = vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]);

        // Near plane
        m_frustum.planes[4] = vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]);

        // Far plane
        m_frustum.planes[5] = vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);

        // Normalize planes
        for (int i = 0; i < 6; ++i) {
            float length = glm::length(vec3(m_frustum.planes[i]));
            m_frustum.planes[i] /= length;
        }
    }

    bool Renderer::IsBoxInFrustum(const BBox& bbox, const mat4& modelMatrix) const {
        // Transform bounding box to world space
        vec3 corners[8] = {
            vec3(bbox.min.x, bbox.min.y, bbox.min.z),
            vec3(bbox.max.x, bbox.min.y, bbox.min.z),
            vec3(bbox.min.x, bbox.max.y, bbox.min.z),
            vec3(bbox.max.x, bbox.max.y, bbox.min.z),
            vec3(bbox.min.x, bbox.min.y, bbox.max.z),
            vec3(bbox.max.x, bbox.min.y, bbox.max.z),
            vec3(bbox.min.x, bbox.max.y, bbox.max.z),
            vec3(bbox.max.x, bbox.max.y, bbox.max.z)
        };

        // Transform all corners to world space
        for (int i = 0; i < 8; ++i) {
            vec4 worldPos = modelMatrix * vec4(corners[i], 1.0f);
            corners[i] = vec3(worldPos) / worldPos.w;
        }

        // Check each plane
        for (int p = 0; p < 6; ++p) {
            int insideCount = 0;

            // Check all 8 corners against this plane
            for (int c = 0; c < 8; ++c) {
                float distance = glm::dot(vec3(m_frustum.planes[p]), corners[c]) + m_frustum.planes[p].w;
                if (distance > 0.0f) {
                    insideCount++;
                }
            }

            // If all corners are outside this plane, the box is outside the frustum
            if (insideCount == 0) {
                return false;
            }
        }

        // Box intersects or is inside frustum
        return true;
    }

} // namespace Engine