#include <engine/renderer.hpp>
#include <engine/resource.hpp>
#include <engine/application.hpp>
#include <engine/perf_profiler.hpp>
#include <algorithm>

constexpr static struct {
    float BloomStrength = 1.2f;
    float BrightnessThreshold = 1.0f;
} RendererConfig;

static const char* GLErrorToString(GLenum err) {
    switch (err) {
        case GL_NO_ERROR:                      return "GL_NO_ERROR";
        case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW:                return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW:               return "GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default:                               return "GL_UNKNOWN_ERROR";
    }
}

inline void GLCheckError(const char* msg) {
    for (GLenum err; (err = glGetError()) != GL_NO_ERROR; ) {
        printf("[GL ERROR] %s: %s (0x%X)\n", msg, GLErrorToString(err), err);
    }
}

namespace Engine {
    // Helper utility to get OpenGL format details from our enum
    static void GetTextureFormatDetails(Framebuffer::TextureFormat format, GLenum& internalFormat, GLenum& dataFormat, GLenum& type) {
        switch (format) {
        case Framebuffer::TextureFormat::RGB:
            internalFormat = GL_RGB8;
            dataFormat = GL_RGB;
            type = GL_UNSIGNED_BYTE;
            return;
        case Framebuffer::TextureFormat::RGBA:
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            return;
        case Framebuffer::TextureFormat::RGBA16F:
            internalFormat = GL_RGBA16F;
            dataFormat = GL_RGBA;
            type = GL_FLOAT;
            return;
        case Framebuffer::TextureFormat::DEPTH24_STENCIL8:
            internalFormat = GL_DEPTH24_STENCIL8;
            dataFormat = GL_DEPTH_STENCIL;
            type = GL_UNSIGNED_INT_24_8;
            return;
        default:
            // Should not happen
            internalFormat = 0;
            dataFormat = 0;
            type = 0;
            return;
        }
    }

    Framebuffer::Framebuffer(uint32_t width, uint32_t height) : m_Width{ width }, m_Height{ height } { }

    Framebuffer::~Framebuffer() { Release(); }

    Framebuffer& Framebuffer::AddColorAttachment(const AttachmentSpecification& spec) {
        m_ColorAttachmentSpecs.push_back(spec);
        return *this;
    }

    Framebuffer& Framebuffer::SetDepthAttachment(const AttachmentSpecification& spec) {
        m_DepthAttachmentSpec = spec;
        return *this;
    }

    void Framebuffer::Build() {
        Invalidate();
    }

