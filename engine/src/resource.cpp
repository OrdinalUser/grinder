#include <engine/resource.hpp>
#include <engine/exception.hpp>
#include <engine/application.hpp>

// Image handling
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// 3D model handling
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// gl* calls
#include <glad/glad.h>

#include <fstream>
#include <functional>

namespace Engine {
    namespace DefaultAssets {
        ENGINE_API std::shared_ptr<Texture> GetDefaultColorTexture() {
            static std::shared_ptr<Texture> white;
            if (!white) {
                auto vfs = Application::Get().GetVFS();
                auto rs = Application::Get().GetResourceSystem();
                std::shared_ptr<Image> img = rs->load<Image>(vfs->GetEngineResourcePath("assets/textures/white1x1.png"));
                white = std::make_shared<Texture>(*img);
            }
            return white;
        }

        ENGINE_API std::shared_ptr<Texture> GetDefaultNormalTexture() {
            static std::shared_ptr<Texture> flatNormal;
            if (!flatNormal) {
                auto vfs = Application::Get().GetVFS();
                auto rs = Application::Get().GetResourceSystem();
                std::shared_ptr<Image> img = rs->load<Image>(vfs->GetEngineResourcePath("assets/textures/normal1x1.png"));
                flatNormal = std::make_shared<Texture>(*img);
            }
            return flatNormal;
        }
    };

