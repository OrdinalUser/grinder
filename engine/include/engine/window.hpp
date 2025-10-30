#include <engine/api.hpp>

#include <string>
#include <memory>

// struct GLFWwindow;
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Engine {
	struct WindowProps {
		std::string Title;
		unsigned int Width;
		unsigned int Height;
		bool Fullscreen;

		ENGINE_API WindowProps(const std::string& title = "Grinder Engine",
			unsigned int width = 1600, unsigned int height = 900,
			bool fullscreen = false
		)
			: Title(title), Width(width), Height(height), Fullscreen{ fullscreen } {
		}
	};

	class Window {
	public:
		ENGINE_API Window(const WindowProps& props);
		ENGINE_API ~Window();

		ENGINE_API void OnUpdate();

		ENGINE_API unsigned int GetWidth() const { return m_Data.Width; }
		ENGINE_API unsigned int GetHeight() const { return m_Data.Height; }

		// This is the key function for ImGui and other libraries
		ENGINE_API GLFWwindow* GetNativeWindow() const { return m_Window; }

		ENGINE_API static std::unique_ptr<Window> Create(const WindowProps& props = WindowProps());

		ENGINE_API void Resize(int width, int height);

		ENGINE_API float GetAspectRatio() const;

	private:
		ENGINE_API void Init(const WindowProps& props);
		ENGINE_API void Shutdown();

		GLFWwindow* m_Window;

		struct WindowData {
			std::string Title;
			unsigned int Width;
			unsigned int Height;
		};

		WindowData m_Data;
	};
}