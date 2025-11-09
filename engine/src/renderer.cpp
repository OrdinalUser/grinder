#include <engine/renderer.hpp>
#include <engine/resource.hpp>
#include <engine/application.hpp>
#include <algorithm>

namespace Engine {

    // ========== Public API ==========

    void Renderer::SetCamera(Transform* transform, Camera* camera) {
        m_cameraTransform = transform;
        m_camera = camera;
        m_hasCameraSet = (transform != nullptr && camera != nullptr);

        if (m_hasCameraSet) {
            m_projViewMatrix = camera->projectionMatrix * camera->viewMatrix;
            m_cameraPosition = transform->position;
        }
    }

    void Renderer::Queue(Transform* transform, Mesh* mesh, Material* material) {
        if (!mesh || !material || !material->shader) return;

        // Determine if transparent
        if (material->isTransparent) {
            // Calculate distance to camera for sorting
            float distance = 0.0f;
            if (m_hasCameraSet) {
                vec3 objPos = transform->position;
                distance = glm::length(m_cameraPosition - objPos);
            }

            DrawCommand cmd;
            cmd.transform = transform;
            cmd.mesh = mesh;
            cmd.material = material;
            cmd.distanceToCamera = distance;
            m_transparentQueue.push_back(cmd);
        }
        else {
            // Batch opaque objects
            BatchKey key{ mesh, material, material->shader.get() };

            auto& batch = m_opaqueBatches[key];
            batch.mesh = mesh;
            batch.material = material;
            batch.modelMatrices.push_back(transform->modelMatrix);
        }
    }

    void Renderer::QueueLight(Transform* transform, Light* light) {
        if (!transform || !light) return;
        m_queuedLights.emplace_back(transform, light);
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

        // Process lights into world space
        ProcessLights();

        // Optional: Render to framebuffer for post-processing
        if (m_useFramebuffer) {
            BeginFramebufferPass();
        }

        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Render opaque geometry
        DrawOpaque();

        // Render transparent geometry
        if (!m_transparentQueue.empty()) {
            DrawTransparent();
        }

        // Optional: Apply post-processing
        if (m_useFramebuffer) {
            EndFramebufferPass();
        }
    }

    void Renderer::Clear() {
        m_opaqueBatches.clear();
        m_transparentQueue.clear();
        m_queuedLights.clear();
        m_processedLights.clear();
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
        shader->SetUniform("uLightCount", static_cast<int>(m_processedLights.size()));

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
        }
    }

    void Renderer::SetCommonUniforms(Shader* shader) {
        shader->SetUniform("uProjView", m_projViewMatrix);
        shader->SetUniform("uViewPos", m_cameraPosition);
        SetLightUniforms(shader);
    }

    void Renderer::SetMaterialUniforms(Material* material) {
        Shader* shader = material->shader.get();

        // Set material properties
        shader->SetUniform("uMaterial.diffuseColor", material->diffuseColor);
        shader->SetUniform("uMaterial.specularColor", material->specularColor);
        shader->SetUniform("uMaterial.shininess", material->shininess);

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
                key.mesh->Draw();
                m_stats.drawCalls++;
            }
            else {
                // Multiple objects - instanced draw (TODO: implement instancing)
                // For now, draw individually
                for (const mat4& modelMatrix : batch.modelMatrices) {
                    shader->SetUniform("uModel", modelMatrix);
                    key.mesh->Draw();
                    m_stats.drawCalls++;
                }
                // m_stats.instancedDrawCalls++; // When instancing is implemented
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

} // namespace Engine