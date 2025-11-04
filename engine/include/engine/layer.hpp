#pragma once

#include <engine/api.hpp>
#include <engine/ecs.hpp>

#include <string>
#include <memory>
#include <vector>

namespace Engine {
	class Scene;

	class ILayer {
	public:
		ENGINE_API virtual ~ILayer() = default;

		// Called when layer is pushed onto the stack
		ENGINE_API virtual void OnAttach() {}

		// Called when layer is popped from the stack
		ENGINE_API virtual void OnDetach() {}

		// Called every frame for logic updates
		ENGINE_API virtual void OnUpdate(float deltaTime) {}

		// Called every fixed frame for physics updates
		ENGINE_API virtual void OnUpdateFixed(float deltaTime) {}

		// called every frame for rendering
		ENGINE_API virtual void OnRender(const std::vector<entity_id>& updatedEntities) {}

		ENGINE_API const std::string& GetName() const { return m_DebugName; }
	protected:
		ENGINE_API ILayer(const std::string& name = "Layer") : m_DebugName{ name } {}
		std::string m_DebugName;
	};

	class LayerStack {
	public:
		ENGINE_API LayerStack() = default;
		ENGINE_API ~LayerStack();

		ENGINE_API void Push(ILayer* layer);
		ENGINE_API void Pop(ILayer* layer);

		// Convenience iteration
		ENGINE_API std::vector<ILayer*>::iterator begin() { return m_Layers.begin(); }
		ENGINE_API std::vector<ILayer*>::iterator end() { return m_Layers.end(); }
		ENGINE_API std::vector<ILayer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
		ENGINE_API std::vector<ILayer*>::reverse_iterator rend() { return m_Layers.rend(); }
	private:
		std::vector<ILayer*> m_Layers;
	};

	class DebugLayer : public ILayer {
	public:
		ENGINE_API DebugLayer();
		ENGINE_API ~DebugLayer();

		ENGINE_API void OnAttach() override;
		ENGINE_API void OnDetach() override;
		ENGINE_API void OnRender(const std::vector<entity_id>& updatedEntities) override;
	private:
		ENGINE_API void Begin();
		ENGINE_API void End();
	};

	class SceneLayer : public ILayer {
	public:
		ENGINE_API SceneLayer(Scene* scene);
		ENGINE_API ~SceneLayer();

		ENGINE_API void OnAttach() override;
		ENGINE_API void OnDetach() override;
		ENGINE_API void OnUpdateFixed(float deltaTime) override;
		ENGINE_API void OnUpdate(float deltaTime) override;
		ENGINE_API void OnRender(const std::vector<entity_id>& updatedEntities) override;
		ENGINE_API void Reload();
	private:
		std::unique_ptr<Scene> m_Scene;
	};
}