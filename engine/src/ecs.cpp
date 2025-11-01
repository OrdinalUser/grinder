#include <engine/ecs.hpp>
#include <engine/exception.hpp>
#include <engine/resource.hpp>

namespace Engine {
	struct ECSImpl {
		// Entity Management
		entity_id m_NextEntityID = 0;
		std::unordered_set<entity_id> m_FreeEntitySet;

		// Component Management: Maps a component's type_index to its storage pool.
		std::unordered_map<std::type_index, std::unique_ptr<detail::IComponentPool>> m_ComponentPools;

		// System Management: Maps a system's type_index to its instance.
		std::unordered_map<std::type_index, std::shared_ptr<ISystem>> m_Systems;

		// Helper function to safely get a component pool.
		detail::IComponentPool* GetPool(std::type_index type) {
			auto it = m_ComponentPools.find(type);
			if (it == m_ComponentPools.end()) {
				return nullptr; // Pool doesn't exist for this type.
			}
			return it->second.get();
		}
	};

	ECS::ECS() : m_Impl(std::make_unique<ECSImpl>()) {
		// Register default components and systems
		RegisterComponent<Component::Transform>();
		RegisterComponent<Component::Hierarchy>();
		RegisterComponent<Component::Light>();
		RegisterComponent<Component::Drawable3D>();
		RegisterComponent<Component::Name>();
		RegisterComponent<Component::Camera>();
		RegisterSystem<TransformSystem>();
	}

	ECS::~ECS() = default;

	entity_id ECS::CreateEntity() {
		entity_id id;
		if (!m_Impl->m_FreeEntitySet.empty()) {
			// Grab an arbitrary recycled ID from the set.
			id = *m_Impl->m_FreeEntitySet.begin();
			// Remove it from the free list.
			m_Impl->m_FreeEntitySet.erase(m_Impl->m_FreeEntitySet.begin());
		}
		else {
			id = m_Impl->m_NextEntityID++;
		}
		return id;
	}

	entity_id ECS::CreateEntity3D(entity_id parent, Component::Transform transform, const std::string& name) {
		// Create a base entity with a unique ID.
		const entity_id id = CreateEntity();

		// Add a default Transform component, as all 3D entities have one.
		AddComponent(id, transform);

		// Create and configure the Hierarchy component based on the parent.
		Component::Hierarchy hierarchy = { null, null, null, null, 0 }; // default initialized to a root element
		if (parent != null) {
			// Get a mutable reference to the parent's hierarchy to modify it.
			auto& parentHierarchy = GetComponent<Component::Hierarchy>(parent);
			const entity_id oldFirstChild = parentHierarchy.first_child;

			// Configure the new entity's hierarchy:
			hierarchy.parent = parent;
			hierarchy.depth = parentHierarchy.depth + 1;
			hierarchy.next_sibling = oldFirstChild; // The old first child is now our next sibling.
			hierarchy.first_child = null;
			hierarchy.prev_sibling = null;

			// Update the parent to point to this new entity as its first child.
			parentHierarchy.first_child = id;

			// If there was an old first child, we need to update its 'prev_sibling'
			// to point to our new entity, completing the linked-list insertion.
			if (oldFirstChild != null) {
				auto& oldFirstChildHierarchy = GetComponent<Component::Hierarchy>(oldFirstChild);
				oldFirstChildHierarchy.prev_sibling = id;
			}
		}

		// Add the fully configured Hierarchy component to the new entity.
		AddComponent(id, hierarchy);

		// Add a name if provided
		if (name.size() != 0) {
			AddComponent(id, Component::Name{ .name = name });
		}

		return id;
	}

	// This is a recursive helper to efficiently update the depth of all descendants of a moved entity.
	static void UpdateDescendantDepths(ECS& ecs, entity_id entity, int depth_delta) {
		if (depth_delta == 0) return;

		auto& hierarchy = ecs.GetComponent<Component::Hierarchy>(entity);
		hierarchy.depth = static_cast<u16>(static_cast<int>(hierarchy.depth) + depth_delta);

		entity_id child = hierarchy.first_child;
		while (child != null) {
			UpdateDescendantDepths(ecs, child, depth_delta);
			child = ecs.GetComponent<Component::Hierarchy>(child).next_sibling;
		}
	}

