#pragma once
#include <engine/api.hpp>
#include <engine/types.hpp>

namespace Engine {
    struct IResource {
    public:
        ENGINE_API virtual ~IResource() = default;
        
        ENGINE_API const std::filesystem::path& getPath() const { return m_path; }
        
        std::filesystem::path m_path;
    };

    class Material; // forward decl for Shader
    class Texture; // forward decl for Shader
    struct Shader : public IResource {
        enum class TextureSlot {
            DIFFUSE = 0,
            SPECULAR = 1,
            NORMAL = 2
        };

        unsigned int program = 0;
        
        ENGINE_API u32 GetUniformLoc(const std::string& name);

        ENGINE_API void SetUniform(const std::string& name, float v);
        ENGINE_API void SetUniform(const std::string& name, vec2& v);
        ENGINE_API void SetUniform(const std::string& name, vec3& v);
        ENGINE_API void SetUniform(const std::string& name, vec4& v);
        ENGINE_API void SetUniform(const std::string& name, mat4& v);
        
        ENGINE_API void SetUniform(const std::string& name, const Texture& tex, TextureSlot slot);
        ENGINE_API void SetUniform(Material& v);

        ENGINE_API void Enable() const;
        ENGINE_API ~Shader();
    };

    struct Image : public IResource {
    public:
        int width = 0, height = 0, channels = 0;
        unsigned char* data = nullptr;
        
        ENGINE_API ~Image();
    };

    struct Texture : IResource {
        u32 id = 0;
        int width = 0, height = 0;

        Texture() = default;
        ENGINE_API Texture(const Image& img);
        ENGINE_API ~Texture();

        // No copies
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        // Moveable
        ENGINE_API Texture(Texture&& other) noexcept;
        ENGINE_API Texture& operator=(Texture&& other) noexcept;
    };

    struct Material {
        glm::vec3 diffuseColor{ 1.0f };
        glm::vec3 specularColor{ 1.0f };
        float shininess = 32.0f;

        std::shared_ptr<Texture> diffuse;
        std::shared_ptr<Texture> specular;
        std::shared_ptr<Texture> normal;

        std::shared_ptr<Shader> shader = nullptr;

        Material() = default;
        ENGINE_API ~Material() = default;
    };

    struct Vertex {
        vec3 position;
        vec3 normal;
        vec2 uv;
        vec3 tangent;

        ENGINE_API static void SetupVAO();
    };

    struct Mesh {
        u32 vao = 0;
        u32 vbo = 0;
        u32 ebo = 0;
        u32 indicesCount = 0;

        ENGINE_API void Bind() const;
        ENGINE_API void Draw() const;

        ENGINE_API Mesh(std::vector<Vertex>& vertices, std::vector<u32>& indices);
        ENGINE_API ~Mesh();
    };

    struct Model : public IResource {
    public:
        struct MeshEntry {
            Mesh* mesh;
            Material* material;
        };

        struct BlueprintNode {
            string name;
            entity_id parent; // model relative parent
            unsigned int collectionIndex;
            Component::Transform transform;
        };

        using MeshCollection = std::vector<MeshEntry>;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<MeshCollection> collections;

        std::vector<BlueprintNode> blueprint;
        
        // We'll have ECS::Instantiate(entity_id parent = null, Component::Transform transform = Component::Transform(), Model& model)
        ENGINE_API ~Model() = default;
    };

    namespace Component {
        struct Drawable3D {
            std::shared_ptr<Model> model;
            u32 collectionIndex;
            ENGINE_API const Model::MeshCollection& GetCollection() const {
                return model->collections[collectionIndex];
            }
        };
    }

    template<typename T>
    struct ResourceLoader {
        static std::shared_ptr<T> load(const std::filesystem::path& path);
    };

    extern template struct ResourceLoader<Image>;
    extern template struct ResourceLoader<Texture>;
    extern template struct ResourceLoader<Shader>;
    extern template struct ResourceLoader<Model>;

	class ResourceSystem {
    public:
        // Load a resource (or get cached version)
        template<typename T>
        std::shared_ptr<T> load(const std::filesystem::path& path) {
            auto key = makeCacheKey<T>(path);
            
            // Check cache
            auto it = m_cache.find(key);
            if (it != m_cache.end()) {
                return std::static_pointer_cast<T>(it->second);
            }
            
            // Load new resource using specialized loader
            auto resource = ResourceLoader<T>::load(path);
            if (resource) {
                resource->m_path = path;
                m_cache[key] = resource;
            }
            
            return resource;
        }

        // For resources that don't come from a single file
        template<typename T>
        std::shared_ptr<T> create(const std::string& name) {
            auto key = makeCacheKey<T>(name);
            
            auto it = m_cache.find(key);
            if (it != m_cache.end()) {
                return std::static_pointer_cast<T>(it->second);
            }
            
            auto resource = std::make_shared<T>();
            m_cache[key] = resource;
            return resource;
        }

        // Manually add a resource to cache
        template<typename T>
        void cache(const std::string& name, std::shared_ptr<T> resource) {
            auto key = makeCacheKey<T>(name);
            m_cache[key] = resource;
        }

        ENGINE_API void clear() {
            m_cache.clear();
        }

        ENGINE_API std::unordered_map<std::string, std::shared_ptr<IResource>>& get_cache() { return m_cache; }

    private:
        template<typename T>
        std::string makeCacheKey(const std::filesystem::path& path) {
            return std::string(typeid(T).name()) + "|" + path.string();
        }

        std::unordered_map<std::string, std::shared_ptr<IResource>> m_cache;
    };
}