#pragma once

#include <engine/api.hpp>
#include <engine/types.hpp>
#include <engine/exception.hpp>

#include <functional>   // For std::function
#include <typeindex>    // For std::type_index
#include <limits>

namespace Engine {

	// Forward decl of external stuff
	struct Model; // 3D model

	// Forward declarations for types used in the ECS class interface
	struct ECSImpl;
	class ISystem;
	class RefTransform;
	class TransformSystem;
	class ECS;

	namespace detail {
		// Non-templated interface for a component storage pool.
		class IComponentPool {
		public:
			virtual ~IComponentPool() = default;
			// A virtual function to handle removing a component when an entity is destroyed.
			virtual void OnEntityDestroyed(entity_id entity) = 0;
			// Pure virtual functions for the type-erased API
			virtual void Add(entity_id entity, void* pData) = 0;
			virtual void* Get(entity_id entity) = 0;
			virtual void Remove(entity_id entity) = 0;
			virtual bool Has(entity_id entity) const = 0;
			// View stuff
			virtual size_t Size() const = 0;
			virtual entity_id DenseToEntity(size_t index) const = 0;
		};

		// Templated, concrete implementation of component storage using a sparse/dense set.
		template<typename T>
		class ComponentPool : public IComponentPool {
		public:
			// Adds a component for an entity.
			void Add(entity_id entity, void* pData) override {
				if (Has(entity)) ENGINE_THROW("Entity already has this component.");

				// Ensure the sparse array is large enough before we write to it.
				// This is the optimized part. Instead of resizing with a value,
				// we just default-construct new elements, which is faster for POD types.
				constexpr int grow_factor = 2;
				if (entity >= m_Sparse.size()) {
					m_Sparse.resize((entity * grow_factor) + 1, null);
				}

				// Map the entity to its future location in the dense array.
				const u32 denseIndex = static_cast<u32>(m_Dense.size());
				m_Sparse[entity] = denseIndex;

				// Use emplace_back for potentially more efficient construction.
				m_Dense.emplace_back(*static_cast<T*>(pData));
				m_DenseToEntity.emplace_back(entity);
			}

			// Retrieves a pointer to an entity's component.
			void* Get(entity_id entity) override {
				if (!Has(entity)) ENGINE_THROW("Entity does not have this component to get.");
				return &m_Dense[m_Sparse[entity]];
			}

			// Removes a component from an entity using swap-and-pop.
			void Remove(entity_id entity) override {
				if (!Has(entity)) ENGINE_THROW("Entity does not have this component to remove.");

				const u32 denseIndexOfRemoved = m_Sparse[entity];
				const u32 denseIndexOfLast = static_cast<u32>(m_Dense.size() - 1);

				// Check if the element to remove is already the last one.
				// If so, we can skip the expensive swap and just pop.
				if (denseIndexOfRemoved == denseIndexOfLast) {
					m_Dense.pop_back();
					m_DenseToEntity.pop_back();
				}
				else {
					// It's not the last element, so we perform the swap.
					const entity_id entityOfLast = m_DenseToEntity[denseIndexOfLast];

					// Move the last element's data into the slot of the removed element.
					m_Dense[denseIndexOfRemoved] = std::move(m_Dense[denseIndexOfLast]);
					m_DenseToEntity[denseIndexOfRemoved] = entityOfLast;

					// Update the sparse map for the entity that was just moved.
					m_Sparse[entityOfLast] = denseIndexOfRemoved;

					// Shrink the dense arrays. These are cheap O(1) operations.
					m_Dense.pop_back();
					m_DenseToEntity.pop_back();
				}

				// Invalidate the sparse entry for the removed entity.
				m_Sparse[entity] = null;
			}

			// Checks if an entity has a component.
			bool Has(entity_id entity) const override {
				// The entity must be within the bounds of the sparse array
				// and its entry must not be the 'null' sentinel value.
				return entity < m_Sparse.size() && m_Sparse[entity] != null;
			}

