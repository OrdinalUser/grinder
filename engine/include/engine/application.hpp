#pragma once

#include <engine/api.hpp>
#include <engine/window.hpp>
#include <engine/layer.hpp>
#include <engine/vfs.hpp>
#include <engine/resource.hpp>

namespace Engine {
	class Application {
	public:
		ENGINE_API Application(std::unique_ptr<Window> window, std::shared_ptr<VFS> vfs, std::shared_ptr<ResourceSystem> rs);

		ENGINE_API void Run();

		ENGINE_API static Application& Get();

		// The getter our GuiLayer needs
		ENGINE_API Window& GetWindow() { return *m_Window; }

		ENGINE_API void PushLayer(ILayer* layer);

		ENGINE_API void PopLayer(ILayer* layer);

		ENGINE_API std::shared_ptr<VFS> GetVFS();

		ENGINE_API std::shared_ptr<ResourceSystem> GetResourceSystem();

	private:
		std::unique_ptr<Window> m_Window;
		LayerStack m_LayerStack;
		std::shared_ptr<VFS> m_Vfs;
		std::shared_ptr<ResourceSystem> m_Rs;
		bool m_Running = true;
	};
}