	void ECS::ReparentEntity(entity_id entity, entity_id new_parent) {
		if (!HasComponent<Component::Hierarchy>(entity)) ENGINE_THROW("Cannot reparent an entity that does not have a Hierarchy component.");

		auto& hierarchy_to_move = GetComponent<Component::Hierarchy>(entity);
		const entity_id old_parent = hierarchy_to_move.parent;

		// --- SAFETY CHECKS ---
		if (old_parent == new_parent) return; // No-op
		if (entity == new_parent) ENGINE_THROW("Cannot parent an entity to itself.");

		// Critical: Prevent creating a cycle (parenting an entity to one of its own descendants).
		entity_id ancestor = new_parent;
		while (ancestor != null) {
			if (ancestor == entity) ENGINE_THROW("Cannot parent an entity to one of its own descendants.");
			ancestor = GetComponent<Component::Hierarchy>(ancestor).parent;
		}

		// --- 1. DETACH FROM OLD SIBLING LIST ---
		if (old_parent != null) {
			auto& old_parent_hierarchy = GetComponent<Component::Hierarchy>(old_parent);
			if (old_parent_hierarchy.first_child == entity) {
				old_parent_hierarchy.first_child = hierarchy_to_move.next_sibling;
			}
		}
		if (hierarchy_to_move.prev_sibling != null) {
			GetComponent<Component::Hierarchy>(hierarchy_to_move.prev_sibling).next_sibling = hierarchy_to_move.next_sibling;
		}
		if (hierarchy_to_move.next_sibling != null) {
			GetComponent<Component::Hierarchy>(hierarchy_to_move.next_sibling).prev_sibling = hierarchy_to_move.prev_sibling;
		}

		// --- 2. ATTACH TO NEW PARENT (as the new first child) ---
		hierarchy_to_move.parent = new_parent;
		entity_id old_first_child = null;

		if (new_parent != null) {
			auto& new_parent_hierarchy = GetComponent<Component::Hierarchy>(new_parent);
			old_first_child = new_parent_hierarchy.first_child;
			new_parent_hierarchy.first_child = entity;
		}

		hierarchy_to_move.prev_sibling = null;
		hierarchy_to_move.next_sibling = old_first_child;

		if (old_first_child != null) {
			GetComponent<Component::Hierarchy>(old_first_child).prev_sibling = entity;
		}

		// --- 3. UPDATE DEPTH ---
		u16 old_depth = hierarchy_to_move.depth;
		u16 new_depth = (new_parent == null) ? 0 : GetComponent<Component::Hierarchy>(new_parent).depth + 1;

		// Pass 'this' to the static helper function
		UpdateDescendantDepths(*this, entity, static_cast<int>(new_depth) - static_cast<int>(old_depth));

		// --- 4. MARK AS DIRTY ---
		// The entity's transform is now invalid and must be recalculated.
		GetSystem<TransformSystem>()->Enqueue(entity);
	}

	void ECS::DestroyEntity(entity_id entity, bool recurse) {
		if (!Exists(entity))
			return;

		if (recurse && HasComponent<Component::Hierarchy>(entity)) {
			auto& hierarchy = GetComponent<Component::Hierarchy>(entity);
			entity_id child = hierarchy.first_child;
			while (child != null) {
				entity_id next = GetComponent<Component::Hierarchy>(child).next_sibling;
				DestroyEntity(child, true); // recurse downwards
				child = next;
			}
		}

		if (HasComponent<Component::Hierarchy>(entity)) {
			auto& hierarchy_to_destroy = GetComponent<Component::Hierarchy>(entity);
			const entity_id parent = hierarchy_to_destroy.parent;

			// If not recursive, reparent children upward
			if (!recurse) {
				std::vector<entity_id> children_to_reparent;
				entity_id child = hierarchy_to_destroy.first_child;
				while (child != null) {
					children_to_reparent.push_back(child);
					child = GetComponent<Component::Hierarchy>(child).next_sibling;
				}
				for (entity_id child_to_move : children_to_reparent)
					ReparentEntity(child_to_move, parent);
			}

			// Detach from parent and siblings
			if (hierarchy_to_destroy.parent != null) {
				auto& parent_hierarchy = GetComponent<Component::Hierarchy>(hierarchy_to_destroy.parent);
				if (parent_hierarchy.first_child == entity)
					parent_hierarchy.first_child = hierarchy_to_destroy.next_sibling;
			}
			if (hierarchy_to_destroy.prev_sibling != null)
				GetComponent<Component::Hierarchy>(hierarchy_to_destroy.prev_sibling).next_sibling = hierarchy_to_destroy.next_sibling;
			if (hierarchy_to_destroy.next_sibling != null)
				GetComponent<Component::Hierarchy>(hierarchy_to_destroy.next_sibling).prev_sibling = hierarchy_to_destroy.prev_sibling;
		}

		// Wipe out components
		for (auto const& [type, pool] : m_Impl->m_ComponentPools)
			pool->OnEntityDestroyed(entity);

		// Recycle the entity ID
		m_Impl->m_FreeEntitySet.insert(entity);
	}

	// --- Component Implementation ---

	void ECS::RegisterComponentImpl(std::type_index type, std::function<std::unique_ptr<detail::IComponentPool>()> factory) {
		if (m_Impl->m_ComponentPools.find(type) != m_Impl->m_ComponentPools.end()) ENGINE_THROW("Component type already registered.");
		// The factory function passed from the header knows how to create the correct ComponentPool<T>.
		m_Impl->m_ComponentPools[type] = factory();
	}