			// Called by ECS::DestroyEntity to clean up a component if it exists.
			void OnEntityDestroyed(entity_id entity) override {
				if (Has(entity)) {
					Remove(entity);
				}
			}

			size_t Size() const override {
				return m_Dense.size();
			}

			entity_id DenseToEntity(size_t index) const override {
				if (index >= m_Dense.size()) ENGINE_THROW("Out of bound external access");
				return m_DenseToEntity[index];
			}

		private:
			// Tightly packed array of component data.
			std::vector<T> m_Dense;
			// Maps a dense index back to its owner entity ID.
			std::vector<entity_id> m_DenseToEntity;
			// Maps an entity ID to its index in the dense array.
			std::vector<u32> m_Sparse;
		};
	} // namespace detail

	template<typename... Components>
	class ViewIterator {
	public:
		// STL stuff
		using value_type = std::tuple<entity_id, Components&...>;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type*;
		using reference = value_type;
		using iterator_category = std::forward_iterator_tag;

		ViewIterator(ECS* ecs, const detail::IComponentPool* pool, size_t index)
			: m_Ecs{ ecs }, m_Pool{ pool }, m_Index{ index } {
			AdvanceToValid();
		}

		reference operator*() const {
			entity_id entity = m_Pool->DenseToEntity(m_Index);
			return value_type(entity, m_Ecs->GetComponent<Components>(entity)...);
		}

		ViewIterator& operator++() {
			++m_Index;
			AdvanceToValid();
			return *this;
		}

		ViewIterator operator++(int) {
			ViewIterator temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(const ViewIterator& other) const {
			return m_Index == other.m_Index;
		}

		bool operator!=(const ViewIterator& other) const {
			return !(*this == other);
		}

	private:
		bool HasAllComponents(entity_id entity) const {
			// Check all types with folded expression
			return (m_Ecs->template HasComponent<Components>(entity) && ...);
		}

		void AdvanceToValid() {
			while (m_Index < m_Pool->Size()) {
				entity_id entity = m_Pool->DenseToEntity(m_Index);
				if (HasAllComponents(entity)) break;
				++m_Index;
			}
		}

		ECS* m_Ecs;
		const detail::IComponentPool* m_Pool;
		size_t m_Index;
	};

	template <typename... Components>
	class View {
	public:
		using iterator = ViewIterator<Components...>;
		View(ECS* ecs) : m_Ecs{ ecs }, m_SmallestPool{ nullptr } {
			static_assert(sizeof...(Components) > 0, "View must have at least one component type");
			FindSmallestPool();
		}

		iterator begin() const {
			if (!m_SmallestPool) return iterator(m_Ecs, nullptr, 0);
			return iterator(m_Ecs, m_SmallestPool, 0);
		}

		iterator end() const {
			if (!m_SmallestPool) return iterator(m_Ecs, nullptr, 0);
			return iterator(m_Ecs, m_SmallestPool, m_SmallestPool->Size());
		}

	private:
		size_t size_hint() const {
			return m_SmallestPool ? m_SmallestPool->Size() : 0;
		}

		bool empty() const {
			return !m_SmallestPool || (m_SmallestPool->Size() == 0);
		}

		void FindSmallestPool() {
			size_t minSize = std::numeric_limits<size_t>::max();
			detail::IComponentPool* smallestPool = nullptr;
			((CheckPool<Components>(minSize, smallestPool)), ...);
			m_SmallestPool = smallestPool;
		}

		template<typename T>
		void CheckPool(size_t& minSize, detail::IComponentPool*& smallestPool) {
			std::type_index type = std::type_index(typeid(T));
			detail::IComponentPool* pool = m_Ecs->GetPoolImpl(type);
			size_t poolSize = pool->Size();
			if (poolSize < minSize) {
				minSize = poolSize;
				smallestPool = pool;
			}
		}

