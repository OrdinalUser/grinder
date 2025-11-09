#pragma once
#include <engine/api.hpp>
#include <engine/types.hpp>
#include <engine/exception.hpp>

namespace Engine {
    struct BBox {
        glm::vec3 min;
        glm::vec3 max;

        glm::vec3 center() const { return (min + max) * 0.5f; }
        glm::vec3 size() const { return max - min; }
        glm::vec3 extents() const { return size() * 0.5f; }  // Half-size

        bool contains(const glm::vec3& point) const {
            return point.x >= min.x && point.x <= max.x &&
                point.y >= min.y && point.y <= max.y &&
                point.z >= min.z && point.z <= max.z;
        }
    };

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
        
        ENGINE_API u32 GetUniformLoc(const std::string& name) const;

        ENGINE_API void SetUniform(const std::string& name, const float v) const;
        ENGINE_API void SetUniform(const std::string& name, const vec2& v) const;
        ENGINE_API void SetUniform(const std::string& name, const vec3& v) const;
        ENGINE_API void SetUniform(const std::string& name, const vec4& v) const;
        ENGINE_API void SetUniform(const std::string& name, const mat4& v) const;
        
        ENGINE_API void SetUniform(const std::string& name, const Texture& tex, const TextureSlot slot) const;
        ENGINE_API void SetUniform(const Material& v) const;

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

    struct Material : IResource {
        enum class RenderType : u8 {
            UNLIT, LIT, TEXTURED
        };

        glm::vec3 diffuseColor{ 1.0f };
        glm::vec3 specularColor{ 1.0f };
        float shininess = 32.0f;
        
        RenderType renderType;
        bool isTransparent;

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
        BBox bounds;
        
        // We'll have ECS::Instantiate(entity_id parent = null, Component::Transform transform = Component::Transform(), Model& model)
        ENGINE_API ~Model() = default;
    };

    namespace Component {
        struct Drawable3D {
            std::shared_ptr<Model> model;
            u32 collectionIndex;
            ENGINE_API const Model::MeshCollection& GetCollection() const;
        };
    }

    namespace LoadCfg {
        enum class ColorFormat {
            Auto = 0,      // Keep original format
            Grayscale = 1,
            GrayscaleAlpha = 2,
            RGB = 3,
            RGBA = 4
        };

        enum class TextureFilter {
            Nearest,
            Linear,
            NearestMipmapNearest,
            LinearMipmapNearest,
            NearestMipmapLinear,
            LinearMipmapLinear
        };

        enum class TextureWrap {
            Repeat,
            MirroredRepeat,
            ClampToEdge,
            ClampToBorder
        };

        struct Image {
            ColorFormat format = ColorFormat::RGB;
            bool flip_vertically = false;

            // Resize options (0 = no resize)
            int width = 0;
            int height = 0;
            bool maintain_aspect = false;  // If one dimension is 0, calculate from aspect ratio
        };

        // Has to inherit in a stupid way, sorry
        struct Texture {
            ColorFormat format = ColorFormat::RGB;
            bool flip_vertically = false;

            // Resize options (0 = no resize)
            int width = 0;
            int height = 0;
            bool maintain_aspect = false;  // If one dimension is 0, calculate from aspect ratio

            TextureFilter min_filter = TextureFilter::LinearMipmapLinear;
            TextureFilter mag_filter = TextureFilter::Linear;
            TextureWrap wrap_s = TextureWrap::Repeat;
            TextureWrap wrap_t = TextureWrap::Repeat;
            bool generate_mipmaps = true;
        };

        struct Model {
            bool normalize = false; // doesn't work currently
            bool static_mesh = false;
            bool flip_uvs = true;
        };

        struct Shader {
            bool unused = false;
        };
    }

    namespace ResourceLoader {
        ENGINE_API std::shared_ptr<Image> load(const std::filesystem::path& path, const LoadCfg::Image& cfg = LoadCfg::Image());
        ENGINE_API std::shared_ptr<Texture> load(const std::filesystem::path& path, const LoadCfg::Texture& cfg = LoadCfg::Texture());
        ENGINE_API std::shared_ptr<Shader> load(const std::filesystem::path& path, const LoadCfg::Shader& cfg = LoadCfg::Shader());
        ENGINE_API std::shared_ptr<Model> load(const std::filesystem::path& path, const LoadCfg::Model& cfg = LoadCfg::Model());
    }

    // Traits to get config type for each resource
    template<typename T> struct ResourceConfigTraits;
    template<> struct ResourceConfigTraits<Image> { using ConfigType = LoadCfg::Image; };
    template<> struct ResourceConfigTraits<Texture> { using ConfigType = LoadCfg::Texture; };
    template<> struct ResourceConfigTraits<Model> { using ConfigType = LoadCfg::Model; };
    template<> struct ResourceConfigTraits<Shader> { using ConfigType = LoadCfg::Shader; };

	class ResourceSystem {
    public:
        template<typename T, typename Config>
        std::shared_ptr<T> load(const std::filesystem::path& path, const Config cfg) {
            return loadImpl<T>(path, cfg);
        }

        template<typename T>
        std::shared_ptr<T> load(const std::filesystem::path& path) {
            using Config = typename ResourceConfigTraits<T>::ConfigType;
            return load<T, Config>(path, Config{});
        }

        // IDE support, please!
        /*std::shared_ptr<Image> load(const std::filesystem::path& path, const LoadCfg::Image cfg = {}) {
            return this->template loadImpl<Image>(path, cfg);
        }

        std::shared_ptr<Texture> load(const std::filesystem::path& path, const LoadCfg::Texture cfg = {}) {
            return this->template loadImpl<Texture>(path, cfg);
        }

        std::shared_ptr<Shader> load(const std::filesystem::path& path, const LoadCfg::Shader cfg = {}) {
            return this->template loadImpl<Shader>(path, cfg);
        }

        std::shared_ptr<Model> load(const std::filesystem::path& path, const LoadCfg::Model cfg = {}) {
            return this->template loadImpl<Model>(path, cfg);
        }*/

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
        template<typename T, typename Config>
        std::shared_ptr<T> loadImpl(const std::filesystem::path& path, const Config& cfg) {
            auto key = makeCacheKey<T>(path);

            if (auto it = m_cache.find(key); it != m_cache.end())
                return std::static_pointer_cast<T>(it->second);

            auto resource = ResourceLoader::load(path, cfg);

            if (!resource) {
                ENGINE_THROW("Failed to load resource");
                return nullptr;
            }

            resource->m_path = path;
            m_cache[key] = resource;
            return resource;
        }

        template<typename T>
        std::string makeCacheKey(const std::filesystem::path& path) {
            return std::string(typeid(T).name()) + "|" + path.string();
        }

        std::unordered_map<std::string, std::shared_ptr<IResource>> m_cache;
    };

    namespace DefaultAssets {
        ENGINE_API std::shared_ptr<Texture> GetDefaultColorTexture();
        ENGINE_API std::shared_ptr<Texture> GetDefaultNormalTexture();
        ENGINE_API std::shared_ptr<Shader> GetUnlitShader();
        ENGINE_API std::shared_ptr<Shader> GetLitShader();
        ENGINE_API std::shared_ptr<Shader> GetTexturedShader();
    }
}