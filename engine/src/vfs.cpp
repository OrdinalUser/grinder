#include "engine/vfs.hpp"
#include <engine/exception.hpp>

#include <system_error>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Psapi.h>
#endif

namespace Engine {
    std::filesystem::path VFS::GetResourcePath(const std::string& module_name, const std::string& filepath) {
        std::filesystem::path path = Resolve(module_name, filepath);
        if (!std::filesystem::exists(path)) {
            ENGINE_THROW("Filepath mapping for " + module_name + " leads to non-existant file " + filepath);
        }
        return path;
    }

    std::filesystem::path VFS::Resolve(const std::string& module_name, const std::string& filepath) {
        if (m_module_root.find(module_name) == m_module_root.end()) {
            ENGINE_THROW("Filepath mapping for " + module_name + " doesn't exist " + filepath);
        }
        return m_root / m_module_root[module_name] / filepath;
    }

    std::filesystem::path VFS::GetEngineResourcePath(const std::string& filepath) {
       return GetResourcePath(GetCurrentModuleName(), filepath);
    }

    void VFS::AddResourcePath(const std::string& module_name, const std::string& filepath) {
        if (m_module_root.find(module_name) != m_module_root.end()) {
            ENGINE_THROW("Filepath mapping for " + module_name + " already exists at " + filepath);
        }
        std::filesystem::path path = m_root / filepath;
        if (!std::filesystem::exists(path) or not std::filesystem::is_directory(path)) {
            ENGINE_THROW("Filepath mapping for " + module_name + " is not a valid path " + filepath);
        }
        m_module_root[module_name] = filepath;
    }

    void VFS::DeleteResourcePath(const std::string& module_name) {
        if (m_module_root.find(module_name) == m_module_root.end()) {
            ENGINE_THROW("Filepath mapping for " + module_name + " has never been registered");
        }
        m_module_root.erase(module_name);
    }

    VFS::VFS() : m_root{ std::filesystem::current_path() } {}

    std::filesystem::path VFS::AbsoluteToRelative(const std::filesystem::path& absolute_path) const {
        // We use the overload of std::filesystem::relative that takes an error_code.
        // This prevents it from throwing an exception if a relative path can't be made
        // (for example, if 'absolute_path' is on D:\ and 'm_root' is on C:\).
        std::error_code ec;
        std::filesystem::path relative_path = std::filesystem::relative(absolute_path, m_root, ec);

        if (ec) {
            // An error occurred. This is the "disk boundary" case you mentioned.
            // We can log a warning and return an empty path to signal failure.
            // Log::warn("Could not create relative path for '" + absolute_path.string() + "': " + ec.message());
            ENGINE_THROW("Failed to convert path: " + absolute_path.string() + " to relateive VFS path with root: " + m_root.string());
        }

        return relative_path;
    }

    std::unordered_map<std::string, std::filesystem::path>& VFS::GetMap() { return m_module_root; }

#ifdef _WIN32
    std::string VFS::GetCurrentModuleName() {
        HMODULE hModule = NULL;
        if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&VFS::GetCurrentModuleName,
            &hModule))
        {
            ENGINE_THROW("Failed to get handle to current module");
            return "";
        }

        char buffer[MAX_PATH];
        if (GetModuleBaseNameA(GetCurrentProcess(), hModule, buffer, sizeof(buffer)) == 0) {
            ENGINE_THROW("Failed to name of current module");
            return "";
        }

        std::filesystem::path modulePath(buffer);
        return modulePath.stem().string();
    }
#endif
}