	void ECS::AddComponentImpl(entity_id entity, std::type_index type, void* pData) {
		detail::IComponentPool* pool = m_Impl->GetPool(type);
		if (!pool) ENGINE_THROW("Component type not registered.");
		pool->Add(entity, pData);
	}

	void* ECS::GetComponentImpl(entity_id entity, std::type_index type) {
		detail::IComponentPool* pool = m_Impl->GetPool(type);
		if (!pool) ENGINE_THROW("Component type not registered.");
		return pool->Get(entity);
	}

	void ECS::RemoveComponentImpl(entity_id entity, std::type_index type) {
		detail::IComponentPool* pool = m_Impl->GetPool(type);
		if (!pool) ENGINE_THROW("Component type not registered.");
		pool->Remove(entity);
	}

	bool ECS::HasComponentImpl(entity_id entity, std::type_index type) {
		detail::IComponentPool* pool = m_Impl->GetPool(type);
		if (!pool) {
			return false; // If the component type isn't even registered, it can't have one.
		}
		return pool->Has(entity);
	}

	detail::IComponentPool* ECS::GetPoolImpl(std::type_index type) {
		detail::IComponentPool* pool = m_Impl->GetPool(type);
		if (!pool) ENGINE_THROW("Component not registered");
		return pool;
	}

	// --- System Implementation ---
	void ECS::RegisterSystemImpl(std::type_index type, std::shared_ptr<ISystem> system) {
		if (m_Impl->m_Systems.find(type) != m_Impl->m_Systems.end()) ENGINE_THROW("System type already registered.");
		m_Impl->m_Systems[type] = system;
	}

	std::shared_ptr<ISystem> ECS::GetSystemImpl(std::type_index type) {
		auto it = m_Impl->m_Systems.find(type);
		if (it == m_Impl->m_Systems.end()) ENGINE_THROW("System type not registered.");
		return it->second;
	}

	// --- Special handles ---
	RefTransform ECS::GetTransformRef(entity_id entity) {
		// Get the TransformSystem instance from the system registry.
		auto transformSystem = GetSystem<TransformSystem>();
		if (!transformSystem) ENGINE_THROW("TransformSystem is not registered.");

		// Get the actual Transform component data from its pool.
		auto& transformComponent = GetComponent<Component::Transform>(entity);

		// Return a reference with some other bound information
		return RefTransform(
			*this,
			entity,
			transformComponent,
			*transformSystem
		);
	}
	
	bool ECS::Exists(entity_id entity) const {
		// Exists if id is less than maxId and not being recycled
		return entity < m_Impl->m_NextEntityID && !m_Impl->m_FreeEntitySet.contains(entity);
	}

	entity_id ECS::Instantiate(entity_id parent, Component::Transform rootTransform, std::shared_ptr<Model> model) {
		if (!model) ENGINE_THROW("Trying to instantiate non-existant model");

		// Keep track of mapping from blueprint node index → actual entity
		std::vector<entity_id> entityMap(model->blueprint.size(), null);
		entity_id rootEntity = null;

		// Step 1: create entities for each blueprint node
		for (size_t i = 0; i < model->blueprint.size(); ++i) {
			const auto& bp = model->blueprint[i];

			// Combine blueprint transform with the instantiation root if this is the root node
			Component::Transform worldTransform = bp.transform;
			if (bp.parent == null) {
				worldTransform.modelMatrix = rootTransform.modelMatrix * worldTransform.modelMatrix;
			}

			// Create entity; if node has a parent, use the parent's entity id
			entity_id parentEntity = (bp.parent != null)
				? entityMap[bp.parent]
				: parent;

			entity_id entity = CreateEntity3D(parentEntity, worldTransform);
			entityMap[i] = entity;

			if (bp.parent == null)
				rootEntity = entity;

			// Step 2: attach the drawable for this node’s collection
			if (bp.collectionIndex < model->collections.size()) {
				auto drawable = Component::Drawable3D{
					.model = model,
					.collectionIndex = bp.collectionIndex
				};
				AddComponent<Component::Drawable3D>(entity, drawable);
				AddComponent<Component::Name>(entity, Component::Name{.name = bp.name});
			}

			// Step 3: schedule transform system update
			GetSystem<TransformSystem>()->Enqueue(entity);
		}
		return rootEntity;
	}

	RefTransform::~RefTransform() {
		if (isDirty) system.Enqueue(id);
	}

	const Component::Transform& RefTransform::GetTransform() const {
		return data;
	}

	vec3 RefTransform::GetPosition() const {
		return data.position;
	}

	quat RefTransform::GetRotation() const {
		return data.rotation;
	}