	private:
		ECS* m_Ecs;
		detail::IComponentPool* m_SmallestPool;
	};

	class ECS {
	public:
		ENGINE_API ECS();
		ENGINE_API ~ECS();

		// Disallow copying
		ENGINE_API ECS(const ECS&) = delete;
		ENGINE_API ECS& operator=(const ECS&) = delete;

		// Entity management
		ENGINE_API entity_id CreateEntity();
		ENGINE_API entity_id CreateEntity3D(entity_id parent = null, Component::Transform transform = Component::Transform(), const std::string& name = "");
		ENGINE_API entity_id Instantiate(entity_id parent, Component::Transform rootTransform, std::shared_ptr<Model> model);
		ENGINE_API void DestroyEntity(entity_id entity, bool recurse = false);

		// Special functions
		ENGINE_API RefTransform GetTransformRef(entity_id entity);
		ENGINE_API const unordered_set<entity_id>& GetDeletedEntities() const;
		ENGINE_API void ReparentEntity(entity_id entity, entity_id new_parent);
		ENGINE_API bool Exists(entity_id entity) const;
		

		// Component management
		template<typename T>
		void RegisterComponent() {
			std::type_index type_idx = std::type_index(typeid(T));
			// This lambda captures how to create the specific ComponentPool<T>.
			// It will be passed across the DLL boundary to the implementation.
			std::function<std::unique_ptr<detail::IComponentPool>()> factory =
				[]() { return std::make_unique<detail::ComponentPool<T>>(); };
			RegisterComponentImpl(type_idx, std::move(factory));
		}

		template<typename T>
		T& AddComponent(entity_id entity, T component) {
			std::type_index type_idx = std::type_index(typeid(T));
			// We pass the component data via a void pointer (type erasure).
			AddComponentImpl(entity, type_idx, &component);
			// The implementation will place it in storage, and we can get it back.
			return GetComponent<T>(entity);
		}

		template<typename T>
		T& GetComponent(entity_id entity) {
			std::type_index type_idx = std::type_index(typeid(T));
			void* pData = GetComponentImpl(entity, type_idx);
			if (!pData) ENGINE_THROW("Component not found or entity is invalid.");
			return *static_cast<T*>(pData);
		}

		template<typename T>
		void RemoveComponent(entity_id entity) {
			std::type_index type_idx = std::type_index(typeid(T));
			RemoveComponentImpl(entity, type_idx);
		}

		template<typename T>
		bool HasComponent(entity_id entity) {
			std::type_index type_idx = std::type_index(typeid(T));
			return HasComponentImpl(entity, type_idx);
		}

		// System management
		template<typename T>
		std::shared_ptr<T> RegisterSystem() {
			std::type_index type_idx = std::type_index(typeid(T));
			auto system = std::make_shared<T>(*this);
			RegisterSystemImpl(type_idx, system);
			return system;
		}

		template<typename T>
		std::shared_ptr<T> GetSystem() {
			std::type_index type_idx = std::type_index(typeid(T));
			return std::static_pointer_cast<T>(GetSystemImpl(type_idx));
		}

		template<typename T>
		detail::ComponentPool<T>* GetPool() {
			std::type_index type_idx = std::type_index(typeid(T));
			return std::static_pointer_cast<detail::ComponentPool<T>>(GetSystemImpl(type_idx));
		}

		template<typename... Components>
		View<Components...> View() {
			return ::Engine::View<Components...>(this);
		}

	private:
		template <typename ... Components>
		friend class View;

		// Non-templated functions
		ENGINE_API void RegisterComponentImpl(std::type_index type, std::function<std::unique_ptr<detail::IComponentPool>()> factory);
		ENGINE_API void AddComponentImpl(entity_id entity, std::type_index type, void* pData);
		ENGINE_API void* GetComponentImpl(entity_id entity, std::type_index type);
		ENGINE_API void RemoveComponentImpl(entity_id entity, std::type_index type);
		ENGINE_API bool HasComponentImpl(entity_id entity, std::type_index type);
		ENGINE_API detail::IComponentPool* GetPoolImpl(std::type_index type);

