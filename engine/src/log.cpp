#include <engine/api.hpp>
#include <engine/exception.hpp>
#include <engine/log.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <csignal>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#undef min
#include <backward.hpp>

constexpr int STACK_TRACE_DEPTH = 10;

#ifdef _WIN32
// Windows exception handler that mimics backward's approach
static LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* info) {
    backward::StackTrace st;
    void* error_addr = nullptr;

#ifdef _M_X64
    error_addr = reinterpret_cast<void*>(info->ContextRecord->Rip);
#elif _M_IX86
    error_addr = reinterpret_cast<void*>(info->ContextRecord->Eip);
#elif _M_ARM64
    error_addr = reinterpret_cast<void*>(info->ContextRecord->Pc);
#endif

    if (error_addr) {
        // This mimics what backward does on POSIX:
        // load_from(error_addr, size, context, fault_addr)
        st.load_from(error_addr, STACK_TRACE_DEPTH,
            reinterpret_cast<void*>(info->ContextRecord),
            info->ExceptionRecord->ExceptionAddress);
    }
    else {
        st.load_here(STACK_TRACE_DEPTH,
            reinterpret_cast<void*>(info->ContextRecord),
            info->ExceptionRecord->ExceptionAddress);
    }
    // Cannot skip CxxThrowException and RaiseException since this also handles signals

    // Print to spdlog
    backward::Printer printer;
    printer.address = true;
    printer.snippet = true;
    printer.color_mode = backward::ColorMode::never;

    std::ostringstream oss;
    printer.print(st, oss);

    Engine::Log::error("Signal 0x{:X} at address 0x{:X}",
        info->ExceptionRecord->ExceptionCode,
        reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress));
    Engine::Log::error("Stack trace:\n{}", oss.str());
    
    #ifdef _DEBUG
    Engine::Log::info("Program paused before halt, press [Enter] to exit");
    (void)getchar();
    #endif // _DEBUG

    // Let system terminate
    return EXCEPTION_EXECUTE_HANDLER;
}

#else
// POSIX signal handler that mimics backward's approach
static void posix_signal_handler(int signo, siginfo_t* info, void* _ctx) {
    ucontext_t* uctx = static_cast<ucontext_t*>(_ctx);

    backward::StackTrace st;
    void* error_addr = nullptr;

#ifdef REG_RIP // x86_64
    error_addr = reinterpret_cast<void*>(uctx->uc_mcontext.gregs[REG_RIP]);
#elif defined(REG_EIP) // x86_32
    error_addr = reinterpret_cast<void*>(uctx->uc_mcontext.gregs[REG_EIP]);
#elif defined(__arm__)
    error_addr = reinterpret_cast<void*>(uctx->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
#if defined(__APPLE__)
    error_addr = reinterpret_cast<void*>(uctx->uc_mcontext->__ss.__pc);
#else
    error_addr = reinterpret_cast<void*>(uctx->uc_mcontext.pc);
#endif
#elif defined(__APPLE__) && defined(__x86_64__)
    error_addr = reinterpret_cast<void*>(uctx->uc_mcontext->__ss.__rip);
#elif defined(__APPLE__)
    error_addr = reinterpret_cast<void*>(uctx->uc_mcontext->__ss.__eip);
#endif

    if (error_addr) {
        st.load_from(error_addr, STACK_TRACE_DEPTH, reinterpret_cast<void*>(uctx), info->si_addr);
    }
    else {
        st.load_here(STACK_TRACE_DEPTH, reinterpret_cast<void*>(uctx), info->si_addr);
    }

    // Print to spdlog
    backward::Printer printer;
    printer.address = true;
    printer.snippet = true;
    printer.color_mode = backward::ColorMode::never;

    std::ostringstream oss;
    printer.print(st, oss);

    Engine::Log::error("Signal {} received", signo);
    Engine::Log::error("Stack trace:\n{}", oss.str());
    spdlog::shutdown();

    // Forward signal
    raise(signo);
    _exit(EXIT_FAILURE);
}
#endif

namespace backward {
    backward::SignalHandling sh;  // Setup signal handling globally
}

static std::string capture_current_stacktrace() {
    backward::StackTrace st;
    st.load_here(STACK_TRACE_DEPTH);
    st.skip_n_firsts(2);
    std::ostringstream oss;
    backward::Printer printer;
    printer.address = true;
    printer.snippet = true;
    printer.color_mode = backward::ColorMode::never;
    printer.print(st, oss);
    return oss.str();
}

#ifdef _WIN32
// RAII Initializer for DbgHelp and the Microsoft Symbol Server
struct DbgHelpInitializer {
    DbgHelpInitializer() {
        // Tell DbgHelp to find symbols from the MS server.
        // This is the magic that fixes resolving stack frames in system DLLs.
        char searchPath[4096];
        if (SymGetSearchPath(GetCurrentProcess(), searchPath, sizeof(searchPath))) {
            // Append our symbol path if a path already exists
            std::string newPath = std::string(searchPath) + ";srv*C:\\symbols*https://msdl.microsoft.com/download/symbols";
            SymSetSearchPath(GetCurrentProcess(), newPath.c_str());
        }
        else {
            SymSetSearchPath(GetCurrentProcess(), "srv*C:\\symbols*https://msdl.microsoft.com/download/symbols");
        }

        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
        if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
            // You might want to log this failure, but don't crash.
            spdlog::warn("SymInitialize failed with error: {}", GetLastError());
        }
    }

    ~DbgHelpInitializer() {
        SymCleanup(GetCurrentProcess());
    }
};
#endif

