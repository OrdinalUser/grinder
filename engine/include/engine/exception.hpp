#pragma once

#include <engine/log.hpp>

#undef min
#include <backward.hpp>
#include <exception>
#include <string>
#include <sstream>

namespace Engine {

    // A class to capture the stack trace at the point of its creation.
    class StackTrace {
    public:
        StackTrace(const StackTrace&) = delete;
        StackTrace& operator=(const StackTrace&) = delete;
        StackTrace(StackTrace&&) noexcept = default;
        StackTrace& operator=(StackTrace&&) noexcept = default;

        StackTrace(int skip_frames = 0) {
#ifdef _WIN32
            // On Windows, RtlCaptureContext is more reliable than the generic
            // approach inside a complex C++ throw.
            CONTEXT context;
            RtlCaptureContext(&context);

            // We give backward-cpp the full context AND a starting address (Rip).
            // This is much more robust and is similar to what the signal handler does.
            stack_trace_.load_from(reinterpret_cast<void*>(context.Rip), 8, &context);
#else
            // The POSIX/Linux implementation of load_here is generally more reliable.
            stack_trace_.load_here(8);
#endif
            // We now need to skip the frames for this constructor and the Exception constructor.
            stack_trace_.skip_n_firsts(skip_frames + 1);
        }

        std::string to_string() const {
            try {
                std::ostringstream oss;
                backward::Printer printer;
                printer.address = true;
                printer.snippet = true;
                printer.color_mode = backward::ColorMode::never;
                printer.print(stack_trace_, oss);
                return oss.str();
            }
            catch (...) {
                return "[Failed to format stack trace]";
            }
        }

        backward::StackTrace stack_trace_;
    };

    // Custom exception that stores a stack trace captured at its creation.
    class Exception : public std::exception {
    public:
        // Constructor now takes a message and a pre-captured stack trace.
        Exception(const std::string& message, int error_code)
            : message_(message), error_code_(error_code), stack_trace_(1) {
        }

        const char* what() const noexcept override {
            return message_.c_str();
        }

        int error_code() const noexcept {
            return error_code_;
        }

        // Log the exception with its full details, including the captured stack trace.
        void log() const {
            Engine::Log::error("Engine::Exception: {} ({})", message_, error_code_);
            std::string msg = stack_trace_.to_string();
            if (!msg.empty()) {
                Engine::Log::error("  Stack trace:\n{}", msg);
            }
        }

    private:
        std::string message_;
        StackTrace stack_trace_;
        int error_code_;
    };

} // namespace Engine

// MACROS to make throwing exceptions easier and ensure stack trace is captured.
// This is the key: Engine::StackTrace() is created AT THE THROW SITE.
#define ENGINE_EXCEPTION(msg) \
    Engine::Exception((msg), 0)

#define ENGINE_EXCEPTION_CODE(msg, code) \
    Engine::Exception((msg), (code))

// Convenience macros that include file and line info.
#define ENGINE_THROW(msg) \
    throw ENGINE_EXCEPTION(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " - " + (msg))

#define ENGINE_THROW_CODE(msg, code) \
    throw ENGINE_EXCEPTION_CODE(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " - " + (msg), (code))