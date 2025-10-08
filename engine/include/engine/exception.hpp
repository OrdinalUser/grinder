#pragma once

#include <engine/api.hpp>

#include <exception>
#include <string>
#include <sstream>
#include <memory>

// Forward declaration to make this thing work
namespace backward {
    class StackTrace;
};

namespace Engine {

    // A class to capture the stack trace at the point of its creation.
    class StackTrace {
    public:
        ENGINE_API StackTrace(int skip_frames = 0);
        ENGINE_API ~StackTrace();
        ENGINE_API std::string to_string() const;
    private:
        backward::StackTrace* stack_trace_;
    };

    // Custom exception that stores a stack trace captured at its creation.
    class Exception : public std::exception {
    public:
        // Constructor now takes a message and a pre-captured stack trace.
        ENGINE_API Exception(const std::string& message, int error_code);

        ENGINE_API const char* what() const noexcept override;

        ENGINE_API int error_code() const noexcept;

        // Log the exception with its full details, including the captured stack trace.
        ENGINE_API void log() const;

    private:
        std::string message_;
        StackTrace stack_trace_;
        int error_code_;
    };

} // namespace Engine

// MACROS to make throwing exceptions easier and ensure stack trace is captured.
// This is the key: Engine::StackTrace() is created AT THE THROW SITE.
#define ENGINE_EXCEPTION(msg) \
    Engine::Exception((msg), -1)

#define ENGINE_EXCEPTION_CODE(msg, code) \
    Engine::Exception((msg), (code))

// Convenience macros that include file and line info.
#define ENGINE_THROW(msg) \
    throw ENGINE_EXCEPTION(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " - " + (msg))

#define ENGINE_THROW_CODE(msg, code) \
    throw ENGINE_EXCEPTION_CODE(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " - " + (msg), (code))