		ENGINE_API void RegisterSystemImpl(std::type_index type, std::shared_ptr<ISystem> system);
		ENGINE_API std::shared_ptr<ISystem> GetSystemImpl(std::type_index type);

		std::unique_ptr<ECSImpl> m_Impl;
	};

	// Iterates over all children
	class SiblingIterator {
	public:
		using value_type = entity_id;
		using difference_type = std::ptrdiff_t;
		using pointer = const value_type*;
		using reference = value_type;
		using iterator_category = std::forward_iterator_tag;

		SiblingIterator(ECS* ecs, entity_id current)
			: m_Ecs{ ecs }, m_Current{ current } {}

		reference operator*() const { return m_Current; }

		SiblingIterator& operator++() {
			Component::Hierarchy& h = m_Ecs->GetComponent<Component::Hierarchy>(m_Current);
			m_Current = h.next_sibling;
			return *this;
		}

		bool operator==(const SiblingIterator& other) const { return m_Current == other.m_Current; }
		bool operator!=(const SiblingIterator& other) const { return !(*this == other); }

	private:
		ECS* m_Ecs;
		entity_id m_Current;
	};

	class ChildrenRange {
	public:
		ChildrenRange(ECS* ecs, entity_id parent)
			: m_Ecs{ ecs }, m_Parent{ parent } {
		}

		auto begin() const {
			auto& h = m_Ecs->GetComponent<Component::Hierarchy>(m_Parent);
			return SiblingIterator{ m_Ecs, h.first_child };
		}

		auto end() const { return SiblingIterator{ m_Ecs, null }; }

	private:
		ECS* m_Ecs;
		entity_id m_Parent;
	};

	// Generic ECS system interface
	class ISystem {
	public:
		ENGINE_API ISystem(ECS& ecs) : m_Ecs{ &ecs } {}
		ENGINE_API ~ISystem() = default;

		ENGINE_API virtual optional<vector<entity_id>> Update(f32 deltaTime) = 0;
		ENGINE_API virtual void PostUpdate() = 0;
	protected:
		ECS* m_Ecs;
	};

	// Implements hierarchical updates
	class TransformSystem : public ISystem {
	public:
		ENGINE_API TransformSystem(ECS& ecs) : ISystem(ecs) {}

		ENGINE_API void Enqueue(entity_id entity);
		ENGINE_API optional<vector<entity_id>> Update(f32 deltaTime) override;
		ENGINE_API void PostUpdate() override;
	private:
		vector<vector<entity_id>> m_DepthBuckets;
		unordered_set<entity_id> m_Registered;
	};

	// Should be used if we plan on modifying the transform
	class RefTransform {
		friend ECS;
	public:
		ENGINE_API ~RefTransform(); // logic to submit ourselves for update if isDirty
		
		ENGINE_API const Component::Transform& GetTransform() const; // read-only view
		ENGINE_API void SetTransform(Component::Transform& transform); // copy into our data, isDirty=true

		ENGINE_API vec3 GetPosition() const;
		ENGINE_API void SetPosition(vec3 position);

		ENGINE_API quat GetRotation() const;
		ENGINE_API vec3 GetRotationEuler() const;
		ENGINE_API void SetRotation(vec3 euler);
		ENGINE_API void SetRotation(quat rotation);

		ENGINE_API void RotateAround(vec3 axis, float radians);

		ENGINE_API vec3 GetScale() const;
		ENGINE_API void SetScale(vec3 scale);
	private:
		ENGINE_API RefTransform(ECS& ecs, entity_id entity, Component::Transform& transform, TransformSystem& system);
		Component::Transform& data;
		entity_id id;
		ECS& ecs;
		TransformSystem& system;
		bool isDirty;
	};
} // namespace Engine
