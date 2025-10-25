#include <engine/exception.hpp>
#include <engine/log.hpp>

#include <backward.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMIMAX
#include <windows.h>
#undef min
#undef max

namespace Engine {
    StackTrace::StackTrace(int skip_frames) {
        stack_trace_ = new backward::StackTrace;
        #ifdef _WIN32
        // On Windows, RtlCaptureContext is more reliable than the generic
        // approach inside a complex C++ throw.
        CONTEXT context;
        RtlCaptureContext(&context);
        
        // We give backward-cpp the full context AND a starting address (Rip).
        // This is much more robust and is similar to what the signal handler does.
        stack_trace_->load_from(reinterpret_cast<void*>(context.Rip), 8 + 1 + skip_frames, &context);
        #else
        // The POSIX/Linux implementation of load_here is generally more reliable.
        stack_trace_.load_here(8 + 1 + skip_frames);
        #endif
        // We now need to skip the frames for this constructor and the Exception constructor.
        stack_trace_->skip_n_firsts(skip_frames + 1);
    }

    StackTrace::~StackTrace() {
        delete stack_trace_;
    }
    
    std::string StackTrace::to_string() const {
        try {
            std::ostringstream oss;
            backward::Printer printer;
            printer.address = true;
            printer.snippet = true;
            printer.color_mode = backward::ColorMode::never;
            printer.print(*stack_trace_, oss);
            return oss.str();
        }
        catch (...) {
            return "[Failed to format stack trace]";
        }
    }

    Exception::Exception(const std::string& message, int error_code)
        : message_(message), error_code_(error_code), stack_trace_(1) {
    }

    const char* Exception::what() const noexcept {
        return message_.c_str();
    }

    int Exception::error_code() const noexcept {
        return error_code_;
    }

    // Log the exception with its full details, including the captured stack trace.
    void Exception::log() const {
        Engine::Log::error("Engine::Exception: {} ({})", message_, error_code_);
        std::string msg = stack_trace_.to_string();
        if (!msg.empty()) {
            Engine::Log::error(" --- Stack trace:\n{}", msg);
        }
    }
}