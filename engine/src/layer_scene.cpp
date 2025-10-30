#include <engine/layer.hpp>
#include <engine/scene.hpp>
#include <engine/ecs.hpp>

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
		// Update renderer here, #TODO
		m_Scene->Render();
	}

	void SceneLayer::OnUpdate(float deltaTime) {
		m_Scene->Update(deltaTime);
	}

	void SceneLayer::Reload() {
		m_Scene->Reload();
	}

}