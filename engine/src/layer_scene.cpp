#include <engine/application.hpp>
#include <engine/layer.hpp>
#include <engine/scene.hpp>
#include <engine/ecs.hpp>
#include <engine/resource.hpp>
#include <engine/log.hpp>
#include <engine/renderer.hpp>

#include <engine/perf_profiler.hpp>

#include <chrono>

namespace Engine {
	SceneLayer::SceneLayer(Scene* scene) : ILayer{ "Scene" }, m_Scene{ scene } {}

	SceneLayer::~SceneLayer() {}

	void SceneLayer::OnAttach() {
		m_Scene->Init();
	}

	void SceneLayer::OnDetach() {
		// m_Scene->Shutdown(); // let destructor handle this
	}

	void SceneLayer::OnRender(const std::vector<entity_id>& updatedEntities) {
		Application& app = Application::Get();
		std::shared_ptr<ECS> ecs = app.GetECS();
		
		Renderer& renderer = *app.GetRenderer().get();
		PERF_BEGIN("Render_Queue");
		m_Scene->Render();

		// Get our main camera
		Component::Camera* mainCam = nullptr;
		vec3 viewPos{ 0 };
		for (auto [entity, transform, cam] : ecs->View<Component::Transform, Component::Camera>()) {
			if (cam.isMain) {
				mainCam = &cam;
				viewPos = transform.position;
				renderer.SetCamera(&transform, &cam);
				break;
			}
		}
		if (!mainCam) return; // No camera, no rendering :3

		// Get our lights
		for (auto [entity, transform, light] : ecs->View<Component::Transform, Component::Light>()) {
			renderer.QueueLight(&transform, &light);
		}

		// vec3 lightPos = viewPos; // shines from camera
		// vec3 lightColor = vec3(1, 1, 1);
		// mat4 projView = mainCam->projectionMatrix * mainCam->viewMatrix;

		// Collect our drawables
		for (auto [entity, transform, drawable] : ecs->View<Component::Transform, Component::Drawable3D>()) {
			renderer.QueueDrawable3D(&transform, &drawable);
			//const Model::MeshCollection& collection = drawable.GetCollection();
			//for (const auto [mesh, material] : collection) {
			//	renderer.Queue(&transform, mesh, material);
			//	//Engine::Shader& shader = *material->shader;
			//	//shader.Enable();
			//	//shader.SetUniform(*material);

			//	//shader.SetUniform("uViewPos", viewPos);
			//	//shader.SetUniform("uLightPos", lightPos);
			//	//shader.SetUniform("uLightColor", lightColor);

			//	//shader.SetUniform("uModel", transform.modelMatrix);
			//	//shader.SetUniform("uProjView", projView);
			//	//// shader.SetUniform("uView", mainCam->viewMatrix);
			//	//// shader.SetUniform("uProj", mainCam->projectionMatrix);

			//	//mesh->Draw();
			//}
		}

		PERF_END("Render_Queue");

		PERF_BEGIN("Render_Draw");
		renderer.Draw();
		renderer.Clear();
		PERF_END("Render_Draw");
		// auto vendor = glGetString(GL_VENDOR);
		// Log::info("ECS_iteration: {} ns", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
	}

	void SceneLayer::OnUpdate(float deltaTime) {
		m_Scene->Update(deltaTime);
	}

	void SceneLayer::OnUpdateFixed(float deltaTime) {
		m_Scene->UpdateFixed(deltaTime);
	}

	void SceneLayer::OnReload() {
		m_Scene->Reload();
	}

}