	// Helper to convert the internal quaternion to user-friendly Euler angles.
	vec3 RefTransform::GetRotationEuler() const {
		// glm::eulerAngles returns radians, so we convert to degrees for easier use.
		return glm::degrees(glm::eulerAngles(data.rotation));
	}

	vec3 RefTransform::GetScale() const {
		return data.scale;
	}

	RefTransform::RefTransform(ECS& ecs, entity_id entity, Component::Transform& transform, TransformSystem& system)
		: ecs{ ecs }, id{ entity }, data{ transform }, system{ system }, isDirty{ false } {
	}

	void RefTransform::SetTransform(Component::Transform& transform) {
		data = transform;
		isDirty = true;
	}

	void RefTransform::SetPosition(vec3 position) {
		data.position = position;
		isDirty = true;
	}

	// Sets the rotation directly using a quaternion.
	void RefTransform::SetRotation(quat rotation) {
		data.rotation = rotation;
		isDirty = true;
	}

	// Sets the rotation using Euler angles (in degrees).
	void RefTransform::SetRotation(vec3 euler) {
		// Convert degrees to radians and then create the quaternion.
		data.rotation = glm::quat(glm::radians(euler));
		isDirty = true;
	}

	void RefTransform::RotateAround(vec3 axis, float radians) {
		data.rotation = glm::angleAxis(radians, axis) * data.rotation;
		isDirty = true;
	}

	void RefTransform::SetScale(vec3 scale) {
		data.scale = scale;
		isDirty = true;
	}

	// --- Systems impl ---

	void TransformSystem::Enqueue(entity_id entity) {
		// Check if entity has already been registered for update and skip
		if (m_Registered.find(entity) != m_Registered.end()) return;

		// Fetch pre-computed entity depth from the hierarchy
		u16 depth = m_Ecs->GetComponent<Component::Hierarchy>(entity).depth;

		// Ensure our bucket vector is large enough for this entity's depth
		if (depth >= m_DepthBuckets.size()) {
			m_DepthBuckets.resize(depth + 1);
		}

		// Register entity for update
		m_DepthBuckets[depth].push_back(entity);
		m_Registered.insert(entity);
	};
	
	optional<vector<entity_id>> TransformSystem::Update(f32 deltaTime) {
		(void)deltaTime;
		vector<entity_id> updatedEntities;
		// Iterate through each depth level, starting from the root (depth 0).
		// We use an index-based loop because the m_DepthBuckets vector can and will grow during iteration
		// as we enqueue children from parent transforms.
		for (size_t depth = 0; depth < m_DepthBuckets.size(); ++depth) {
			const auto& bucket = m_DepthBuckets[depth];

			// Process every entity that has been marked dirty at this depth level.
			for (entity_id entity : bucket) {
				// Check if entity is still valid
				if (!m_Ecs->Exists(entity)) continue;

				updatedEntities.push_back(entity);
				auto& transform = m_Ecs->GetComponent<Component::Transform>(entity);
				const auto& hierarchy = m_Ecs->GetComponent<Component::Hierarchy>(entity);

				// 1. CALCULATE LOCAL MATRIX from individual TRS components.
				// The multiplication order is Scale -> Rotate -> Translate.
				const mat4 localMatrix = glm::translate(mat4(1.0f), transform.position) *
					Math::mat4_cast(transform.rotation) *
					Math::scale(mat4(1.0f), transform.scale);

				// No need to check parents as they already passed the earlier check due to breadth first nature of this update scheme
				// 2. CALCULATE WORLD MATRIX (the final modelMatrix).
				if (hierarchy.parent != null) {
					// This entity has a parent. Its world transform is the parent's world transform
					// multiplied by its own local transform.
					// We are guaranteed the parent's matrix is up-to-date because we process depth-by-depth.
					const auto& parentTransform = m_Ecs->GetComponent<Component::Transform>(hierarchy.parent);
					transform.modelMatrix = parentTransform.modelMatrix * localMatrix;
				}
				else {
					// This is a root entity; its world transform is just its local transform.
					transform.modelMatrix = localMatrix;
				}

				// 3. PROPAGATE the change to all direct children.
				// Since this parent's matrix has changed, all its children are now also "dirty".
				// We enqueue them so they will be processed in their respective (deeper) buckets.
				entity_id child_id = hierarchy.first_child;
				while (child_id != null) {
					Enqueue(child_id); // Enqueue the child for processing.
					// Move to the next child in the sibling list.
					child_id = m_Ecs->GetComponent<Component::Hierarchy>(child_id).next_sibling;
				}
			}
		}

		return std::move(updatedEntities);
	}
	
	void TransformSystem::PostUpdate() {
		// Clear out all update data
		for (auto& bucket : m_DepthBuckets) {
			bucket.clear();
		}
		m_Registered.clear();
	};
} // namespace Engine
