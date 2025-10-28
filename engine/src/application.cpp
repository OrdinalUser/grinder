#include <engine/application.hpp>
#include <engine/log.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

static void glfw_resize_callback(GLFWwindow* window, int width, int height) {
	Engine::Application& app = Engine::Application::Get();
}

namespace Engine {
	static Application* g_Application_Instance = nullptr;

	Application::Application(std::unique_ptr<Window> window, std::shared_ptr<VFS> vfs, std::shared_ptr<ResourceSystem> rs) : m_Vfs{ vfs }, m_Rs{ rs } {
		g_Application_Instance = this;
		Log::info("Initializing Grinder Application");
		
		m_Window = std::move(window);
		glClearColor(0.1f, 0.1f, 0.1f, 1);
	}

	void Application::Run() {
		while (m_Running) {
			if (glfwWindowShouldClose(m_Window->GetNativeWindow()))
				m_Running = false;
				
			// Clear screen
			glClear(GL_COLOR_BUFFER_BIT);

			// Update all layers
			for (auto* layer : m_LayerStack)
				layer->OnUpdate(0.016f); // Deltatime placeholder

            for (auto it = m_LayerStack.begin(); it != m_LayerStack.end(); ++it) {
                ILayer* layer = *it;
               layer->OnRender();
            }
			
			m_Window->OnUpdate();
		}
	}

	Application& Application::Get() {
		return *g_Application_Instance;
	}

	void Application::PushLayer(ILayer* layer) {
		m_LayerStack.Push(layer);
	}

	void Application::PopLayer(ILayer* layer) {
		m_LayerStack.Pop(layer);
	}

	std::shared_ptr<VFS> Application::GetVFS() {
		return m_Vfs;
	}

	std::shared_ptr<ResourceSystem> Application::GetResourceSystem() {
		return m_Rs;
	}
}