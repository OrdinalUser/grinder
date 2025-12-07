#pragma once

#include <engine/api.hpp>
#include <engine/types.hpp>
#include <engine/resource.hpp>
#include <glad/glad.h>

namespace Engine {
    class Framebuffer {
    public:
        enum class TextureFormat : u32 {
            None = 0,
            RGB = GL_RGB,
            RGBA = GL_RGBA8,
            RGBA16F = GL_RGBA16F,
            Color = RGBA16F,
            DEPTH24_STENCIL8 = GL_DEPTH24_STENCIL8,
            Depth = DEPTH24_STENCIL8
        };

        enum class TextureFiltering : u32 {
            Nearest = GL_NEAREST,
            Linear = GL_LINEAR,
        };

        enum class TextureWrap : u32 {
            Clamp = GL_CLAMP_TO_EDGE
        };

        struct AttachmentSpecification {
            TextureFormat Format = TextureFormat::Color;
            struct {
                TextureFiltering Min = TextureFiltering::Linear;
                TextureFiltering Max = TextureFiltering::Linear;
            } Filtering;
            struct {
                TextureWrap S = TextureWrap::Clamp;
                TextureWrap T = TextureWrap::Clamp;
            } Wrapping;
        };

    public:
        Framebuffer(uint32_t width, uint32_t height);
        ~Framebuffer();

        Framebuffer& AddColorAttachment(const AttachmentSpecification& spec = { });
        Framebuffer& SetDepthAttachment(const AttachmentSpecification& spec = { .Format = TextureFormat::Depth });

        void Build();

        inline void Bind() const;
        inline void Unbind() const;

        void Resize(uint32_t width, uint32_t height);

        std::shared_ptr<Texture> GetColorAttachment(uint32_t index = 0) const;
        std::shared_ptr<Texture> GetDepthAttachment() const;

        inline uint32_t GetWidth() const { return m_Width; }
        inline uint32_t GetHeight() const { return m_Height; }

    private:
        void Invalidate();
        void Release();

        uint32_t m_FramebufferID = 0;
        uint32_t m_Width, m_Height;

        std::vector<AttachmentSpecification> m_ColorAttachmentSpecs;
        std::optional<AttachmentSpecification> m_DepthAttachmentSpec;

        std::vector<std::shared_ptr<Texture>> m_ColorAttachments;
        std::shared_ptr<Texture> m_DepthAttachment;
    };

    struct GlState {
        Color clearColor = Color(0.00455, 0.00455, 0.00455, 1.0);
    };

    class Renderer {
        using Transform = Component::Transform;
        using Camera = Component::Camera;
        using Light = Component::Light;
        using Drawable3D = Component::Drawable3D;

    public:
        ENGINE_API void SetCamera(Transform* transform, Camera* camera);
        ENGINE_API void Queue(Transform* transform, Mesh* mesh, Material* material);
        ENGINE_API void QueueDrawable3D(Transform* transform, Drawable3D* drawable);
        ENGINE_API void QueueLight(Transform* transform, Light* light);
        ENGINE_API void Draw();
        ENGINE_API void Clear();
        ENGINE_API void OnResize(unsigned int width, unsigned int height);

        ENGINE_API void SetClearColor(const Color clearColor);
        ENGINE_API void LoadSkybox(const path filepath, const std::string ext = ".png");
        ENGINE_API void LoadSkybox(const array<std::filesystem::path, 6>& faces);

        ENGINE_API Renderer();
        ENGINE_API ~Renderer();

        struct Stats {
            size_t drawCalls = 0;
            size_t instancedDrawCalls = 0;
            size_t totalObjects = 0;
            size_t batchCount = 0;
            size_t culledObjects = 0;
            size_t drawnObjects = 0;
        };
        ENGINE_API const std::list<Stats>& GetStats() const { return m_Stats; }

    private:
        struct ComputeShader {
            ComputeShader(const std::filesystem::path& filepath);
            ~ComputeShader();
            unsigned int program;
        };

