#pragma once

#include <engine/api.hpp>
#include <spdlog/spdlog.h>

namespace Engine::Log {
	ENGINE_API void setup_logging();
    ENGINE_API void destroy_logging();

    ENGINE_API std::shared_ptr<spdlog::logger>& get();
    ENGINE_API std::shared_ptr<spdlog::logger> GetLogger();

    // --- Logging convenience wrappers ---
    template <typename... Args>
    inline void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->trace(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->debug(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->warn(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->error(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        get()->critical(fmt, std::forward<Args>(args)...);
    }
}