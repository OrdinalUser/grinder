#include <engine/resource.hpp>
#include <engine/exception.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glad/glad.h>

#include <fstream>

namespace Engine {
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

    Image::~Image() {
        if (data) stbi_image_free(data);
    }

    template<>
    ENGINE_API std::shared_ptr<Texture> ResourceLoader<Texture>::load(const std::filesystem::path& path) {
        auto tex = std::make_shared<Texture>();
        
        // Load image data
        int channels;
        unsigned char* data = stbi_load(
            path.string().c_str(), 
            &tex->width, &tex->height, &channels, 0
        );
        if (!data) return nullptr;
        
        // Create OpenGL texture
        glGenTextures(1, &tex->id);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(
            GL_TEXTURE_2D, 0, format, tex->width, tex->height, 
            0, format, GL_UNSIGNED_BYTE, data
        );
        glGenerateMipmap(GL_TEXTURE_2D);
        
        stbi_image_free(data);
        
        return tex;
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

    template<>
    ENGINE_API std::shared_ptr<Model> ResourceLoader<Model>::load(const std::filesystem::path& path) {
        // TODO
        return std::make_shared<Model>();
    }


    Shader::~Shader() {
        if (program) glDeleteProgram(program);
    }

    Mesh::~Mesh() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
    }
}