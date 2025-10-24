#include <engine/layer.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>
#include <engine/application.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace Engine {
	GuiLayer::GuiLayer() : ILayer("GuiLayer") {}

	GuiLayer::~GuiLayer() {}

	void GuiLayer::OnAttach() {
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
		// GLFWwindow* window = Application::Get().GetWindow().GetNativeWindow(); // <-- Get your window pointer
		Engine::Window& window = Engine::Application::Get().GetWindow();
		ImGui_ImplGlfw_InitForOpenGL(window.GetNativeWindow(), true);
		ImGui_ImplOpenGL3_Init("#version 460");
	}

	void GuiLayer::OnDetach() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void GuiLayer::OnRender() {
		Begin();
		// UI code goes here
		ImGui::ShowDemoWindow();
		End();
	}

	void GuiLayer::Begin() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void GuiLayer::End() {
		ImGuiIO& io = ImGui::GetIO();
		// In a real app, you'd get this from your application/window class
		io.DisplaySize = ImVec2(1280, 720);

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

}