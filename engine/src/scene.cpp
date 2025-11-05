#include <engine/scene.hpp>
#include <engine/exception.hpp>
#include <engine/vfs.hpp>
#include <engine/application.hpp>

namespace Engine {

	void Scene::LoadModule(const std::filesystem::path& module_path) {
		#ifdef _WIN32
		m_module = LoadLibraryW((LPCWSTR)m_path.c_str());
		if (!m_module) ENGINE_THROW("Failed to load scene " + module_path.string());

		if (!(m_init_f = (scene_init_f)GetProcAddress(m_module, "scene_init"))) ENGINE_THROW("Failed to load init function from " + module_path.string());
		if (!(m_update_f = (scene_update_f)GetProcAddress(m_module, "scene_update"))) ENGINE_THROW("Failed to load update function from " + module_path.string());
		if (!(m_update_fixed_f = (scene_update_fixed_f)GetProcAddress(m_module, "scene_update_fixed"))) ENGINE_THROW("Failed to load update function from " + module_path.string());
		if (!(m_render_f = (scene_render_f)GetProcAddress(m_module, "scene_render"))) ENGINE_THROW("Failed to load render function from " + module_path.string());
		if (!(m_shutdown_f = (scene_shutdown_f)GetProcAddress(m_module, "scene_shutdown"))) ENGINE_THROW("Failed to load shutdown function from " + module_path.string());
		#endif
	}

	void Scene::UnloadModule() {
		#ifdef _WIN32
		FreeLibrary(m_module);
		#endif

		m_module = nullptr;
		m_init_f = nullptr;
		m_update_fixed_f = nullptr;
		m_update_f = nullptr;
		m_render_f = nullptr;
		m_shutdown_f = nullptr;
	}

	Scene::Scene(const std::filesystem::path& module_path, const std::filesystem::path& root):
		m_path{ std::filesystem::absolute(module_path) }, m_root{ std::filesystem::absolute(root) }, m_module{ 0 }, m_init_f{ 0 }, m_update_f{ 0 }, m_render_f{ 0 }, m_shutdown_f{ 0 },
		m_initialized{ 0 }
	{
		// Check scene resources
		auto vfs = Application::Get().GetVFS();
		std::filesystem::path relative_root = vfs->AbsoluteToRelative(root);

		// Load module
		LoadModule(module_path);

		// Module load survived, everything checks out
		m_name = m_path.stem().filename().string();
		vfs->AddResourcePath(m_name, relative_root.string());
	}

	Scene::~Scene() {
		Shutdown();
		auto vfs = Application::Get().GetVFS();
		vfs->DeleteResourcePath(m_name);

		UnloadModule();
	}

	void Scene::Init() {
		if (m_initialized) ENGINE_THROW("Attempt to re-initialize scene " + m_name);
		m_initialized = true;
		scene_data_t data = { 0 };
		std::memcpy(&data.module_name, m_name.c_str(), m_name.size());
		m_init_f(data);
	}

	void Scene::Update(float deltaTime) const {
		m_update_f(deltaTime);
	}

	void Scene::UpdateFixed(float deltaTime) const {
		m_update_fixed_f(deltaTime);
	}

	void Scene::Render() const {
		m_render_f();
	}

	void Scene::Shutdown() const {
		 m_shutdown_f();
	}

	void Scene::Reload() {
		m_initialized = false;
		Shutdown();
		UnloadModule();
		LoadModule(m_path);
		Init();
	}
}