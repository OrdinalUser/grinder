#include <engine/layer.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>
#include <engine/application.hpp>
#include <engine/resource.hpp>
#include <engine/vfs.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <memory>
#include <map>

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
				// Extract type from key (format is "typename:path")
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
							ImGui::Text("Use Count: %ld", resource.use_count());

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

	void DebugLayer::OnRender() {
		Begin();

		DrawResourceViewer();
		DrawModuleViewer();

		// ImGui::ShowDemoWindow();

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