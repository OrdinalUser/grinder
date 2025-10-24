#pragma once

#include <engine/api.hpp>
#include <engine/window.hpp>
#include <engine/layer.hpp>

namespace Engine {
	class Application {
	public:
		ENGINE_API Application(std::unique_ptr<Window> window);

		ENGINE_API void Run();

		ENGINE_API static Application& Get();

		// The getter our GuiLayer needs
		ENGINE_API Window& GetWindow() { return *m_Window; }

		ENGINE_API void PushLayer(ILayer* layer);

		ENGINE_API void PopLayer(ILayer* layer);

	private:
		std::unique_ptr<Window> m_Window;
		LayerStack m_LayerStack;
		bool m_Running = true;
	};
}