    void Framebuffer::Bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);
        // glViewport(0, 0, m_Width, m_Height);
    }

    void Framebuffer::Unbind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            ENGINE_THROW("Attempting to resize framebuffer to zero size");
            return;
        }
        // No need to resize
        if (m_Width == width && m_Height == height) return;
        m_Width = width;
        m_Height = height;
        Invalidate();
    }

    std::shared_ptr<Texture> Framebuffer::GetColorAttachment(uint32_t index) const {
        if (index >= m_ColorAttachments.size()) {
            throw std::out_of_range("Color attachment index out of range.");
        }
        return m_ColorAttachments[index];
    }

    std::shared_ptr<Texture> Framebuffer::GetDepthAttachment() const {
        if (m_DepthAttachment == nullptr) {
            ENGINE_THROW("Attempting to fetch unbound depth attachment");
        }
        return m_DepthAttachment;
    }

    void Framebuffer::Release() {
        if (m_FramebufferID) {
            glDeleteFramebuffers(1, &m_FramebufferID);
            m_ColorAttachments.clear();
            m_DepthAttachment.reset();
            m_FramebufferID = 0;
        }
    }

    void Framebuffer::Invalidate() {
        // Drop a framebuffer if exists
        if (m_FramebufferID) {
            Release();
        }

        // Build our framebuffer from scratch
        glGenFramebuffers(1, &m_FramebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);
        glViewport(0, 0, m_Width, m_Height);

        // Create and attach color components
        if (!m_ColorAttachmentSpecs.empty()) {
            m_ColorAttachments.resize(m_ColorAttachmentSpecs.size());
            for (size_t i = 0; i < m_ColorAttachmentSpecs.size(); i++) {
                const auto& spec = m_ColorAttachmentSpecs[i];
                GLenum internalFormat, dataFormat, type;
                GetTextureFormatDetails(spec.Format, internalFormat, dataFormat, type);

                // Create the texture from specification
                auto texture = std::make_shared<Texture>();
                texture->width = m_Width;
                texture->height = m_Height;
                glGenTextures(1, &texture->id);
                glBindTexture(GL_TEXTURE_2D, texture->id);
                glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, dataFormat, type, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLuint>(spec.Filtering.Min));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLuint>(spec.Filtering.Min));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLuint>(spec.Wrapping.S));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLuint>(spec.Wrapping.T));

                // Attach and save
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, texture->id, 0);
                m_ColorAttachments[i] = texture;
            }
        }

        // Create and attach our depth components
        if (m_DepthAttachmentSpec.has_value()) {
            const auto& spec = m_DepthAttachmentSpec.value();
            GLenum internalFormat, dataFormat, type;
            GetTextureFormatDetails(spec.Format, internalFormat, dataFormat, type);

            // Create the texture from specification
            auto texture = std::make_shared<Texture>();
            texture->width = m_Width;
            texture->height = m_Height;
            glGenTextures(1, &texture->id);
            glBindTexture(GL_TEXTURE_2D, texture->id);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, dataFormat, type, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLuint>(spec.Filtering.Min));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLuint>(spec.Filtering.Min));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLuint>(spec.Wrapping.S));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLuint>(spec.Wrapping.T));

            // Attach and save
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture->id, 0);
            m_DepthAttachment = texture;
        }

        // Set draw buffers
        if (m_ColorAttachments.size() > 1) {
            std::vector<GLenum> buffers;
            for (size_t i = 0; i < m_ColorAttachments.size(); ++i) {
                buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
            }
            glDrawBuffers(buffers.size(), buffers.data());
        }
        else if (m_ColorAttachments.empty()) {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        // Unbind for housekeeping
        Unbind();
    }

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

    void Renderer::OnResize(unsigned int width, unsigned int height) {
        m_Framebuffer->Resize(width, height);
        m_postProcessBrightFBO->Resize(width, height);
        m_postProcessPongFBO[0]->Resize(width, height);
        m_postProcessPongFBO[1]->Resize(width, height);
    }

    ENGINE_API Renderer::Renderer() {
        Ref<VFS> vfs = Engine::Application::Get().GetVFS();
        Ref<ResourceSystem> rs = Engine::Application::Get().GetResourceSystem();
        Window& window = Engine::Application::Get().GetWindow();

        // Drawing
        glGenBuffers(1, &m_ssbo); // Prepare our reusable ssbo for instancing
        glGenBuffers(1, &m_instancesSSBO);
        glGenBuffers(1, &m_visibilitySSBO);
        glGenBuffers(1, &m_frustumUBO);

        // Main framebuffer
        // InitializeFramebuffer(window.GetWidth(), window.GetHeight());
        m_Framebuffer = new Framebuffer(window.GetWidth(), window.GetHeight());
        m_Framebuffer->AddColorAttachment()
            .SetDepthAttachment()
            .Build();
    
        // Create screen quad for post-processing
        CreateScreenQuad();

        //// Bloom post-processing
        // Bright Extract
        m_postProcessBrightFBO = new Framebuffer(window.GetWidth(), window.GetHeight());
        m_postProcessBrightFBO->AddColorAttachment()
            .Build();

        m_postProcessPongFBO[0] = new Framebuffer(window.GetWidth(), window.GetHeight());
        m_postProcessPongFBO[0]->AddColorAttachment()
            .Build();
        m_postProcessPongFBO[1] = new Framebuffer(window.GetWidth(), window.GetHeight());
        m_postProcessPongFBO[1]->AddColorAttachment()
            .Build();

        // Shaders and other
        m_cullShader = new ComputeShader(vfs->GetEngineResourcePath("assets/shaders/culling.glsl"));
        m_postProcessingShader = rs->load<Shader>(vfs->GetEngineResourcePath("assets/shaders/postprocess"));
        m_brightPassShader = rs->load<Shader>(vfs->GetEngineResourcePath("assets/shaders/postprocess_bright_extract"));
        m_blurShader = rs->load<Shader>(vfs->GetEngineResourcePath("assets/shaders/postprocess_blur"));
    }

    ENGINE_API Renderer::~Renderer() {
        glDeleteBuffers(1, &m_ssbo);
        delete m_cullShader;
        glDeleteBuffers(1, &m_ssbo);
        glDeleteBuffers(1, &m_instancesSSBO);
        glDeleteBuffers(1, &m_visibilitySSBO);
        glDeleteBuffers(1, &m_frustumUBO);

        delete m_Framebuffer;
        delete m_postProcessBrightFBO;
        delete m_postProcessPongFBO[0];
        delete m_postProcessPongFBO[1];
        if (m_screenQuadVAO) glDeleteVertexArrays(1, &m_screenQuadVAO);
        if (m_screenQuadVBO) glDeleteBuffers(1, &m_screenQuadVBO);

        m_Framebuffer = nullptr;
        m_screenQuadVAO = 0;
        m_screenQuadVBO = 0;
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
        m_stats.totalObjects = m_gpuInstances.size();

        // Run global culling
        ProcessQueue();

        BeginFramebufferPass();

        // DepthPrepass
        // TODO!

        // Process lights into world space, clustered culling TODO
        ProcessLights(); // TODO!

        // Render opaque geometry
        DrawOpaque();

        // Render transparent geometry
        if (!m_transparentQueue.empty()) {
            DrawTransparent();
        }

        // Shadows, here?

        EndFramebufferPass();
    }

    void Renderer::Clear() {
        m_opaqueBatches.clear();
        m_transparentQueue.clear();
        m_queuedLights.clear();
        m_processedLights.clear();
        m_gpuInstanceData.clear();
        m_gpuInstances.clear();
        if (m_Stats.size() > 10) m_Stats.pop_back();
        m_Stats.insert(m_Stats.begin(), m_stats);
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
        shader->SetUniform("uLightPos", vec3(30, 100, 0));
        shader->SetUniform("uLightColor", vec3(0.19f, 0.19f, 0.64f)); // moonlight color, should be less intense! TODO!
        // shader->SetUniform("uLightColor", vec3(1.0f));
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
            shader->SetUniform("uMaterial.emmisiveIntensity", material->emmisiveIntensity);
            shader->SetUniform("uMaterial.emmisiveColor", material->emmisiveColor);
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
            if (material->emmisive && (material->emmisive->id)) {
                shader->SetUniform("uMaterial.emmisiveMap", *material->emmisive, Shader::TextureSlot::EMMISIVE);
            }
            shader->SetUniform("uMaterial.shininess", material->shininess);
            shader->SetUniform("uMaterial.emmisiveIntensity", material->emmisiveIntensity);
            shader->SetUniform("uMaterial.emmisiveColor", material->emmisiveColor);
        }
    }

    // ========== Drawing ==========

    void Renderer::DrawOpaque() {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);

        m_stats.batchCount = m_opaqueBatches.size();

        for (const auto& [key, batch] : m_opaqueBatches) {
            Shader* shader = key.shader;
            shader->Enable();

            // Set common uniforms once per batch
            SetCommonUniforms(shader);
            SetMaterialUniforms(key.material);

            if (batch.modelMatrices.size() == 1) {
                // Single object - standard draw
                shader->SetUniform("uModel", batch.modelMatrices[0]);
                shader->SetUniform("uUseInstancing", false);
                key.mesh->Draw();
                m_stats.drawCalls++;
                m_stats.drawnObjects++;
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
                m_stats.drawnObjects += batch.modelMatrices.size();
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

            // Draw
            cmd.mesh->Draw();
            m_stats.drawCalls++;
        }

        // Restore state
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // ========== Framebuffer Management ==========

    void Renderer::CreateScreenQuad() {
        // positions (clip space) + uv
        static constexpr float quadVertices[] = {
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
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        // attribute 0 = position (vec2)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // attribute 1 = uv (vec2)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void Renderer::BeginFramebufferPass() {
        // glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        m_Framebuffer->Bind();
        // glEnable(GL_FRAMEBUFFER_SRGB);

        // Clear this framebuffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Window& window = Application::Get().GetWindow();
        /*if (m_framebufferWidth != window.GetWidth() || m_framebufferHeight != window.GetHeight())
            ResizeFramebuffer(window.GetWidth(), window.GetHeight());*/
        // glViewport managed outside
    }

    void Renderer::EndFramebufferPass() {
        PERF_BEGIN("Render_PostProcess");
        RunPostProcessPipeline();
        PERF_END("Render_PostProcess");

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }

    void Renderer::RunPostProcessPipeline() {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        // Scene output
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_Framebuffer->GetColorAttachment(0)->id);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_Framebuffer->GetDepthAttachment()->id);

        //// Bloom
        // 1. Bright-pass (extract bright areas)
        m_brightPassShader->Enable(); // *global* Shader for extracting bright pixels
        m_brightPassShader->SetUniform("uSceneTexture", 0);
        m_brightPassShader->SetUniform("uThreshold", RendererConfig.BrightnessThreshold); // Brightness threshold

        // glBindFramebuffer(GL_FRAMEBUFFER, m_brightPassFBO); // *global* FBO for bright-pass output
        m_postProcessBrightFBO->Bind();
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(m_screenQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 2. Blur (ping-pong between two FBOs for multiple passes)
        bool horizontal = true;
        int blurPasses = 10; // Number of blur iterations (5 horizontal + 5 vertical)

        m_blurShader->Enable(); // *global* Shader for Gaussian blur

        for (int i = 0; i < blurPasses; i++) {
            // glBindFramebuffer(GL_FRAMEBUFFER, m_bloomPingPongFbos[horizontal ? 1 : 0]); // *global* Two FBOs for ping-pong blur
            m_postProcessPongFBO[horizontal ? 1 : 0]->Bind();
            m_blurShader->SetUniform("uHorizontal", horizontal ? 1 : 0);

            // First pass reads from bright-pass, rest read from previous blur
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, i == 0 ? m_postProcessBrightFBO->GetColorAttachment(0)->id : m_postProcessPongFBO[horizontal ? 0 : 1]->GetColorAttachment(0)->id); // *global* Textures attached to ping-pong FBOs
            m_blurShader->SetUniform("uTexture", 0);

            glBindVertexArray(m_screenQuadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            horizontal = !horizontal;
        }

        // Final composite (blend original scene with bloom)
        m_postProcessingShader->Enable();
        m_postProcessingShader->SetUniform("uSceneTexture", 0); // Original scene
        m_postProcessingShader->SetUniform("uBloomTexture", 1); // Blurred bright areas
        m_postProcessingShader->SetUniform("uBloomStrength", RendererConfig.BloomStrength); // Bloom intensity

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_Framebuffer->GetColorAttachment()->id);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_postProcessPongFBO[!horizontal ? 1 : 0]->GetColorAttachment(0)->id); // Final blurred result
        // glBindTexture(GL_TEXTURE_2D, m_brightPassTexture);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);

        glBindVertexArray(m_screenQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
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