#pragma once

#include <engine/api.hpp>
#include <engine/types.hpp>

#include <functional>   // For std::function
#include <typeindex>    // For std::type_index
#include <cassert>      // For assertions

namespace Engine {

	using entity_id = u32; // entity id type, please use
	constexpr entity_id null = 0xFFFFFFFF; // entity_id that represents no entity

	namespace Component {
		// Default initialized to identity values
		struct Transform {
			vec3 position{ 0.0f };
			quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
			vec3 scale{ 1.0f };
			mat4 modelMatrix{ 1.0f };
		};

		// No default initialization, please tread carefully
		struct Hierarchy {
			entity_id parent;
			entity_id first_child;
			entity_id next_sibling;
			entity_id prev_sibling;
			u16 depth;
		};
	}

	// Forward declarations for types used in the ECS class interface
	struct ECSImpl;
	class ISystem;
	class RefTransform;
	class TransformSystem;

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
		};

		// Templated, concrete implementation of component storage using a sparse/dense set.
		template<typename T>
		class ComponentPool : public IComponentPool {
		public:
			// Adds a component for an entity.
			void Add(entity_id entity, void* pData) override {
				assert(!Has(entity) && "Entity already has this component.");

				// Ensure the sparse array is large enough before we write to it.
				// This is the optimized part. Instead of resizing with a value,
				// we just default-construct new elements, which is faster for POD types.
				constexpr int grow_factor = 2;
				if (entity >= m_Sparse.size()) {
					m_Sparse.resize((m_Sparse.size() * grow_factor) + 1, null);
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
				assert(Has(entity) && "Entity does not have this component to get.");
				return &m_Dense[m_Sparse[entity]];
			}

			// Removes a component from an entity using swap-and-pop.
			void Remove(entity_id entity) override {
				assert(Has(entity) && "Entity does not have this component to remove.");

				// --- OPTIMIZED SWAP-AND-POP ---

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

		private:
			// Tightly packed array of component data.
			std::vector<T> m_Dense;
			// Maps a dense index back to its owner entity ID.
			std::vector<entity_id> m_DenseToEntity;
			// Maps an entity ID to its index in the dense array.
			std::vector<u32> m_Sparse;
		};
	} // namespace detail

	class ECS {
	public:
		ENGINE_API ECS();
		ENGINE_API ~ECS();

		// Disallow copying
		ENGINE_API ECS(const ECS&) = delete;
		ENGINE_API ECS& operator=(const ECS&) = delete;

		// Entity management
		ENGINE_API entity_id CreateEntity();
		ENGINE_API entity_id CreateEntity3D(entity_id parent = null, Component::Transform transform = Component::Transform());
		ENGINE_API void DestroyEntity(entity_id entity);

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
			assert(pData && "Component not found or entity is invalid.");
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


	private:
		// Non-templated functions
		ENGINE_API void RegisterComponentImpl(std::type_index type, std::function<std::unique_ptr<detail::IComponentPool>()> factory);
		ENGINE_API void AddComponentImpl(entity_id entity, std::type_index type, void* pData);
		ENGINE_API void* GetComponentImpl(entity_id entity, std::type_index type);
		ENGINE_API void RemoveComponentImpl(entity_id entity, std::type_index type);
		ENGINE_API bool HasComponentImpl(entity_id entity, std::type_index type);

		ENGINE_API void RegisterSystemImpl(std::type_index type, std::shared_ptr<ISystem> system);
		ENGINE_API std::shared_ptr<ISystem> GetSystemImpl(std::type_index type);

		std::unique_ptr<ECSImpl> m_Impl;
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
