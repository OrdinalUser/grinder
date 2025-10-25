#pragma once

#include <engine/api.hpp>
#include <engine/scene_api.hpp>
#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HMODULE module_t;
#endif

namespace Engine {
	class Scene {
	public:
		ENGINE_API Scene(const std::filesystem::path& module_path, const std::filesystem::path& root);
		ENGINE_API ~Scene();

		ENGINE_API void Init();
		ENGINE_API void Update(float deltaTime);
		ENGINE_API void Render();
		ENGINE_API void Shutdown();
		ENGINE_API void Reload();
	private:
		void LoadModule(const std::filesystem::path& module_path);
		void UnloadModule();

		module_t m_module;
		std::string m_name;
		std::filesystem::path m_path;
		std::filesystem::path m_root;

		bool m_initialized;
		scene_init_f m_init_f;
		scene_update_f m_update_f;
		scene_render_f m_render_f;
		scene_shutdown_f m_shutdown_f;
	};
}