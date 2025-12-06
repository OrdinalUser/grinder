#include <engine/layer.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>
#include <engine/application.hpp>
#include <engine/resource.hpp>
#include <engine/vfs.hpp>
#include <engine/renderer.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <memory>
#include <map>

#ifdef _DEBUG
#include <engine/perf_profiler.hpp>
PerfProfiler gProfiler;
#endif

namespace Engine {
	DebugLayer::DebugLayer() : ILayer("DebugLayer") {}

	DebugLayer::~DebugLayer() {}

	void DebugLayer::OnAttach() {
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		// io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
		// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// When viewports are enabled, we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		/*if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}*/

		// Setup Platform/Renderer backends
		Engine::Window& window = Engine::Application::Get().GetWindow();
		ImGui_ImplGlfw_InitForOpenGL(window.GetNativeWindow(), true);
		ImGui_ImplOpenGL3_Init("#version 460");
	}

	void DebugLayer::OnDetach() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void DrawResourceViewer() {
		auto rs = Engine::Application::Get().GetResourceSystem();

		// Resource Browser Window
		if (ImGui::Begin("Resource Browser")) {
			auto& cache = rs->get_cache();

			ImGui::Text("Total Resources: %zu", cache.size());
			ImGui::Separator();

			// Filter input
			static char filter[256] = "";
			ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter));
			ImGui::Separator();

			// Group resources by type
			std::map<std::string, std::vector<std::pair<std::string, std::shared_ptr<IResource>>>> grouped;

			for (const auto& [key, resource] : cache) {
				// Extract type from key (format is "typename|path:sub")
				size_t colonPos = key.find('|');
				if (colonPos != std::string::npos) {
					std::string type = key.substr(0, colonPos);
					std::string path = key.substr(colonPos + 1);

					// Apply filter
					std::string filterStr(filter);
					if (filterStr.empty() || path.find(filterStr) != std::string::npos) {
						grouped[type].push_back({ path, resource });
					}
				}
			}