static void setup_signals() {
    #ifdef _WIN32
        SetUnhandledExceptionFilter(windows_exception_handler);
    #else
        struct sigaction sa;
        sa.sa_sigaction = posix_signal_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);

        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGABRT, &sa, nullptr);
        sigaction(SIGFPE, &sa, nullptr);
        sigaction(SIGILL, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
    #endif
}

static void setup_exceptions() {
    // Set up terminate handler for uncaught exceptions.
    std::set_terminate([]() {
        Engine::Log::critical("Terminate handler called.");
        std::exception_ptr eptr = std::current_exception();

        try {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
            else {
                Engine::Log::critical("Terminate called without an active exception.");
            }
        }
        catch (const Engine::Exception& e) {
            Engine::Log::critical("Unhandled Engine::Exception caught by terminate handler.");
            e.log();
        }
        catch (const std::exception& e) {
            Engine::Log::critical("Unhandled std::exception: {}", e.what());
            Engine::Log::error("Stack trace:\n{}", capture_current_stacktrace());
        }
        catch (...) {
            Engine::Log::critical("Unhandled unknown exception.");
            Engine::Log::error("Stack trace:\n{}", capture_current_stacktrace());
        }
        std::abort();
    });
}

static std::string generate_log_filename(const std::string& base_name) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << base_name << "_"
        << std::put_time(std::localtime(&time), "%Y-%m-%d_%H%M%S")
        << ".log";

    return oss.str();
}

namespace Engine::Log {
    static std::shared_ptr<spdlog::logger> g_logger;
    static std::once_flag g_init_flag;

    ENGINE_API std::shared_ptr<spdlog::logger> GetLogger() {
        return g_logger;
    }

    static void setup_sinks() {
#ifdef _DEBUG
        // Log to both file and console
        constexpr const char* debug_base_name = "debug";
        auto debug_filename = generate_log_filename(debug_base_name);

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(debug_filename);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        g_logger = std::make_shared<spdlog::logger>("engine", spdlog::sinks_init_list{ console_sink, file_sink });
        g_logger->set_pattern("[%H:%M:%S] [thread %t] %v");
        g_logger->set_level(spdlog::level::debug);
        g_logger->info("Debug logging initialized, outputting to console and {}", debug_filename);
        g_logger->flush_on(spdlog::level::err);
#else
        // Log only errors and to debug console, maybe change to also dump to file, we'll see
        g_logger = std::make_shared<spdlog::logger>("engine", std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        g_logger->set_pattern("[%H:%M:%S] [thread %t] %v");
        g_logger->set_level(spdlog::level::err);
        g_logger->flush_on(spdlog::level::err);
#endif

    }

    std::shared_ptr<spdlog::logger>& Get() {
        std::call_once(g_init_flag, setup_sinks);
        return g_logger;
    }

    ENGINE_API void setup_logging() {
        #ifdef _WIN32
        // This MUST be created before any stack traces are captured.
        static DbgHelpInitializer dbg_help_init;
        #endif

        setup_signals();
        setup_exceptions();
    }

    ENGINE_API void destroy_logging() {
        g_logger->flush();
        spdlog::shutdown();
    }

}