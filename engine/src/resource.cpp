#include <engine/resource.hpp>
#include <engine/exception.hpp>
#include <engine/application.hpp>

// Image handling
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_resize2.h>

// 3D model handling
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// gl* calls
#include <glad/glad.h>

#include <fstream>
#include <functional>

// Helper to compile a single shader stage
static unsigned int compileShader(GLenum type, const std::string& source, const std::string& name) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Check compilation status
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        glDeleteShader(shader);

        const char* stageType = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        ENGINE_THROW(std::string("Shader compilation failed (") + stageType + " - " + name + "): " + infoLog);
    }

    return shader;
}

// Helper to read file contents
static std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ENGINE_THROW("Failed to open file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper to resize image data
static unsigned char* resizeImage(unsigned char* original, int orig_w, int orig_h, int channels,
    int new_w, int new_h) {
    unsigned char* resized = (unsigned char*)malloc((size_t)new_w * new_h * channels);
    if (!resized) return nullptr;

    // v2: cast channels to stbir_pixel_layout and call the linear helper
    if (!stbir_resize_uint8_linear(
        original, orig_w, orig_h, 0,
        resized, new_w, new_h, 0,
        (stbir_pixel_layout)channels)) {
        free(resized);
        return nullptr;
    }

    return resized;
}

// Calculate dimensions maintaining aspect ratio
static std::pair<int, int> calculateResizeDimensions(int orig_w, int orig_h,
    int target_w, int target_h,
    bool maintain_aspect) {
    if (target_w == 0 && target_h == 0) {
        return { orig_w, orig_h };  // No resize
    }

    if (!maintain_aspect) {
        return { target_w > 0 ? target_w : orig_w,
                target_h > 0 ? target_h : orig_h };
    }

    // Maintain aspect ratio
    if (target_w == 0) {
        float ratio = (float)target_h / orig_h;
        return { (int)(orig_w * ratio), target_h };
    }
    if (target_h == 0) {
        float ratio = (float)target_w / orig_w;
        return { target_w, (int)(orig_h * ratio) };
    }

    // Both specified - fit within bounds
    float ratio = std::min((float)target_w / orig_w, (float)target_h / orig_h);
    return { (int)(orig_w * ratio), (int)(orig_h * ratio) };
}

GLenum toGLFilter(Engine::LoadCfg::TextureFilter filter) {
    switch (filter) {
    case Engine::LoadCfg::TextureFilter::Nearest: return GL_NEAREST;
    case Engine::LoadCfg::TextureFilter::Linear: return GL_LINEAR;
    case Engine::LoadCfg::TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
    case Engine::LoadCfg::TextureFilter::LinearMipmapNearest: return GL_LINEAR_MIPMAP_NEAREST;
    case Engine::LoadCfg::TextureFilter::NearestMipmapLinear: return GL_NEAREST_MIPMAP_LINEAR;
    case Engine::LoadCfg::TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
    default: return GL_LINEAR;
    }
}

GLenum toGLWrap(Engine::LoadCfg::TextureWrap wrap) {
    switch (wrap) {
    case Engine::LoadCfg::TextureWrap::Repeat: return GL_REPEAT;
    case Engine::LoadCfg::TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
    case Engine::LoadCfg::TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
    case Engine::LoadCfg::TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
    default: return GL_REPEAT;
    }
}

GLenum getGLFormat(int channels) {
    switch (channels) {
    case 1: return GL_RED;
    case 2: return GL_RG;
    case 3: return GL_RGB;
    case 4: return GL_RGBA;
    default: return GL_RGB;
    }
}

namespace Engine {
    namespace DefaultAssets {
        std::shared_ptr<Texture> GetDefaultColorTexture() {
            return Application::Get().GetResourceSystem()->load<Texture>(Application::Get().GetVFS()->GetEngineResourcePath("assets/textures/white1x1.png"));
        }

        std::shared_ptr<Texture> GetDefaultNormalTexture() {
            return Application::Get().GetResourceSystem()->load<Texture>(Application::Get().GetVFS()->GetEngineResourcePath("assets/textures/normal1x1.png"));
        }

        std::shared_ptr<Shader> GetUnlitShader() {
            return Application::Get().GetResourceSystem()->load<Shader>(
                Application::Get().GetVFS()->GetEngineResourcePath("assets/shaders/unlit")
            );
        }

        std::shared_ptr<Shader> GetLitShader() {
            return Application::Get().GetResourceSystem()->load<Shader>(
                Application::Get().GetVFS()->GetEngineResourcePath("assets/shaders/lit")
            );
        }

        std::shared_ptr<Shader> GetTexturedShader() {
            return Application::Get().GetResourceSystem()->load<Shader>(
                Application::Get().GetVFS()->GetEngineResourcePath("assets/shaders/textured")
            );
        }
    };

    std::shared_ptr<Image> ResourceLoader::load(const std::filesystem::path& path, const LoadCfg::Image& cfg) {
        auto img = std::make_shared<Image>();

        // Set flip flag before loading
        stbi_set_flip_vertically_on_load(cfg.flip_vertically);

        // Load image with desired format
        int desired_channels = static_cast<int>(cfg.format);
        int original_channels;

        img->data = stbi_load(
            path.string().c_str(),
            &img->width, &img->height, &original_channels,
            desired_channels  // 0 = auto, 1-4 = force specific format
        );

        if (!img->data) {
            ENGINE_THROW("Failed to load image from " + path.string() + ": " + std::string(stbi_failure_reason()));
        }

        // Set actual channel count
        img->channels = (desired_channels == 0) ? original_channels : desired_channels;

        // Handle resizing if requested
        if (cfg.width > 0 || cfg.height > 0) {
            auto [new_w, new_h] = calculateResizeDimensions(
                img->width, img->height,
                cfg.width, cfg.height,
                cfg.maintain_aspect
            );

            if (new_w != img->width || new_h != img->height) {
                unsigned char* resized = resizeImage(img->data, img->width, img->height,
                    img->channels, new_w, new_h);
                if (!resized) {
                    stbi_image_free(img->data);
                    ENGINE_THROW("Failed to resize image from " + path.string());
                }

                stbi_image_free(img->data);
                img->data = resized;
                img->width = new_w;
                img->height = new_h;
            }
        }

        return img;
    }

    Texture::Texture(Texture&& other) noexcept {
        id = other.id;
        width = other.width;
        height = other.height;
        other.id = 0;
    }

    Texture& Texture::operator=(Texture&& other) noexcept {
        if (this != &other) {
            glDeleteTextures(1, &id);
            id = other.id;
            width = other.width;
            height = other.height;
            other.id = 0;
        }
        return *this;
    }

    Image::~Image() {
        if (data) stbi_image_free(data);
    }

    Texture::Texture(const Image& img) {
        width = img.width;
        height = img.height;
        m_path = img.m_path;

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        GLenum format = (img.channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, img.width, img.height, 0, format, GL_UNSIGNED_BYTE, img.data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Texture::~Texture() {
        if (id) glDeleteTextures(1, &id);
    }

    const Model::MeshCollection& Component::Drawable3D::GetCollection() const {
        return model->collections[collectionIndex];
    }

    std::shared_ptr<Shader> ResourceLoader::load(const std::filesystem::path& path, const LoadCfg::Shader& cfg) {
        auto shader = std::make_shared<Shader>();

        // Build shader file paths
        auto vertPath = path.string() + "_vert.glsl";
        auto fragPath = path.string() + "_frag.glsl";

        // Read shader source files
        std::string vertCode = readFile(vertPath);
        std::string fragCode = readFile(fragPath);

        // Compile shader stages
        unsigned int vertShader = compileShader(GL_VERTEX_SHADER, vertCode, path.filename().string());
        unsigned int fragShader = 0;

        try {
            fragShader = compileShader(GL_FRAGMENT_SHADER, fragCode, path.filename().string());
        }
        catch (...) {
            glDeleteShader(vertShader);
            throw;
        }

        // Link shader program
        shader->program = glCreateProgram();
        glAttachShader(shader->program, vertShader);
        glAttachShader(shader->program, fragShader);
        glLinkProgram(shader->program);

        // Check linking status
        int success;
        glGetProgramiv(shader->program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shader->program, 512, nullptr, infoLog);

            glDeleteShader(vertShader);
            glDeleteShader(fragShader);
            glDeleteProgram(shader->program);

            ENGINE_THROW("Shader linking failed (" + path.string() + "): " + infoLog);
        }

        // Cleanup shader objects (no longer needed after linking)
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        return shader;
    }

    std::shared_ptr<Texture> ResourceLoader::load(const std::filesystem::path& path, const LoadCfg::Texture& cfg) {
        // First, load the image using Image loader with inherited config
        LoadCfg::Image img_cfg;
        img_cfg.format = cfg.format;
        img_cfg.flip_vertically = cfg.flip_vertically;
        img_cfg.width = cfg.width;
        img_cfg.height = cfg.height;
        img_cfg.maintain_aspect = cfg.maintain_aspect;

        auto image = ResourceLoader::load(path, img_cfg);
        if (!image || !image->data) {
            ENGINE_THROW("Failed to load image for texture: " + path.string());
        }

        // Create OpenGL texture
        auto tex = std::make_shared<Texture>();
        tex->width = image->width;
        tex->height = image->height;

        glGenTextures(1, &tex->id);
        glBindTexture(GL_TEXTURE_2D, tex->id);

        // Upload texture data
        GLenum format = getGLFormat(image->channels);
        glTexImage2D(
            GL_TEXTURE_2D, 0, format,
            tex->width, tex->height, 0,
            format, GL_UNSIGNED_BYTE, image->data
        );

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, toGLFilter(cfg.min_filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, toGLFilter(cfg.mag_filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, toGLWrap(cfg.wrap_s));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, toGLWrap(cfg.wrap_t));

        // Generate mipmaps if requested
        if (cfg.generate_mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        return tex;
    }

    static Component::Transform ConvertToTransform(const aiMatrix4x4& m)
    {
        Component::Transform t;

        // Assimp stores in row-major; GLM expects column-major.
        aiVector3D scaling, position;
        aiQuaternion rotation;
        m.Decompose(scaling, rotation, position);

        t.position = { position.x, position.y, position.z };
        t.rotation = glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);
        t.scale = { scaling.x, scaling.y, scaling.z };

        // Compose model matrix (GLM column-major)
        t.modelMatrix = glm::translate(glm::mat4(1.0f), t.position)
            * glm::mat4_cast(t.rotation)
            * glm::scale(glm::mat4(1.0f), t.scale);

        return t;
    }

    std::shared_ptr<Model> ResourceLoader::load(const std::filesystem::path& path, const LoadCfg::Model& cfg) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality |
            aiProcess_OptimizeMeshes |
            (cfg.flip_uvs ? aiProcess_FlipUVs : 0) |
            (cfg.static_mesh ? aiProcess_OptimizeGraph : 0)
        );

        if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
            ENGINE_THROW("Failed to load model: " + std::string(importer.GetErrorString()));
            return nullptr;
        }

        std::shared_ptr<Model> model = std::make_shared<Model>();

        Application& app = Application::Get();
        Ref<VFS> vfs = app.GetVFS();
        Ref<ResourceSystem> rs = app.GetResourceSystem();

        // ========== FIRST PASS: Find which materials are actually used ==========
        std::unordered_set<unsigned int> usedMaterialIndices;

        std::function<void(aiNode*)> scanNode = [&](aiNode* node) {
            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                unsigned int meshIdx = node->mMeshes[i];
                unsigned int matIdx = scene->mMeshes[meshIdx]->mMaterialIndex;
                usedMaterialIndices.insert(matIdx);
            }
            for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                scanNode(node->mChildren[i]);
            }
            };

        scanNode(scene->mRootNode);

        // ========== Calculate bounds across all meshes ==========
        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);

        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            aiMesh* m = scene->mMeshes[i];
            for (unsigned int v = 0; v < m->mNumVertices; ++v) {
                glm::vec3 pos = { m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z };
                minBounds = glm::min(minBounds, pos);
                maxBounds = glm::max(maxBounds, pos);
            }
        }
        model->bounds.min = minBounds;
        model->bounds.max = maxBounds;

        // ========== Load all meshes (we need all since nodes reference them) ==========
        model->meshes.reserve(scene->mNumMeshes);
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            aiMesh* m = scene->mMeshes[i];
            std::vector<Vertex> vertices;
            std::vector<u32> indices;

            vertices.reserve(m->mNumVertices);
            for (unsigned int v = 0; v < m->mNumVertices; ++v) {
                Vertex vert{};
                vert.position = { m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z };
                vert.normal = { m->mNormals[v].x, m->mNormals[v].y, m->mNormals[v].z };
                vert.uv = m->mTextureCoords[0]
                    ? glm::vec2(m->mTextureCoords[0][v].x, m->mTextureCoords[0][v].y)
                    : glm::vec2(0.0f);
                vert.tangent = m->mTangents
                    ? glm::vec3(m->mTangents[v].x, m->mTangents[v].y, m->mTangents[v].z)
                    : glm::vec3(0.0f);
                vertices.push_back(vert);
            }

            for (unsigned int f = 0; f < m->mNumFaces; ++f)
                for (unsigned int j = 0; j < m->mFaces[f].mNumIndices; ++j)
                    indices.push_back(m->mFaces[f].mIndices[j]);

            vertices.shrink_to_fit();
            indices.shrink_to_fit();
            model->meshes.emplace_back(vertices, indices);
        }

        // ========== SECOND PASS: Load only used materials ==========
        // Create mapping: old material index -> new material index
        std::unordered_map<unsigned int, unsigned int> materialIndexRemap;

        // Helper: load texture from material (handles embedded + external)
        auto loadTexture = [path, scene, rs](aiMaterial* mat, aiTextureType type, unsigned int matIndex) -> optional<std::shared_ptr<Texture>> {
            if (mat->GetTextureCount(type) > 0) {
                aiString str;
                mat->GetTexture(type, 0, &str);

                // Embedded texture (*0, *1, etc.)
                if (str.C_Str()[0] == '*') {
                    int texIndex = std::atoi(str.C_Str() + 1);
                    aiTexture* tex = scene->mTextures[texIndex];
                    if (!tex) return {};

                    Image img{};
                    img.width = tex->mWidth;
                    img.height = tex->mHeight;
                    img.channels = 4;
                    img.m_path = path;

                    // If compressed (e.g. jpg/png in memory)
                    if (tex->mHeight == 0) {
                        auto bytes = reinterpret_cast<unsigned char*>(tex->pcData);
                        int w, h, c;
                        unsigned char* data = stbi_load_from_memory(bytes, tex->mWidth, &w, &h, &c, 4);
                        img.width = w;
                        img.height = h;
                        img.channels = 4;
                        img.data = data;
                    }
                    else {
                        // Raw uncompressed BGRA8888
                        img.data = reinterpret_cast<unsigned char*>(tex->pcData);
                    }

                    std::shared_ptr<Texture> texture = std::make_shared<Texture>(img);
                    rs->cache<Texture>(path.string() + ":tex:" + std::to_string(matIndex) + ":" + std::to_string(type), texture);
                    return texture;
                }

                // External texture path
                auto texPath = path.parent_path() / str.C_Str();
                Ref<Image> img = ResourceLoader::load(texPath, LoadCfg::Image());
                return std::make_shared<Texture>(*img);
            }

            return {};
        };

        model->materials.reserve(usedMaterialIndices.size());

        for (unsigned int oldIdx : usedMaterialIndices) {
            aiMaterial* mat = scene->mMaterials[oldIdx];
            Material material;

            // Load textures
            auto diffuseTex = loadTexture(mat, aiTextureType_DIFFUSE, oldIdx);
            auto specularTex = loadTexture(mat, aiTextureType_SPECULAR, oldIdx);
            auto normalTex = loadTexture(mat, aiTextureType_NORMALS, oldIdx);

            // Set textures or defaults
            material.diffuse = diffuseTex.value_or(DefaultAssets::GetDefaultColorTexture());
            material.specular = specularTex.value_or(DefaultAssets::GetDefaultColorTexture());
            material.normal = normalTex.value_or(DefaultAssets::GetDefaultNormalTexture());

            // Load material colors
            aiColor3D color;
            if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, color))
                material.diffuseColor = { color.r, color.g, color.b };

            if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_SPECULAR, color))
                material.specularColor = { color.r, color.g, color.b };

            constexpr float SHININESS_DEFAULT = 32.0f;
            float shininess = SHININESS_DEFAULT;
            if (AI_SUCCESS != mat->Get(AI_MATKEY_SHININESS, shininess) || shininess <= 0.0f)
                shininess = SHININESS_DEFAULT;
            material.shininess = shininess;

            // ========== Determine transparency ==========
            material.isTransparent = false;

            // Check opacity from material
            float opacity = 1.0f;
            if (AI_SUCCESS == mat->Get(AI_MATKEY_OPACITY, opacity)) {
                if (opacity < 1.0f) {
                    material.isTransparent = true;
                }
            }

            // Check if diffuse texture has alpha channel
            if (diffuseTex.has_value() && !material.isTransparent) {
                // We can't easily check texture format after upload, but we can check the original image
                // For now, we'll trust the opacity value or add a check during texture loading
                // TODO: Could store channel count in Texture struct if needed
            }

            // ========== Classify material type (highest wins) ==========
            bool hasAnyTexture = diffuseTex.has_value() || specularTex.has_value() || normalTex.has_value();

            if (hasAnyTexture) {
                material.renderType = Material::RenderType::TEXTURED;
                material.shader = DefaultAssets::GetTexturedShader();
            }
            else {
                material.renderType = Material::RenderType::LIT;
                material.shader = DefaultAssets::GetLitShader();
            }

            // Store the new index for this material
            unsigned int newIdx = static_cast<unsigned int>(model->materials.size());
            materialIndexRemap[oldIdx] = newIdx;

            // Cache the material for debugging
            material.m_path = path;
            rs->cache<Material>(path.string() + ":mat:" + std::to_string(oldIdx),
                std::make_shared<Material>(material));

            model->materials.push_back(std::move(material));
        }

        // ========== Build hierarchy and collections with remapped indices ==========
        std::function<int(aiNode*, int)> processNode = [&](aiNode* node, entity_id parentBlueprintIdx) -> int {
            Model::BlueprintNode blueprintNode;
            blueprintNode.name = std::string(node->mName.C_Str());
            blueprintNode.parent = parentBlueprintIdx;
            blueprintNode.transform = ConvertToTransform(node->mTransformation);

            Model::MeshCollection collection;
            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                unsigned int meshIdx = node->mMeshes[i];
                unsigned int oldMatIdx = scene->mMeshes[meshIdx]->mMaterialIndex;

                // Remap to new material index
                unsigned int newMatIdx = materialIndexRemap[oldMatIdx];

                collection.push_back({
                    &model->meshes[meshIdx],
                    &model->materials[newMatIdx]
                });
            }

            blueprintNode.collectionIndex = model->collections.size();
            model->collections.push_back(std::move(collection));

            int thisIdx = (int)model->blueprint.size();
            model->blueprint.push_back(blueprintNode);

            for (unsigned int i = 0; i < node->mNumChildren; ++i)
                processNode(node->mChildren[i], thisIdx);

            return thisIdx;
        };

        processNode(scene->mRootNode, null);

        return model;
    }

    //std::shared_ptr<Model> ResourceLoader::load(const std::filesystem::path& path, const LoadCfg::Model& cfg) {
    //    Assimp::Importer importer;
    //    const aiScene* scene = importer.ReadFile(
    //        path.string(),
    //        aiProcess_Triangulate |
    //        aiProcess_GenNormals |
    //        aiProcess_CalcTangentSpace |
    //        aiProcess_JoinIdenticalVertices |
    //        aiProcess_ImproveCacheLocality |
    //        aiProcess_OptimizeMeshes |
    //        (cfg.flip_uvs ? aiProcess_FlipUVs : 0) |
    //        (cfg.static_mesh ? aiProcess_OptimizeGraph : 0)
    //    );

    //    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
    //        ENGINE_THROW("Failed to load model: " + std::string(importer.GetErrorString()));
    //        return nullptr;
    //    }

    //    std::shared_ptr<Model> model = std::make_shared<Model>();

    //    /// Start loading shit 2

    //    Application& app = Application::Get();
    //    Ref<VFS> vfs = app.GetVFS();
    //    Ref<ResourceSystem> rs = app.GetResourceSystem();

    //    // Calculate bounds across all meshes
    //    glm::vec3 minBounds(FLT_MAX);
    //    glm::vec3 maxBounds(-FLT_MAX);
    //    float normalizeScale = 1.0f;
    //    glm::vec3 normalizeCenter(0.0f);

    //    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
    //        aiMesh* m = scene->mMeshes[i];
    //        for (unsigned int v = 0; v < m->mNumVertices; ++v) {
    //            glm::vec3 pos = { m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z };
    //            minBounds = glm::min(minBounds, pos);
    //            maxBounds = glm::max(maxBounds, pos);
    //        }
    //    }
    //    model->bounds.min = minBounds;
    //    model->bounds.max = maxBounds;

    //    // Load meshes
    //    model->meshes.reserve(scene->mNumMeshes);
    //    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
    //        aiMesh* m = scene->mMeshes[i];
    //        std::vector<Vertex> vertices;
    //        std::vector<u32> indices;

    //        vertices.reserve(m->mNumVertices);
    //        for (unsigned int v = 0; v < m->mNumVertices; ++v) {
    //            Vertex vert{};
    //            vert.position = { m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z };
    //            vert.normal = { m->mNormals[v].x, m->mNormals[v].y, m->mNormals[v].z };
    //            vert.uv = m->mTextureCoords[0]
    //                ? glm::vec2(m->mTextureCoords[0][v].x, m->mTextureCoords[0][v].y)
    //                : glm::vec2(0.0f);
    //            vert.tangent = m->mTangents
    //                ? glm::vec3(m->mTangents[v].x, m->mTangents[v].y, m->mTangents[v].z)
    //                : glm::vec3(0.0f);
    //            vertices.push_back(vert);
    //        }

    //        for (unsigned int f = 0; f < m->mNumFaces; ++f)
    //            for (unsigned int j = 0; j < m->mFaces[f].mNumIndices; ++j)
    //                indices.push_back(m->mFaces[f].mIndices[j]);

    //        vertices.shrink_to_fit();
    //        indices.shrink_to_fit();
    //        model->meshes.emplace_back(vertices, indices);
    //    }
    //    // model->meshes.shrink_to_fit(); // kinda scared to put this here due to gpu ownership

    //    // Load materials
    //    model->materials.reserve(scene->mNumMaterials);
    //    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
    //        aiMaterial* mat = scene->mMaterials[i];
    //        Material material;

    //        // Helper: load texture from material (handles embedded + external)
    //        auto loadTexture = [mat, path, scene, rs, i](aiTextureType type) -> optional<std::shared_ptr<Texture>> {
    //            if (mat->GetTextureCount(type) > 0) {
    //                aiString str;
    //                mat->GetTexture(type, 0, &str);

    //                // Embedded texture (*0, *1, etc.)
    //                if (str.C_Str()[0] == '*') {
    //                    int texIndex = std::atoi(str.C_Str() + 1);
    //                    aiTexture* tex = scene->mTextures[texIndex];
    //                    if (!tex) return {};

    //                    Image img{};
    //                    img.width = tex->mWidth;
    //                    img.height = tex->mHeight;
    //                    img.channels = 4;
    //                    img.m_path = path;

    //                    // If compressed (e.g. jpg/png in memory)
    //                    if (tex->mHeight == 0) {
    //                        // Decode from compressed memory (stb_image or similar)
    //                        auto bytes = reinterpret_cast<unsigned char*>(tex->pcData);
    //                        int w, h, c;
    //                        unsigned char* data = stbi_load_from_memory(bytes, tex->mWidth, &w, &h, &c, 4);
    //                        img.width = w;
    //                        img.height = h;
    //                        img.channels = 4;
    //                        img.data = data;
    //                    }
    //                    else {
    //                        // Raw uncompressed BGRA8888
    //                        img.data = reinterpret_cast<unsigned char*>(tex->pcData);
    //                    }

    //                    // Also add to the resource system
    //                    std::shared_ptr<Texture> texture = std::make_shared<Texture>(img);
    //                    rs->cache<Texture>(img.m_path.string() + ":" + std::to_string(i), texture);
    //                    return texture;
    //                }

    //                // External texture path
    //                auto texPath = path.parent_path() / str.C_Str();
    //                Ref<Image> img = ResourceLoader::load(texPath, LoadCfg::Image());
    //                return std::make_shared<Texture>(*img);
    //            }

    //            // No texture found
    //            return {};
    //            };

    //        // Load possible texture maps
    //        material.diffuse = loadTexture(aiTextureType_DIFFUSE).value_or(DefaultAssets::GetDefaultColorTexture());
    //        material.specular = loadTexture(aiTextureType_SPECULAR).value_or(DefaultAssets::GetDefaultColorTexture());
    //        material.normal = loadTexture(aiTextureType_NORMALS).value_or(DefaultAssets::GetDefaultNormalTexture());

    //        // Fallback to solid colors if textures are missing
    //        aiColor3D color;
    //        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, color))
    //            material.diffuseColor = { color.r, color.g, color.b };

    //        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_SPECULAR, color))
    //            material.specularColor = { color.r, color.g, color.b };

    //        constexpr float SHININESS_DEFAULT = 32.0f;
    //        float shininess = SHININESS_DEFAULT;
    //        if (AI_SUCCESS != mat->Get(AI_MATKEY_SHININESS, shininess) || shininess <= 0.0f)
    //            shininess = SHININESS_DEFAULT;
    //        material.shininess = shininess;

    //        // Default shader
    //        material.shader = rs->load<Shader>(vfs->GetEngineResourcePath("assets/shaders/blinn_phong"));

    //        model->materials.push_back(material);
    //    }
    //    // model->materials.shrink_to_fit(); // kinda scared to put this here due to gpu ownership

    //    // Get hierarchy and construction blueprint
    //    std::function<int(aiNode*, int)> processNode = [&](aiNode* node, entity_id parentBlueprintIdx) -> int {
    //        Model::BlueprintNode blueprintNode;
    //        blueprintNode.name = std::string(node->mName.C_Str());
    //        blueprintNode.parent = parentBlueprintIdx;
    //        blueprintNode.transform = ConvertToTransform(node->mTransformation);

    //        Model::MeshCollection collection;
    //        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
    //            unsigned int meshIdx = node->mMeshes[i];
    //            unsigned int matIdx = scene->mMeshes[meshIdx]->mMaterialIndex;
    //            collection.push_back({ &model->meshes[meshIdx], &model->materials[matIdx] });
    //        }

    //        blueprintNode.collectionIndex = model->collections.size();
    //        model->collections.push_back(std::move(collection));

    //        int thisIdx = (int)model->blueprint.size();
    //        model->blueprint.push_back(blueprintNode);

    //        for (unsigned int i = 0; i < node->mNumChildren; ++i)
    //            processNode(node->mChildren[i], thisIdx);

    //        return thisIdx;
    //        };

    //    processNode(scene->mRootNode, null);

    //    return model;
    //}

    void Vertex::SetupVAO() {
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
        glEnableVertexAttribArray(3);
    }

    void Mesh::Bind() const {
        glBindVertexArray(vao);
    }

    void Mesh::Draw() const {
        Bind();
        glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
    }

    Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<u32>& indices) : indicesCount{ static_cast<u32>(indices.size()) } {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        // Vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        // Index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);

        Vertex::SetupVAO();

        glBindVertexArray(0);
    }

    Mesh::~Mesh() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
    }

    void Shader::Enable() const {
        if (!program) ENGINE_THROW("Attempting to use uninitialized shader program");
        glUseProgram(program);
    }

    Shader::~Shader() {
        if (program) glDeleteProgram(program);
    }

    u32 Shader::GetUniformLoc(const std::string& name) const {
        return glGetUniformLocation(program, name.c_str());
    }

    void Shader::SetUniform(const std::string& name, const float v) const {
        glUniform1f(GetUniformLoc(name), v);
    }

    void Shader::SetUniform(const std::string& name, const vec2& v) const {
        glUniform2f(GetUniformLoc(name), v.x, v.y);
    }

    void Shader::SetUniform(const std::string& name, const vec3& v) const {
        glUniform3f(GetUniformLoc(name), v.x, v.y, v.z);
    }

    void Shader::SetUniform(const std::string& name, const vec4& v) const {
        glUniform4f(GetUniformLoc(name), v.x, v.y, v.z, v.w);
    }

    void Shader::SetUniform(const std::string& name, const mat4& v) const {
        glUniformMatrix4fv(GetUniformLoc(name), 1, GL_FALSE, glm::value_ptr(v));
    }

    void Shader::SetUniform(const std::string& name, const Texture& tex, const TextureSlot slot) const {
        GLenum texSlot = GL_TEXTURE0 + static_cast<GLenum>(slot);
        glActiveTexture(texSlot);
        glBindTexture(GL_TEXTURE_2D, tex.id);
        glUniform1i(GetUniformLoc(name), static_cast<GLint>(slot));
    }

    void Shader::SetUniform(const Material& m) const {
        // Push color & shininess
        SetUniform("uMaterial.diffuseColor", m.diffuseColor);
        SetUniform("uMaterial.specularColor", m.specularColor);
        SetUniform("uMaterial.shininess", m.shininess);

        // Push textures (if valid)
        if (m.renderType == Material::RenderType::TEXTURED) {
            if (m.diffuse->id) SetUniform("uMaterial.diffuseMap", *m.diffuse, TextureSlot::DIFFUSE);
            if (m.specular->id) SetUniform("uMaterial.specularMap", *m.specular, TextureSlot::SPECULAR);
            if (m.normal->id) SetUniform("uMaterial.normalMap", *m.normal, TextureSlot::NORMAL);
        }
    }

}