			// Display grouped resources
			for (const auto& [type, resources] : grouped) {
				if (ImGui::CollapsingHeader((type + " (" + std::to_string(resources.size()) + ")").c_str(),
					ImGuiTreeNodeFlags_DefaultOpen)) {

					ImGui::Indent();

					for (const auto& [path, resource] : resources) {
						ImGui::PushID(path.c_str());

						bool node_open = ImGui::TreeNode("##node", "%s", path.c_str());

						if (node_open) {
							ImGui::Text("Path: %s", resource->getPath().string().c_str());
							// We technically also "own" the resource in debug gui, but we don't care for that
							ImGui::Text("Use Count: %ld", resource.use_count() - 1);

							// Type-specific info
							if (auto img = std::dynamic_pointer_cast<Image>(resource)) {
								ImGui::Text("Dimensions: %dx%d", img->width, img->height);
								ImGui::Text("Channels: %d", img->channels);
								ImGui::Text("Data: %p", img->data);
							}
							else if (auto tex = std::dynamic_pointer_cast<Texture>(resource)) {
								ImGui::Text("OpenGL ID: %u", tex->id);
								ImGui::Text("Dimensions: %dx%d", tex->width, tex->height);

								// Show texture preview
								if (tex->id != 0) {
									ImGui::Text("Preview:");
									float aspect = (float)tex->width / (float)tex->height;
									ImVec2 size(128, 128 / aspect);
									ImGui::Image((void*)(intptr_t)tex->id, size, ImVec2(0, 1), ImVec2(1, 0));
								}
							}
							else if (auto shader = std::dynamic_pointer_cast<Shader>(resource)) {
								ImGui::Text("Program ID: %u", shader->program);
							}
							else if (auto model = std::dynamic_pointer_cast<Model>(resource)) {
								ImGui::Text("Meshes: %zu", model->meshes.size());
                                ImGui::Text("Materials: %zu", model->materials.size());
                                ImGui::Text("Collections: %zu", model->collections.size());
                                ImGui::Text("Bounds:");
                                //
                                ImGui::Columns(4, "bounds_min", false);
                                ImGui::Text("Min");
                                ImGui::NextColumn();
                                ImGui::Text("X: %.3f", model->bounds.min.x);
                                ImGui::NextColumn();
                                ImGui::Text("Y: %.3f", model->bounds.min.y);
                                ImGui::NextColumn();
                                ImGui::Text("Z: %.3f", model->bounds.min.z);
                                //
                                ImGui::Columns(1);
                                ImGui::Spacing();
                                //
                                ImGui::Columns(4, "bounds_max", false);
                                ImGui::Text("Max");
                                ImGui::NextColumn();
                                ImGui::Text("X: %.3f", model->bounds.max.x);
                                ImGui::NextColumn();
                                ImGui::Text("Y: %.3f", model->bounds.max.y);
                                ImGui::NextColumn();
                                ImGui::Text("Z: %.3f", model->bounds.max.z);
                                //
                                ImGui::Columns(1);
							}
                            else if (auto material = std::dynamic_pointer_cast<Material>(resource)) {
                                constexpr const char* RENDER_TYPES_STR[] = {
                                    "UNLIT", "LIT", "TEXTURED", "EMMISIVE"
                                };

                                ImGui::Text("Shader Program ID: %u", material->shader->program);
                                ImGui::Text("Type: %s", RENDER_TYPES_STR[(u8)(material->renderType)]);
                                ImGui::Text("IsTransparent: %u", material->isTransparent);
                                ImGui::Text("Opacity: %.2f", material->opacity);
                                if (material->renderType == Material::RenderType::LIT || material->renderType == Material::RenderType::TEXTURED) {
                                    ImGui::Text("Diffuse color: %.2f %.2f %.2f", material->diffuseColor.r, material->diffuseColor.b, material->diffuseColor.b);
                                    ImGui::Text("Specular color: %.2f %.2f %.2f", material->specularColor.r, material->specularColor.b, material->specularColor.b);
                                    ImGui::Text("Shininess: %.2f", material->shininess);
                                }
                                if (material->renderType == Material::RenderType::EMMISIVE) {
                                    ImGui::Text("Emmisive Intensity: %.2f", material->emmisiveIntensity);
                                    ImGui::Text("Emmisive Color: %.2f %.2f %.2f", material->emmisiveColor.x, material->emmisiveColor.y, material->emmisiveColor.z);
                                    {
                                        auto tex = material->emmisive;
                                        ImGui::Text("Emmisive:");
                                        float aspect = (float)tex->width / (float)tex->height;
                                        ImVec2 size(128, 128 / aspect);
                                        ImGui::Image((void*)(intptr_t)tex->id, size, ImVec2(0, 1), ImVec2(1, 0));
                                    }
                                }
                                if (material->renderType == Material::RenderType::TEXTURED) {
                                    {
                                        auto tex = material->diffuse;
                                        ImGui::Text("Diffuse:");
                                        float aspect = (float)tex->width / (float)tex->height;
                                        ImVec2 size(128, 128 / aspect);
                                        ImGui::Image((void*)(intptr_t)tex->id, size, ImVec2(0, 1), ImVec2(1, 0));
                                    }
                                    {
                                        auto tex = material->specular;
                                        ImGui::Text("Specular:");
                                        float aspect = (float)tex->width / (float)tex->height;
                                        ImVec2 size(128, 128 / aspect);
                                        ImGui::Image((void*)(intptr_t)tex->id, size, ImVec2(0, 1), ImVec2(1, 0));
                                    }
                                    {
                                        auto tex = material->normal;
                                        ImGui::Text("Normal:");
                                        float aspect = (float)tex->width / (float)tex->height;
                                        ImVec2 size(128, 128 / aspect);
                                        ImGui::Image((void*)(intptr_t)tex->id, size, ImVec2(0, 1), ImVec2(1, 0));
                                    }
                                }
                            }

							ImGui::TreePop();
						}

						ImGui::PopID();
					}

					ImGui::Unindent();
				}
			}

