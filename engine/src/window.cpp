#include <engine/window.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Engine {

	static bool s_GLFWInitialized = false;

	std::unique_ptr<Window> Window::Create(const WindowProps& props) {
		return std::make_unique<Window>(props);
	}

	Window::Window(const WindowProps& props) {
		Init(props);
	}

	Window::~Window() {
		Shutdown();
	}

	void Window::Init(const WindowProps& props) {
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;

		Log::info("Creating window {} ({}, {})", props.Title, props.Width, props.Height);
		m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
		glfwMakeContextCurrent(m_Window);
		if (props.Fullscreen) glfwSetWindowMonitor(m_Window, glfwGetPrimaryMonitor(), 0, 0, m_Data.Width, m_Data.Height, GLFW_DONT_CARE);
	}

	void Window::Shutdown() {
		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	void Window::OnUpdate() {
		glfwPollEvents();
		glfwSwapBuffers(m_Window);
	}
}