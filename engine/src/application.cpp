#include <engine/application.hpp>
#include <engine/log.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <chrono>

static void glfw_resize_callback(GLFWwindow* window, int width, int height) {
	Engine::Application& app = Engine::Application::Get();
}

namespace Engine {
	static Application* g_Application_Instance = nullptr;

	Application::Application(std::shared_ptr<Window> window, std::shared_ptr<VFS> vfs, std::shared_ptr<ResourceSystem> rs, std::shared_ptr<ECS> ecs) : m_Vfs{ vfs }, m_Rs{ rs }, m_Ecs{ ecs } {
		g_Application_Instance = this;
		Log::info("Initializing Grinder Application");
		
		m_Window = std::move(window);
		glClearColor(0.1f, 0.1f, 0.1f, 1);
		glEnable(GL_DEPTH_TEST);
	}

	void Application::Run() {
		constexpr float deltaTime = 0.016f;
		while (m_Running) {
			if (glfwWindowShouldClose(m_Window->GetNativeWindow()))
				m_Running = false;
				
			// Clear screen
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all layers
			auto start = std::chrono::high_resolution_clock::now();
			for (auto* layer : m_LayerStack)
				layer->OnUpdate(deltaTime); // Deltatime placeholder
			auto end = std::chrono::high_resolution_clock::now();
			Log::info("Update: {} ns", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

			// Update simulation
			start = std::chrono::high_resolution_clock::now();
			vector<entity_id> updatedEntities = m_Ecs->GetSystem<TransformSystem>()->Update(deltaTime).value_or(std::vector<entity_id>());
			m_Ecs->GetSystem<TransformSystem>()->PostUpdate();
			end = std::chrono::high_resolution_clock::now();
			Log::info("Simulation: {} ns", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

			start = std::chrono::high_resolution_clock::now();
            for (auto it = m_LayerStack.begin(); it != m_LayerStack.end(); ++it) {
				ILayer* layer = *it;
				layer->OnRender(updatedEntities);
            }
			end = std::chrono::high_resolution_clock::now();
			Log::info("Render: {} ns", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

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

	std::shared_ptr<VFS> Application::GetVFS() const {
		return m_Vfs;
	}

	std::shared_ptr<ResourceSystem> Application::GetResourceSystem() const  {
		return m_Rs;
	}

	std::shared_ptr<ECS> Application::GetECS() const {
		return m_Ecs;
	}
}