        struct DrawCommand {
            Transform* transform;
            Mesh* mesh;
            Material* material;
            float distanceToCamera;
        };

        struct BatchKey {
            Mesh* mesh;
            Material* material;
            Shader* shader;

            bool operator==(const BatchKey& other) const {
                return mesh == other.mesh && material == other.material && shader == other.shader;
            }
        };

        struct BatchKeyHash {
            size_t operator()(const BatchKey& key) const {
                size_t h1 = std::hash<void*>{}(key.mesh);
                size_t h2 = std::hash<void*>{}(key.material);
                size_t h3 = std::hash<void*>{}(key.shader);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };

        struct InstanceBatch {
            Mesh* mesh;
            Material* material;
            std::vector<mat4> modelMatrices;
        };

        struct DrawInstance {
            Transform* transform;
            Mesh* mesh;
            Material* material;
        };

        struct Frustum {
            vec4 planes[6];
        } m_frustum;

        // GPU Light Data, std 430 aligned
        struct GPU_LightData {
            vec4 positionAndType;
            vec4 directionAndRange;
            vec4 colorAndIntensity;
            vec4 spotAnglesRadians;
        };

        struct GPU_InstanceData {
            mat4 modelMatrix;
            BSphere bSphere;
        };

        // Camera
        Transform* m_cameraTransform = nullptr;
        Camera* m_camera = nullptr;
        mat4 m_projViewMatrix;
        vec3 m_cameraPosition;
        vec3 m_cameraForward;
        bool m_hasCameraSet = false;

        // Render queues
        std::vector<DrawInstance> m_gpuInstances;
        std::vector<GPU_InstanceData> m_gpuInstanceData;
        std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash> m_opaqueBatches;
        std::vector<DrawCommand> m_transparentQueue;

        // Main render buffer
        Framebuffer* m_Framebuffer;

        // Post-process framebuffers
        Framebuffer* m_postProcessPongFBO[2];
        Framebuffer* m_postProcessBrightFBO;

        GLuint m_screenQuadVAO = 0;
        GLuint m_screenQuadVBO = 0;

        // Culling
        ComputeShader* m_cullShader;
        GLuint m_instanceSSBO = 0;
        GLuint m_instancesSSBO;
        GLuint m_visibilitySSBO;
        GLuint m_frustumUBO;

        // Tiled Deferred Light Processing
        std::vector<std::pair<Transform*, Light*>> m_queuedLights;
        std::vector<GPU_LightData> m_processedLights;
        ComputeShader* m_lightCullShader;
        GLuint m_lightsSSBO;
        GLuint m_lightGridSSBO;
        GLuint m_lightIndicesSSBO;

        // Shaders
        std::shared_ptr<Shader> m_postProcessingShader;
        std::shared_ptr<Shader> m_brightPassShader;
        std::shared_ptr<Shader> m_blurShader;
        std::shared_ptr<Shader> m_depthPrepassShader;

        // Stats
        Stats m_stats;
        std::list<Stats> m_Stats;

        // Other
        GlState m_glState;

        // Skybox
        GLuint m_skyboxVAO = 0;
        GLuint m_skyboxVBO = 0;
        std::shared_ptr<Shader> m_skyboxShader = nullptr;
        GLuint m_skyboxCubemap = 0;

        // Private helper methods
        void ProcessLights();

        void SetCommonUniforms(Shader* shader);
        void SetLightUniforms(Shader* shader);
        void SetMaterialUniforms(Material* material);

        void DrawDepthPrepass();
        void DrawOpaque();
        void DrawTransparent();

        void CreateScreenQuad();
        void ExtractFrustumPlanes();
        bool IsBoxInFrustum(const BBox& bbox, const mat4& modelMatrix) const;
        void ProcessQueue();

        void BeginFramebufferPass();
        void RunPostProcessPipeline();
        void EndFramebufferPass();

        void CreateSkybox();
        GLuint LoadCubemap(const array<std::filesystem::path, 6>& faces);
        void DrawSkybox();
    };
}