			ImGui::Separator();

			// Clear cache button
			if (ImGui::Button("Clear All Resources")) {
				rs->clear();
			}

			ImGui::SameLine();
			ImGui::TextDisabled("(!)");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Warning: This will unload all cached resources!");
			}
		}
		ImGui::End();
	}

	void DrawModuleViewer() {
        if (ImGui::Begin("Virtual File System")) {
            auto vfs = Engine::Application::Get().GetVFS();
            auto& module_map = vfs->GetMap();

            // Header stats
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "VFS Status");
            ImGui::Separator();

            ImGui::Text("Total Modules: %zu", module_map.size());
            ImGui::Text("Current Module: %s", Engine::VFS::GetCurrentModuleName().c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Module mappings table
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Module Mappings");

            if (ImGui::BeginTable("VFSTable", 4,
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY)) {

                // Setup columns
                ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Relative", ImGuiTableColumnFlags_WidthStretch, 80.0f);
				ImGui::TableSetupColumn("Resolved", ImGuiTableColumnFlags_WidthStretch, 200.0f);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 30.0f);
                ImGui::TableSetupScrollFreeze(0, 1); // Freeze header row
                ImGui::TableHeadersRow();

                // Display each module
                static std::string to_delete;
                for (const auto& [module, path] : module_map) {
                    ImGui::TableNextRow();

                    // Module name
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", module.c_str());

                    // Path
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", path.string().c_str());

                    // Copy button tooltip
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", path.string().c_str());
                    }
					
					// Resolved
					std::filesystem::path resolved_path = vfs->Resolve(module, "");
					ImGui::TableSetColumnIndex(2);
					ImGui::TextWrapped("%s", resolved_path.string().c_str());
                    
					// Actions
                    ImGui::TableSetColumnIndex(3);
                    ImGui::PushID(module.c_str());
					
					ImGui::SameLine();
                    // Copy path button
                    if (ImGui::SmallButton("Copy")) {
                        ImGui::SetClipboardText(resolved_path.string().c_str());
                    }

                    ImGui::SameLine();

                    // Delete button
                    if (ImGui::SmallButton("Delete")) {
                        to_delete = module;
                    }

                    ImGui::PopID();
                }

                // Handle deletion after iteration
                if (!to_delete.empty()) {
                    vfs->DeleteResourcePath(to_delete);
                    to_delete.clear();
                }

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Add new module section
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Add Module");

            static char module_name[256] = "";
            static char module_path[512] = "";

            ImGui::InputText("Module Name", module_name, IM_ARRAYSIZE(module_name));
            ImGui::InputText("Relative Path", module_path, IM_ARRAYSIZE(module_path));

            if (ImGui::Button("Add Module")) {
                if (strlen(module_name) > 0 && strlen(module_path) > 0) {
                    vfs->AddResourcePath(module_name, module_path);
                    module_name[0] = '\0';
                    module_path[0] = '\0';
                }
            }

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Path is relative to VFS root");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Path resolver utility
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.8f, 1.0f), "Path Resolver");

            static char test_module[256] = "";
            static char test_path[512] = "";
            static std::string resolved_path = "";

            ImGui::InputText("Test Module##resolver", test_module, IM_ARRAYSIZE(test_module));
            ImGui::InputText("Test Path##resolver", test_path, IM_ARRAYSIZE(test_path));

            if (ImGui::Button("Resolve")) {
                try {
                    auto result = vfs->GetResourcePath(test_module, test_path);
                    resolved_path = result.string();
                }
                catch (...) {
                    resolved_path = "[ERROR: Failed to resolve]";
                }
            }

            if (!resolved_path.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Resolved:");
                ImGui::TextWrapped("%s", resolved_path.c_str());

                if (ImGui::SmallButton("Copy Resolved Path")) {
                    ImGui::SetClipboardText(resolved_path.c_str());
                }
            }
        }
        ImGui::End();
	}

    // Helper to draw a single entity node recursively
    static void DrawEntityNode(entity_id entity, entity_id& selected_entity) {
        auto ecs = Application::Get().GetECS();

        if (!ecs->Exists(entity)) return;

        auto& hierarchy = ecs->GetComponent<Component::Hierarchy>(entity);

        // Build node label
        char label[64];
        if (ecs->HasComponent<Component::Name>(entity)) {
            Component::Name& name = ecs->GetComponent<Component::Name>(entity);
            snprintf(label, sizeof(label), "%s", name.name.c_str());
        }
        else snprintf(label, sizeof(label), "Entity %u", entity);

        // Check if this entity has children
        bool has_children = (hierarchy.first_child != null);

        // Tree node flags
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        if (!has_children) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        if (selected_entity == entity) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Draw the tree node
        bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", label);

        // Handle selection
        if (ImGui::IsItemClicked()) {
            selected_entity = entity;
        }

        // If node is open and has children, recursively draw them
        if (node_open && has_children) {
            entity_id child = hierarchy.first_child;
            while (child != null) {
                DrawEntityNode(child, selected_entity);
                child = ecs->GetComponent<Component::Hierarchy>(child).next_sibling;
            }
            ImGui::TreePop();
        }
    }

    // Helper to draw the inspector panel for selected entity
    static void DrawInspector(entity_id entity) {
        if (entity == null) {
            ImGui::TextDisabled("No entity selected");
            return;
        }

        auto ecs = Application::Get().GetECS();

        if (!ecs->Exists(entity)) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Entity no longer exists!");
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

        // Entity ID header
        ImGui::SeparatorText("Entity Info");
        ImGui::Text("ID: %u", entity);
        if (ecs->HasComponent<Component::Name>(entity)) {
            ImGui::SameLine();
            auto& nameComp = ecs->GetComponent<Component::Name>(entity);
            ImGui::Text("Name: %s", nameComp.name.c_str());
        }

        // Hierarchy Component
        if (ecs->HasComponent<Component::Hierarchy>(entity)) {
            auto& h = ecs->GetComponent<Component::Hierarchy>(entity);

            if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::Text("Depth: %u", h.depth);
                ImGui::Text("Parent: %s", h.parent == null ? "null" : std::to_string(h.parent).c_str());
                ImGui::Text("First Child: %s", h.first_child == null ? "null" : std::to_string(h.first_child).c_str());
                ImGui::Text("Next Sibling: %s", h.next_sibling == null ? "null" : std::to_string(h.next_sibling).c_str());
                ImGui::Text("Prev Sibling: %s", h.prev_sibling == null ? "null" : std::to_string(h.prev_sibling).c_str());
                ImGui::Unindent();
            }
        }

        // Transform Component
        if (ecs->HasComponent<Component::Transform>(entity)) {
            auto tref = ecs->GetTransformRef(entity);
            const auto& t = tref.GetTransform();

            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();

                // Position - column layout
                ImGui::Text("Position:");
                ImGui::Columns(3, "pos_columns", false);
                ImGui::Text("X: %.3f", t.position.x);
                ImGui::NextColumn();
                ImGui::Text("Y: %.3f", t.position.y);
                ImGui::NextColumn();
                ImGui::Text("Z: %.3f", t.position.z);
                ImGui::Columns(1);

                ImGui::Spacing();

                // Rotation (as quaternion) - column layout
                ImGui::Text("Rotation (Quaternion):");
                ImGui::Columns(4, "quat_columns", false);
                ImGui::Text("X: %.3f", t.rotation.x);
                ImGui::NextColumn();
                ImGui::Text("Y: %.3f", t.rotation.y);
                ImGui::NextColumn();
                ImGui::Text("Z: %.3f", t.rotation.z);
                ImGui::NextColumn();
                ImGui::Text("W: %.3f", t.rotation.w);
                ImGui::Columns(1);

                // Rotation (as Euler angles) - column layout
                vec3 euler = tref.GetRotationEuler();
                ImGui::Text("Rotation (Euler Degrees):");
                ImGui::Columns(3, "euler_columns", false);
                ImGui::Text("X: %.2f°", euler.x);
                ImGui::NextColumn();
                ImGui::Text("Y: %.2f°", euler.y);
                ImGui::NextColumn();
                ImGui::Text("Z: %.2f°", euler.z);
                ImGui::Columns(1);

                ImGui::Spacing();

                // Scale - column layout
                ImGui::Text("Scale:");
                ImGui::Columns(3, "scale_columns", false);
                ImGui::Text("X: %.3f", t.scale.x);
                ImGui::NextColumn();
                ImGui::Text("Y: %.3f", t.scale.y);
                ImGui::NextColumn();
                ImGui::Text("Z: %.3f", t.scale.z);
                ImGui::Columns(1);

                ImGui::Spacing();

                // Model Matrix (collapsed by default)
                if (ImGui::TreeNode("Model Matrix")) {
                    for (int row = 0; row < 4; row++) {
                        ImGui::Text("%.2f  %.2f  %.2f  %.2f",
                            t.modelMatrix[0][row],
                            t.modelMatrix[1][row],
                            t.modelMatrix[2][row],
                            t.modelMatrix[3][row]);
                    }
                    ImGui::TreePop();
                }

                ImGui::Unindent();
            }
        }

        if (ecs->HasComponent<Component::Drawable3D>(entity)) {
            auto& drawable = ecs->GetComponent<Component::Drawable3D>(entity);
            if (ImGui::CollapsingHeader("Drawable3D", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::Text("Filepath: %s", drawable.model->m_path.string().c_str());
                ImGui::Text("Collection: %u", drawable.collectionIndex);
                ImGui::Unindent();
            }
        }

        if (ecs->HasComponent<Component::Light>(entity)) {
            auto& light = ecs->GetComponent<Component::Light>(entity);
            if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::Text("Type: %s", light.type == Component::Light::Type::POINT ? "Point" : light.type == Component::Light::Type::SPOT ? "Spot" : "Directional");
                ImGui::Text("Color: %.2f %.2f %.2f", light.color.x, light.color.y, light.color.z);
                ImGui::Text("Range: %.2f", light.range);
                ImGui::Text("Intensity: %.2f", light.intensity);
                ImGui::Text("Direction: %.2f %.2f %.2f", light.direction.x, light.direction.y, light.direction.z);
            }
        }

        ImGui::PopStyleVar();
    }

    // Main function to call in your OnRender
    void DrawHierarchyViewer(const std::vector<entity_id>& updatedEntities) {
        (void)updatedEntities; // Could use this for caching, but simple approach for now

        static entity_id selected_entity = null;
        auto ecs = Application::Get().GetECS();

        // Single unified window with split layout
        if (ImGui::Begin("Scene Hierarchy & Inspector")) {

            // Create two columns: hierarchy tree on left, inspector on right
            ImGui::Columns(2, "hierarchy_inspector_columns", true);

            // Set initial column width (only on first run)
            static bool first_time = true;
            if (first_time) {
                ImGui::SetColumnWidth(0, 250);
                first_time = false;
            }

            // LEFT COLUMN - Hierarchy Tree
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Scene Graph");

            if (ImGui::Button("Deselect")) {
                selected_entity = null;
            }

            ImGui::BeginChild("HierarchyTree", ImVec2(0, 0), true);

            // Find all root entities (parent == null)
            std::vector<entity_id> roots;

            for (entity_id id = 0; id < 10000; ++id) { // Adjust max as needed
                if (ecs->Exists(id) && ecs->HasComponent<Component::Hierarchy>(id)) {
                    auto& h = ecs->GetComponent<Component::Hierarchy>(id);
                    if (h.parent == null) {
                        roots.push_back(id);
                    }
                }
            }

            // Draw all root nodes (they will recursively draw their children)
            if (roots.empty()) {
                ImGui::TextDisabled("No entities in scene");
            }
            else {
                for (entity_id root : roots) {
                    DrawEntityNode(root, selected_entity);
                }
            }

            ImGui::EndChild();

            // RIGHT COLUMN - Inspector
            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Inspector");

            ImGui::BeginChild("Inspector", ImVec2(0, 0), true);
            DrawInspector(selected_entity);
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void DrawPerf() {
        #ifdef _DEBUG
        if (ImGui::Begin("Performance metrics")) {
            for (auto& [name, s] : gProfiler.getSections()) {
                ImGui::Text("%s: avg %.2f | min %.2f | max %.2f | p99 %.2f | last %.2f ms",
                    name.c_str(), s.avg(), s.min(), s.max(), s.p99(), s.last);
            }
        }
        ImGui::End();
        #endif
    }

    void DrawLayerStack() {
        if (ImGui::Begin("Layer stack")) {
            Engine::LayerStack& layerStack = Engine::Application::Get().GetLayerStack();

            int index = 0;
            for (auto it = layerStack.begin(); it != layerStack.end(); ++it, ++index) {
                ILayer* layer = *it;
                if (!layer) continue;

                ImGui::PushID(index);

                // Display layer name with selectable style
                bool open = ImGui::TreeNodeEx(
                    layer->GetName().c_str(),
                    ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
                );

                if (open) {
                    // Could show additional details here later (profiling, etc.)
                    ImGui::Text("Address: %p", (void*)layer);
                    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
                    if (ImGui::SmallButton("Reload")) {
                        layer->OnReload();
                    }
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            if (layerStack.empty())
                ImGui::TextDisabled("No layers loaded.");
        }
        ImGui::End();
    }

    void DrawRenderer() {
        // drawCalls = 0;
        // instancedDrawCalls = 0;
        // totalObjects = 0;
        // batchCount = 0;
        // culledObjects = 0;
        Ref<Renderer> renderer = Engine::Application::Get().GetRenderer();

        if (ImGui::Begin("Renderer")) {
            if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding)) {
                Renderer::Stats avg{ 0 };
                for (const Renderer::Stats& s : renderer->GetStats()) {
                    avg.drawCalls += s.drawCalls;
                    avg.instancedDrawCalls += s.instancedDrawCalls;
                    avg.totalObjects += s.totalObjects;
                    avg.batchCount += s.batchCount;
                    avg.culledObjects += s.culledObjects;
                    avg.drawnObjects += s.drawnObjects;
                }
                avg.drawCalls /= renderer->GetStats().size();
                avg.instancedDrawCalls /= renderer->GetStats().size();
                avg.totalObjects /= renderer->GetStats().size();
                avg.batchCount /= renderer->GetStats().size();
                avg.culledObjects /= renderer->GetStats().size();
                avg.drawnObjects /= renderer->GetStats().size();

                ImGui::Text("Average over %d frames:", renderer->GetStats().size());
                ImGui::Text("> Draw Calls     : %d", avg.drawCalls);
                ImGui::Text("> Instanced Calls: %d", avg.instancedDrawCalls);
                ImGui::Text("> Total objects  : %d", avg.totalObjects);
                ImGui::Text("> Drawn objects  : %d", avg.drawnObjects);
                ImGui::Text("> Batch counts   : %d", avg.batchCount);
                ImGui::Text("> Culled objects : %d", avg.culledObjects);
            }
        }
        ImGui::End();
    }
	
    void DebugLayer::OnRender(const std::vector<entity_id>& updatedEntities) {
		(void)updatedEntities;
		Begin();

		DrawResourceViewer();
		DrawModuleViewer();
        DrawHierarchyViewer(updatedEntities);
        DrawPerf();
        DrawLayerStack();
        DrawRenderer();

		End();
	}

	void DebugLayer::Begin() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void DebugLayer::End() {
		ImGuiIO& io = ImGui::GetIO();
		// In a real app, you'd get this from your application/window class
		Engine::Application& app = Engine::Application::Get();
		io.DisplaySize = ImVec2(app.GetWindow().GetWidth(), app.GetWindow().GetHeight());

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

}