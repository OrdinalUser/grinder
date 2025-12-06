#include <engine/application.hpp>
#include <engine/log.hpp>
#include <engine/perf_profiler.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <chrono>

static void glfw_resize_callback(GLFWwindow* window, int width, int height) {
	Engine::Application& app = Engine::Application::Get();
}

namespace Engine {
	static Application* g_Application_Instance = nullptr;

	Application::Application(std::shared_ptr<Window> window, std::shared_ptr<VFS> vfs, std::shared_ptr<ResourceSystem> rs, std::shared_ptr<ECS> ecs)
		: m_Vfs{ vfs }, m_Rs{ rs }, m_Ecs{ ecs }, m_Window{ window } {
		g_Application_Instance = this;
		m_Renderer = std::make_shared<Renderer>();
		Log::trace("Initializing Grinder Application");
	}

	void Application::Run() {
		// Default GL state variables
		using clock = std::chrono::steady_clock;
		auto lastTime = clock::now();
		float accumulator = 0.0f;
		constexpr float fixedDelta = 1.0f / 50.0f; // 50 Hz fixed update
		
		// Run one tick so transforms are correct after initialization
		vector<entity_id> updatedEntities = m_Ecs->GetSystem<TransformSystem>()->Update(fixedDelta).value_or(std::vector<entity_id>());
		m_Ecs->GetSystem<TransformSystem>()->PostUpdate();

		while (m_Running) {
			PERF_BEGIN("Time_Full");
			if (glfwWindowShouldClose(m_Window->GetNativeWindow()))
				m_Running = false;

			if (m_Window->HasResized()) {
				OnResize(m_Window->GetWidth(), m_Window->GetHeight());
			}
			
			// Compute time delta
			auto now = clock::now();
			float deltaTime = std::chrono::duration<float>(now - lastTime).count();
			lastTime = now;
			accumulator = std::min(accumulator + deltaTime, fixedDelta * 5.0f); // cap to prevent infinite fixed updates while debugging

			// Clear screen
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			PERF_BEGIN("Update_Fixed");
			while (accumulator >= fixedDelta) {
				for (ILayer* layer : m_LayerStack)
					layer->OnUpdateFixed(fixedDelta);
				accumulator -= fixedDelta;
			}
			PERF_END("Update_Fixed");

			PERF_BEGIN("Update");
			for (ILayer* layer : m_LayerStack)
				layer->OnUpdate(deltaTime);
			PERF_END("Update");

			PERF_BEGIN("Simulation");
			vector<entity_id> updatedEntities = m_Ecs->GetSystem<TransformSystem>()->Update(deltaTime).value_or(std::vector<entity_id>());
			m_Ecs->GetSystem<TransformSystem>()->PostUpdate();
			PERF_END("Simulation");

			PERF_BEGIN("Render_Total");
			for (auto it = m_LayerStack.begin(); it != m_LayerStack.end(); ++it) {
				ILayer* layer = *it;
				layer->OnRender(updatedEntities);
			}
			PERF_END("Render_Total");

			m_Window->OnUpdate();
			PERF_END("Time_Full");
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

	LayerStack& Application::GetLayerStack() {
		return m_LayerStack;
	}

	std::shared_ptr<Renderer> Application::GetRenderer() const {
		return m_Renderer;
	}

	ENGINE_API void Application::OnResize(unsigned int width, unsigned int height) {
		m_Renderer->OnResize(width, height);
	}
}