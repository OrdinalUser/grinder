#pragma once

#include <engine/api.hpp>
#include <engine/types.hpp>
#include <engine/resource.hpp>
#include <glad/glad.h>

namespace Engine {
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

        ENGINE_API Renderer();
        ENGINE_API ~Renderer();

        // Debug stats
        struct Stats {
            size_t drawCalls = 0;
            size_t instancedDrawCalls = 0;
            size_t totalObjects = 0;
            size_t batchCount = 0;
            size_t culledObjects = 0;
        };
        ENGINE_API const Stats& GetStats() const { return m_stats; }

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

        // Framebuffer for post-processing & shadow mapping
        GLuint m_framebuffer = 0;
        GLuint m_colorTexture = 0;
        GLuint m_depthTexture = 0;
        GLuint m_screenQuadVAO = 0;
        GLuint m_screenQuadVBO = 0;
        bool m_useFramebuffer = false;
        int m_framebufferWidth = 0;
        int m_framebufferHeight = 0;
        
        // Batching & instancing
        GLuint m_ssbo = 0;

        GLuint m_instancesSSBO;
        GLuint m_visibilitySSBO;
        GLuint m_frustumUBO;

        ComputeShader* m_cullShader;

        // Stats
        Stats m_stats;

        // ========== Private Methods ==========

        void ProcessLights();
        void SetLightUniforms(Shader* shader);
        void SetCommonUniforms(Shader* shader);
        void SetMaterialUniforms(Material* material);

        void DrawOpaque();
        void DrawTransparent();

        void InitializeFramebuffer(int width, int height);
        void ResizeFramebuffer(int width, int height);
        void CleanupFramebuffer();
        void CreateScreenQuad();

        void ExtractFrustumPlanes();
        bool IsBoxInFrustum(const BBox& bbox, const mat4& modelMatrix) const;
        void ProcessQueue();

        void BeginFramebufferPass();
        void EndFramebufferPass();
    };
}