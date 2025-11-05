#pragma once

#include <engine/api.hpp>
#include <engine/window.hpp>
#include <engine/layer.hpp>
#include <engine/vfs.hpp>
#include <engine/resource.hpp>
#include <engine/ecs.hpp>

namespace Engine {
	class Application {
	public:
		ENGINE_API Application(std::shared_ptr<Window> window, std::shared_ptr<VFS> vfs, std::shared_ptr<ResourceSystem> rs, std::shared_ptr<ECS> ecs);

		ENGINE_API void Run();

		ENGINE_API static Application& Get();

		ENGINE_API Window& GetWindow() { return *m_Window; }

		ENGINE_API void PushLayer(ILayer* layer);

		ENGINE_API void PopLayer(ILayer* layer);

		ENGINE_API std::shared_ptr<VFS> GetVFS() const;

		ENGINE_API std::shared_ptr<ResourceSystem> GetResourceSystem() const;

		ENGINE_API std::shared_ptr<ECS> GetECS() const;

		ENGINE_API LayerStack& GetLayerStack();
	private:
		std::shared_ptr<Window> m_Window;
		LayerStack m_LayerStack;
		std::shared_ptr<VFS> m_Vfs;
		std::shared_ptr<ResourceSystem> m_Rs;
		std::shared_ptr<ECS> m_Ecs;
		bool m_Running = true;
	};
}