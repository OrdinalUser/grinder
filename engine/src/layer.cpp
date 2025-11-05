#include <engine/layer.hpp>
#include <engine/exception.hpp>

namespace Engine {

	LayerStack::~LayerStack() {
		for (ILayer* layer : m_Layers) {
			layer->OnDetach();
			delete layer;
		}
		m_Layers.clear();
	}

	void LayerStack::Push(ILayer* layer) {
		layer->OnAttach();
		m_Layers.emplace_back(layer);
	}

	void LayerStack::Pop(ILayer* layer) {
		auto loc = std::find(m_Layers.begin(), m_Layers.end(), layer);
		if (loc == m_Layers.end()) {
			ENGINE_THROW("Attempting to pop layer " + layer->GetName() + " that isn't on layer stack");
		}
		layer->OnDetach();
		m_Layers.erase(loc);
	}

	bool LayerStack::empty() const {
		return m_Layers.empty();
	}
}