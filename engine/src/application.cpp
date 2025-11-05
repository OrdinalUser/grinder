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
		Log::trace("Initializing Grinder Application");
	}

	void Application::Run() {
		// Default GL state variables
		glClearColor(0.1f, 0.1f, 0.1f, 1);
		glEnable(GL_DEPTH_TEST);

		using clock = std::chrono::steady_clock;
		auto lastTime = clock::now();
		float accumulator = 0.0f;
		constexpr float fixedDelta = 1.0f / 60.0f; // 60 Hz fixed update
		
		while (m_Running) {
			if (glfwWindowShouldClose(m_Window->GetNativeWindow()))
				m_Running = false;
			
			// Compute time delta
			auto now = clock::now();
			float deltaTime = std::chrono::duration<float>(now - lastTime).count();
			lastTime = now;
			accumulator = std::min(accumulator + deltaTime, fixedDelta * 5.0f); // cap to prevent infinite fixed updates

			// Clear screen
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			PERF_BEGIN("Update_Fixed");
			while (accumulator >= fixedDelta) {
				for (ILayer* layer : m_LayerStack)
					layer->OnUpdateFixed(fixedDelta); // your new scene_update_fixed() hook
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

			PERF_BEGIN("Render");
			for (auto it = m_LayerStack.begin(); it != m_LayerStack.end(); ++it) {
				ILayer* layer = *it;
				layer->OnRender(updatedEntities);
			}
			PERF_END("Render");

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

	LayerStack& Application::GetLayerStack() {
		return m_LayerStack;
	}
}