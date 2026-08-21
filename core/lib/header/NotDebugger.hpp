
#pragma once


#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
#include <windows.h>    
#endif

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace NovaEngine::Debugger
{   

     enum class DebugSeverity {
        Trace,      // Noamrlly
        Debug,      // Normal Information
        Info,       // Sysyem Information
        Warning,    // Waring
        Error,      // Internal Error
        Critical    // System crash
    };

   struct DebuggerEntry {
        // --- 1. Basic Metadata ---
        std::string id;                         // Unique entry ID (e.g., "DBG_1024")
        std::string tag;                        // Custom Tag (e.g., "PHYSICS_COLLISION")
        std::string category;                   // Engine Module (e.g., "Renderer", "Audio", "ECS")

        // --- 2. Payload & Values ---
        std::string message;                    // Main debug message
        std::string value;                      // Variable value (formatted as string)
        std::string type;                       // Data type (e.g., "int", "Vector3", "Entity*")
        const void* memory_address = nullptr;   // Memory pointer address (e.g., 0x7fff5fbff610)

        // --- 3. Source Code Location ---
        std::string file_name;                  // C++ File Name (__FILE__)
        std::string function_name;              // Function Name (__func__ / __FUNCTION__)
        int line_number = 0;                    // Line Number (__LINE__)

        // --- 4. Severity & Priorities ---
        DebugSeverity severity = DebugSeverity::Debug;
        int priority = 0;                       // Priority level (0 = Low, 10 = High)
        int error_code = 0;                     // Error or warning code number (e.g., 404, 1002)

        // --- 5. Diagnostics ---
        std::vector<std::string> stack_trace;   // Call stack / backtrace for crashes
    };

    class DebugLogger {
    private:
        // UI ke liye log history (Bina Mutex ke)
        inline static std::vector<DebuggerEntry> s_LogHistory;
        inline static bool s_ANSIEnabled = false;

        // ANSI Color Codes
        static constexpr const char* COLOR_RESET = "\033[0m";
        static constexpr const char* COLOR_TRACE = "\033[90m";      // Gray
        static constexpr const char* COLOR_DEBUG = "\033[36m";      // Cyan
        static constexpr const char* COLOR_INFO  = "\033[32m";      // Green
        static constexpr const char* COLOR_WARN  = "\033[33m";      // Yellow
        static constexpr const char* COLOR_ERROR = "\033[31m";      // Red
        static constexpr const char* COLOR_CRIT  = "\033[1;37;41m"; // White text on Red

        static void EnableWindowsANSI() {
            if (s_ANSIEnabled) return;
            #ifdef _WIN32
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD dwMode = 0;
                if (GetConsoleMode(hOut, &dwMode)) {
                    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    SetConsoleMode(hOut, dwMode);
                }
            }
            #endif
            s_ANSIEnabled = true;
        }

        static const char* GetSeverityColor(DebugSeverity severity) {
            switch (severity) {
                case DebugSeverity::Trace:    return COLOR_TRACE;
                case DebugSeverity::Debug:    return COLOR_DEBUG;
                case DebugSeverity::Info:     return COLOR_INFO;
                case DebugSeverity::Warning:  return COLOR_WARN;
                case DebugSeverity::Error:    return COLOR_ERROR;
                case DebugSeverity::Critical: return COLOR_CRIT;
                default:                      return COLOR_RESET;
            }
        }

        static const char* GetSeverityPrefix(DebugSeverity severity) {
            switch (severity) {
                case DebugSeverity::Trace:    return "[TRACE]";
                case DebugSeverity::Debug:    return "[DEBUG]";
                case DebugSeverity::Info:     return "[INFO]";
                case DebugSeverity::Warning:  return "[WARN]";
                case DebugSeverity::Error:    return "[ERROR]";
                case DebugSeverity::Critical: return "[CRITICAL]";
                default:                      return "[LOG]";
            }
        }

    public:
        // 1. Direct Log (Sirf Console par print hoga)
        static void LogDirect(const std::string& message, DebugSeverity severity = DebugSeverity::Info) {
            EnableWindowsANSI();
            const char* color = GetSeverityColor(severity);
            const char* prefix = GetSeverityPrefix(severity);

            std::cout << color << prefix << " " << message << COLOR_RESET << "\n";
        }

        // 2. Stored Log (UI ke liye Vector me save hoga + Console par print hoga)
        static void Log(const DebuggerEntry& entry) {
            EnableWindowsANSI();

            s_LogHistory.push_back(entry);

            const char* color = GetSeverityColor(entry.severity);
            const char* prefix = GetSeverityPrefix(entry.severity);

            std::cout << color << prefix << " ";
            if (!entry.category.empty() || !entry.tag.empty()) {
                std::cout << "[" << entry.category << "::" << entry.tag << "] ";
            }
            std::cout << entry.message << COLOR_RESET << "\n";

            if (!entry.value.empty()) {
                std::cout << "  ↳ Value: " << entry.value;
                if (!entry.type.empty()) std::cout << " (" << entry.type << ")";
                std::cout << "\n";
            }

            if (!entry.file_name.empty()) {
                std::cout << COLOR_TRACE << "  ↳ Location: " << entry.file_name << ":" << entry.line_number;
                if (!entry.function_name.empty()) std::cout << " in " << entry.function_name << "()";
                std::cout << COLOR_RESET << "\n";
            }
        }

        // 3. UI Functions
        static const std::vector<DebuggerEntry>& GetLogHistory() {
            return s_LogHistory;
        }

        static void ClearLogHistory() {
            s_LogHistory.clear();
        }
    };

   

    class NotDebugger
    {   
    private:
        NotDebugger() = default;
        ~NotDebugger() = default;

        // Singleton Copy Prevent (सुरक्षा के लिए)
        NotDebugger(const NotDebugger&) = delete;
        NotDebugger& operator=(const NotDebugger&) = delete;

        std::vector<DebuggerEntry> debuggerEntries;

    public:
        static NotDebugger& GetInstance() {
            static NotDebugger instance;
            return instance;
        }

        void CreateEntry(const DebuggerEntry& entry) {
            DebugLogger::Log(entry); // Console Par Print ke liye
        }

        void CreateEntry(const std::string& message, 
                         DebugSeverity severity = DebugSeverity::Info, 
                         const std::string& category = "", 
                         const std::string& tag = "") {
            DebuggerEntry entry;
            entry.message = message;
            entry.severity = severity;
            entry.category = category;
            entry.tag = tag;

            CreateEntry(entry);
        }

        static void LogDirect(const std::string& message, DebugSeverity severity = DebugSeverity::Info) {
            DebugLogger::LogDirect(message, severity);
        }

        const std::vector<DebuggerEntry>& GetEntries() const {
            return DebugLogger::GetLogHistory();
        }

        void ClearEntries() {
            debuggerEntries.clear();
            DebugLogger::ClearLogHistory();
        }
    };

    #define LOG_DIRECT(msg, severity) \
    NovaEngine::Debugger::NotDebugger::LogDirect(msg, severity)

    #define LOG_FULL(msg, _sev, _cat, _tag) \
        do { \
            NovaEngine::Debugger::DebuggerEntry _e; \
            _e.message = msg; \
            _e.severity = _sev; \
            _e.category = _cat; \
            _e.tag = _tag; \
            _e.file_name = __FILE__; \
            _e.function_name = __func__; \
            _e.line_number = __LINE__; \
            NovaEngine::Debugger::NotDebugger::GetInstance().CreateEntry(_e); \
        } while(0)

}