    template<>
    ENGINE_API std::shared_ptr<Image> ResourceLoader<Image>::load(const std::filesystem::path& path) {
        auto img = std::make_shared<Image>();
        
        img->data = stbi_load(
            path.string().c_str(), 
            &img->width, &img->height, &img->channels, 0
        );
        
        if (!img->data) {
            ENGINE_THROW("Failed to load image from " + path.string());
            return nullptr;
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

    template<>
    ENGINE_API std::shared_ptr<Shader> ResourceLoader<Shader>::load(const std::filesystem::path& path) {
        auto shader = std::make_shared<Shader>();
        
        // Load vertex and fragment shaders
        // Convention: path.vert and path.frag
        auto vertPath = path.string() + "_vert.glsl";
        auto fragPath = path.string() + "_frag.glsl";
        
        // Read files
        std::ifstream vShaderFile(vertPath);
        std::ifstream fShaderFile(fragPath);
        
        if (!vShaderFile.is_open() || !fShaderFile.is_open()) {
            ENGINE_THROW("Failed to open shader source codes for " + path.string());
            return nullptr;
        }
        
        std::stringstream vss, fss;
        vss << vShaderFile.rdbuf();
        fss << fShaderFile.rdbuf();
        
        std::string vertCode = vss.str();
        std::string fragCode = fss.str();
        
        // Compile shaders
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        
        const char* vCode = vertCode.c_str();
        const char* fCode = fragCode.c_str();
        
        glShaderSource(vertex, 1, &vCode, nullptr);
        glCompileShader(vertex);
        // TODO: Check compilation errors
        
        glShaderSource(fragment, 1, &fCode, nullptr);
        glCompileShader(fragment);
        // TODO: Check compilation errors
        
        // Link program
        shader->program = glCreateProgram();
        glAttachShader(shader->program, vertex);
        glAttachShader(shader->program, fragment);
        glLinkProgram(shader->program);
        // TODO: Check linking errors
        
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        
        return shader;
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

    template<>
    ENGINE_API std::shared_ptr<Model> ResourceLoader<Model>::load(const std::filesystem::path& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality |
            aiProcess_OptimizeMeshes
            // tried aiProcess_OptimizeGraph but that broke imported hierarchy
        );

        if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
            ENGINE_THROW("Failed to load model: " + std::string(importer.GetErrorString()));
            return nullptr;
        }
        
        std::shared_ptr<Model> model = std::make_shared<Model>();
        
        /// Start loading shit 2

        Application& app = Application::Get();
        Ref<VFS> vfs = app.GetVFS();
        Ref<ResourceSystem> rs = app.GetResourceSystem();

        // Load meshes
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
        model->meshes.shrink_to_fit(); // kinda scared to put this here due to gpu ownership

        // Load materials
        model->materials.reserve(scene->mNumMaterials);
        for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
            aiMaterial* mat = scene->mMaterials[i];
            Material material;

            // Helper: load texture from material (handles embedded + external)
            auto loadTexture = [mat, path, scene](aiTextureType type) -> optional<std::shared_ptr<Texture>> {
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

                        // If compressed (e.g. jpg/png in memory)
                        if (tex->mHeight == 0) {
                            // Decode from compressed memory (stb_image or similar)
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

                        return std::make_shared<Texture>(img);
                    }

                    // External texture path
                    auto texPath = path.parent_path() / str.C_Str();
                    auto img = ResourceLoader<Image>::load(texPath);
                    return std::make_shared<Texture>(*img);
                }

                // No texture found
                return {};
            };

            // Load possible texture maps
            material.diffuse = loadTexture(aiTextureType_DIFFUSE).value_or(DefaultAssets::GetDefaultColorTexture());
            material.specular = loadTexture(aiTextureType_SPECULAR).value_or(DefaultAssets::GetDefaultColorTexture());
            material.normal = loadTexture(aiTextureType_NORMALS).value_or(DefaultAssets::GetDefaultNormalTexture());

            // Fallback to solid colors if textures are missing
            aiColor3D color;
            if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, color))
                material.diffuseColor = { color.r, color.g, color.b };

            if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_SPECULAR, color))
                material.specularColor = { color.r, color.g, color.b };

            float shininess = 32.0f;
            mat->Get(AI_MATKEY_SHININESS, shininess);
            material.shininess = shininess;

            // Default shader
            material.shader = rs->load<Shader>(
                vfs->GetEngineResourcePath("assets/shaders/blinn_phong")
            );

            model->materials.push_back(material);
        }
        // model->materials.shrink_to_fit(); // kinda scared to put this here due to gpu ownership

        // Get hierarchy and construction blueprint
        std::function<int(aiNode*, int)> processNode = [&](aiNode* node, entity_id parentBlueprintIdx) -> int {
            Model::BlueprintNode blueprintNode;
            blueprintNode.name = std::string(node->mName.C_Str());
            blueprintNode.parent = parentBlueprintIdx;
            blueprintNode.transform = ConvertToTransform(node->mTransformation);

            Model::MeshCollection collection;
            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                unsigned int meshIdx = node->mMeshes[i];
                unsigned int matIdx = scene->mMeshes[meshIdx]->mMaterialIndex;
                collection.push_back({ &model->meshes[meshIdx], &model->materials[matIdx] });
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

    u32 Shader::GetUniformLoc(const std::string& name) {
        return glGetUniformLocation(program, name.c_str());
    }

    void Shader::SetUniform(const std::string& name, float v) {
        glUniform1f(GetUniformLoc(name), v);
    }

    void Shader::SetUniform(const std::string& name, vec2& v) {
        glUniform2f(GetUniformLoc(name), v.x, v.y);
    }

    void Shader::SetUniform(const std::string& name, vec3& v) {
        glUniform3f(GetUniformLoc(name), v.x, v.y, v.z);
    }

    void Shader::SetUniform(const std::string& name, vec4& v) {
        glUniform4f(GetUniformLoc(name), v.x, v.y, v.z, v.w);
    }

    void Shader::SetUniform(const std::string& name, mat4& v) {
        glUniformMatrix4fv(GetUniformLoc(name), 1, GL_FALSE, glm::value_ptr(v));
    }

    void Shader::SetUniform(const std::string& name, const Texture& tex, TextureSlot slot) {
        glActiveTexture((u32)slot);               // e.g. GL_TEXTURE0 + slot
        glBindTexture(GL_TEXTURE_2D, tex.id);
        glUniform1i(GetUniformLoc(name), (u32)slot - GL_TEXTURE0);
    }

    void Shader::SetUniform(Material& m) {
        // Push color & shininess
        SetUniform("material.diffuseColor", m.diffuseColor);
        SetUniform("material.specularColor", m.specularColor);
        SetUniform("material.shininess", m.shininess);

        // Push textures (if valid)
        if (m.diffuse->id) SetUniform("material.diffuseMap", *m.diffuse, TextureSlot::DIFFUSE);
        if (m.specular->id) SetUniform("material.specularMap", *m.specular, TextureSlot::SPECULAR);
        if (m.normal->id) SetUniform("material.normalMap", *m.normal, TextureSlot::NORMAL);
    }

}