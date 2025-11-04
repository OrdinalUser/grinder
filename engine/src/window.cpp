#include <engine/window.hpp>
#include <engine/log.hpp>
#include <engine/exception.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Engine {
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
		
		// Create the window
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
		m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
		glfwMakeContextCurrent(m_Window);
		
		// Load OpenGL functions, once
		static bool glad_loaded = false;
		if (!glad_loaded) {
			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
				ENGINE_THROW("Failed to initialize GLAD");
			}
			glad_loaded = true;
			Log::info("OpenGL information:");
			Log::info("- version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
			Log::info("- renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
			Log::info("- vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
			Log::info("- glsl version: {}",reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
		}
		
		if (props.Fullscreen) glfwSetWindowMonitor(m_Window, glfwGetPrimaryMonitor(), 0, 0, m_Data.Width, m_Data.Height, GLFW_DONT_CARE);
		
		// Store 'this' pointer for callback retrieval
    	glfwSetWindowUserPointer(m_Window, this);
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
			auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (self) self->Resize(width, height);
		});
		glfwSwapInterval(0); // Turn on V-Sync
	}

	void Window::Shutdown() {
		glfwDestroyWindow(m_Window);
	}

	void Window::OnUpdate() {
		glfwPollEvents();
		glfwSwapBuffers(m_Window);
	}

	void Window::Resize(int width, int height) {
		m_Data.Width = width;
		m_Data.Height = height;
		glViewport(0, 0, width, height);
	}

	float Window::GetAspectRatio() const {
		return static_cast<float>(m_Data.Width) / static_cast<float>(m_Data.Height);
	}
}