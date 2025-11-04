#include <engine/application.hpp>
#include <engine/layer.hpp>
#include <engine/scene.hpp>
#include <engine/ecs.hpp>
#include <engine/resource.hpp>
#include <engine/log.hpp>

#include <chrono>

namespace Engine {
	SceneLayer::SceneLayer(Scene* scene) : ILayer{ "Scene" }, m_Scene{ scene } {}

	SceneLayer::~SceneLayer() {}

	void SceneLayer::OnAttach() {
		m_Scene->Init();
	}

	void SceneLayer::OnDetach() {
		m_Scene->Shutdown();
	}

	void SceneLayer::OnRender(const std::vector<entity_id>& updatedEntities) {
		Application& app = Application::Get();
		std::shared_ptr<ECS> ecs = app.GetECS();
		
		m_Scene->Render();

		// Get our main camera
		Component::Camera* mainCam = nullptr;
		vec3 viewPos{ 0 };
		for (auto [entity, transform, cam] : ecs->View<Component::Transform, Component::Camera>()) {
			if (cam.isMain) {
				mainCam = &cam;
				viewPos = transform.position;
				break;
			}
		}
		if (!mainCam) return; // No camera, no rendering :3

		auto start = std::chrono::high_resolution_clock::now();

		vec3 lightDir = -viewPos; // shines from camera
		vec3 lightColor = vec3(1, 1, 1);

		for (auto [entity, transform, drawable] : ecs->View<Component::Transform, Component::Drawable3D>()) {
			const Model::MeshCollection& collection = drawable.GetCollection();
			for (const auto [mesh, material] : collection) {
				Engine::Shader& shader = *material->shader;
				shader.Enable();
				shader.SetUniform(*material);

				shader.SetUniform("uLightDir", lightDir);
				shader.SetUniform("uViewPos", viewPos);
				shader.SetUniform("uLightColor", lightColor);

				shader.SetUniform("uModel", transform.modelMatrix); // rotates randomly a little which is GOOD
				shader.SetUniform("uView", mainCam->viewMatrix);
				shader.SetUniform("uProj", mainCam->projectionMatrix);

				mesh->Draw();
			}
		}

		auto vendor = glGetString(GL_VENDOR);
		auto end = std::chrono::high_resolution_clock::now();
		// Log::info("ECS_iteration: {} ns", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
	}

	void SceneLayer::OnUpdate(float deltaTime) {
		m_Scene->Update(deltaTime);
	}

	void SceneLayer::OnUpdateFixed(float deltaTime) {
		m_Scene->UpdateFixed(deltaTime);
	}

	void SceneLayer::Reload() {
		m_Scene->Reload();
	}

}