#pragma once

#include <engine/api.hpp>
#include <filesystem>
#include <unordered_map>

namespace Engine {
    class VFS {
    public:
        // Stick to defaults
        ENGINE_API VFS();
        ENGINE_API ~VFS() = default;

        // Resolves path according to the current working directory and checks for validity
        ENGINE_API std::filesystem::path GetResourcePath(const std::string& module_name, const std::string& filepath);

        // Resolves path according to the current working directory and returns final destination
        ENGINE_API std::filesystem::path Resolve(const std::string& module_name, const std::string& filepath);

        // Resolves path according to the current engine mapping and relative module directory
        ENGINE_API std::filesystem::path GetEngineResourcePath(const std::string& filepath);

        // Add new mapping, relative to VFS.m_root
        ENGINE_API void AddResourcePath(const std::string& module_name, const std::string& filepath);

        // Remove mapping to a module
        ENGINE_API void DeleteResourcePath(const std::string& module_name);

        // Resolves the engine's module name
        ENGINE_API static std::string GetCurrentModuleName();

        // Resolves absolute path to VFS relative path
        ENGINE_API std::filesystem::path AbsoluteToRelative(const std::filesystem::path& path) const;

        ENGINE_API std::unordered_map<std::string, std::filesystem::path>& GetMap();
        
        // Do not allow unsafe code
        VFS(const VFS&) = delete;
        VFS& operator=(const VFS&) = delete;
        VFS(VFS&&) = delete;
        VFS& operator=(VFS&&) = delete;

    private:
        std::filesystem::path m_root;
        std::unordered_map<std::string, std::filesystem::path> m_module_root;
    };
}