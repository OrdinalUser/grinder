#pragma once

#include <engine/api.hpp>
#include <filesystem>
#include <unordered_map>

namespace Engine {
    class VFS {
    public:
        // Resolves path according to the current working directory
        ENGINE_API std::filesystem::path GetResourcePath(const std::string& module_name, const std::string& filepath);

        // Resolves path according to the current engine mapping and relative module directory
        ENGINE_API std::filesystem::path GetEngineResourcePath(const std::string& filepath);

        // Add new mapping, relative to VFS.m_root
        ENGINE_API void AddResourcePath(const std::string& module_name, const std::string& filepath);

        // Remove mapping to a module
        ENGINE_API void DeleteResourcePath(const std::string& module_name);

        // Returns a reference to the global singleton class
        ENGINE_API static VFS& get();

        // Resolves the engine's module name
        ENGINE_API static std::string GetCurrentModuleName();

        // Resolves absolute path to VFS relative path
        ENGINE_API std::filesystem::path AbsoluteToRelative(const std::filesystem::path& path) const;
    private:
        std::filesystem::path m_root;
        std::unordered_map<std::string, std::filesystem::path> m_module_root;

        // Do not allow unsafe code
        VFS(const VFS&) = delete;
        VFS& operator=(const VFS&) = delete;
        VFS(VFS&&) = delete;
        VFS& operator=(VFS&&) = delete;

        // Stick to mostly defaults
        ENGINE_API VFS();
        ENGINE_API ~VFS() = default;
    };
}