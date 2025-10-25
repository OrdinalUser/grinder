#pragma once
#include <engine/api.hpp>

#include <filesystem>
#include <unordered_map>
#include <tuple>

namespace Engine {
    class IResource {
    public:
        ENGINE_API virtual ~IResource() = default;
        
        ENGINE_API const std::filesystem::path& getPath() const { return m_path; }
        
        std::filesystem::path m_path;
    };

    class Image : public IResource {
    public:
        int width, height, channels;
        unsigned char* data = nullptr;
        
        ENGINE_API ~Image();
    };

    class Texture : public IResource {
    public:
        unsigned int id = 0;
        int width, height;
        
        ENGINE_API ~Texture();
    };

    class Shader : public IResource {
    public:
        unsigned int program = 0;
        
        ENGINE_API ~Shader();
    };

    class Mesh : public IResource {
    public:
        unsigned int vao = 0, vbo = 0, ebo = 0;
        size_t indexCount = 0;
        
        ENGINE_API ~Mesh();
    };

    class Material : public IResource {
        // TODO: Create engine types, and therefore color
    };

    class Model : public IResource {
    public:
        std::vector<std::shared_ptr<Mesh>> meshes;
        std::vector<std::shared_ptr<Material>> materials;
    };

    template<typename T>
    struct ResourceLoader {
        static std::shared_ptr<T> load(const std::filesystem::path& path);
    };

    template struct ENGINE_API ResourceLoader<Image>;
    template struct ENGINE_API ResourceLoader<Texture>;
    template struct ENGINE_API ResourceLoader<Shader>;
    template struct ENGINE_API ResourceLoader<Model>;

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