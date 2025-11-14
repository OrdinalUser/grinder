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
            // Color
            RGB = GL_RGB,
            RGBA = GL_RGBA8,
            RGBA16F = GL_RGBA16F, // Perfect for HDR / Bloom
            Color = RGBA16F,
            // Depth
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
        // Takes the initial size.
        Framebuffer(uint32_t width, uint32_t height);
        ~Framebuffer();

        // --- Builder Methods ---
        // Add an attachment by describing its format.
        Framebuffer& AddColorAttachment(const AttachmentSpecification& spec = { });
        Framebuffer& SetDepthAttachment(const AttachmentSpecification& spec = { .Format = TextureFormat::Depth });

        // --- Finalization ---
        // Creates all the OpenGL objects based on the attachments added.
        // Throws an exception on failure.
        void Build();

        // --- Usage ---
        inline void Bind() const;
        inline void Unbind() const;

        // Recreates the FBO with a new size, preserving the attachment configuration.
        void Resize(uint32_t width, uint32_t height);

        // --- Accessors ---
        std::shared_ptr<Texture> GetColorAttachment(uint32_t index = 0) const;
        std::shared_ptr<Texture> GetDepthAttachment() const;

        inline uint32_t GetWidth() const { return m_Width; }
        inline uint32_t GetHeight() const { return m_Height; }

    private:
        void Invalidate(); // The internal build/re-build logic
        void Release();    // The internal cleanup logic

        uint32_t m_FramebufferID = 0;
        uint32_t m_Width, m_Height;

        // The "recipes" for our attachments, stored from the builder calls
        std::vector<AttachmentSpecification> m_ColorAttachmentSpecs;
        std::optional<AttachmentSpecification> m_DepthAttachmentSpec;

        // The actual Texture objects created in Build()
        std::vector<std::shared_ptr<Texture>> m_ColorAttachments;
        std::shared_ptr<Texture> m_DepthAttachment;
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

        ENGINE_API Renderer();
        ENGINE_API ~Renderer();

        // Debug stats
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
        // ========== Data Structures ==========

        struct ComputeShader {
            ComputeShader(const std::filesystem::path& filepath);
            ~ComputeShader();

            unsigned int program;
        };

        struct DrawCommand {
            Transform* transform;
            Mesh* mesh;
            Material* material;
            float distanceToCamera;  // For transparency sorting
        };

        struct BatchKey {
            Mesh* mesh;
            Material* material;
            Shader* shader;

            bool operator==(const BatchKey& other) const {
                return mesh == other.mesh &&
                    material == other.material &&
                    shader == other.shader;
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

        struct LightData {
            Light::Type type;
            vec3 color;
            float intensity;

            vec3 position;     // Point & Spot
            vec3 direction;    // Directional & Spot

            float range;       // Point & Spot
            float innerCutoff; // Spot
            float outerCutoff; // Spot
        };

        // GPU sided struct
        struct GPU_InstanceData {
            mat4 modelMatrix;
            BSphere bSphere;
        };

        struct GPUInstance {
            Transform* transform;
            Mesh* mesh;
            Material* material;
        };

        // Frustum planes (extracted from projView matrix)
        struct Frustum {
            vec4 planes[6]; // left, right, bottom, top, near, far
        } m_frustum;

        // ========== State ==========

        // Camera
        Transform* m_cameraTransform = nullptr;
        Camera* m_camera = nullptr;
        mat4 m_projViewMatrix;
        vec3 m_cameraPosition;
        bool m_hasCameraSet = false;

        // Render queues
        std::vector<GPUInstance> m_gpuInstances;
        std::vector<GPU_InstanceData> m_gpuInstanceData;

        std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash> m_opaqueBatches;
        std::vector<DrawCommand> m_transparentQueue;

        // Light queue
        std::vector<std::pair<Transform*, Light*>> m_queuedLights;
        std::vector<LightData> m_processedLights;

        // Framebuffer
        Framebuffer* m_Framebuffer;
        Framebuffer* m_postProcessPongFBO[2];
        Framebuffer* m_postProcessBrightFBO;

        GLuint m_screenQuadVAO = 0;
        GLuint m_screenQuadVBO = 0;
        
        // Batching & instancing
        ComputeShader* m_cullShader;
        GLuint m_ssbo = 0;

        GLuint m_instancesSSBO;
        GLuint m_visibilitySSBO;
        GLuint m_frustumUBO;

        // Post-processing
        std::shared_ptr<Shader> m_postProcessingShader;
        std::shared_ptr<Shader> m_brightPassShader;
        std::shared_ptr<Shader> m_blurShader;

        // Stats
        Stats m_stats;
        std::list<Stats> m_Stats;

        // ========== Private Methods ==========

        void ProcessLights();
        void SetLightUniforms(Shader* shader);
        void SetCommonUniforms(Shader* shader);
        void SetMaterialUniforms(Material* material);

        void DrawOpaque();
        void DrawTransparent();

        void CreateScreenQuad();

        void ExtractFrustumPlanes();
        bool IsBoxInFrustum(const BBox& bbox, const mat4& modelMatrix) const;
        void ProcessQueue();

        void BeginFramebufferPass();
        void EndFramebufferPass();
        void RunPostProcessPipeline();
    };
}