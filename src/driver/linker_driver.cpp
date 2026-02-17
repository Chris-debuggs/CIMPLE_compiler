// linker_driver.cpp - Linker driver implementation
#include "driver/linker_driver.h"
#include <cstdlib>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace cimple {
namespace driver {

LinkerDriver::LinkerDriver()
    : dead_code_elimination_(true) {
}

void LinkerDriver::add_object_file(const std::string& obj_file) {
    object_files_.push_back(obj_file);
}

void LinkerDriver::add_library(const std::string& lib_name) {
    libraries_.push_back(lib_name);
}

void LinkerDriver::set_output(const std::string& output_name) {
    output_name_ = output_name;
}

void LinkerDriver::enable_dead_code_elimination(bool enable) {
    dead_code_elimination_ = enable;
}

bool LinkerDriver::link() {
    if (object_files_.empty()) {
        std::cerr << "[linker] No object files to link\n";
        return false;
    }

    if (output_name_.empty()) {
        std::cerr << "[linker] No output name specified\n";
        return false;
    }

    std::vector<std::string> args;

#ifdef _WIN32
    // Windows: use link.exe or lld-link
    args.push_back("link.exe");
    
    // Output file
    args.push_back("/OUT:" + output_name_);
    
    // Dead code elimination
    if (dead_code_elimination_) {
        args.push_back("/OPT:REF"); // Remove unreferenced functions/data
        args.push_back("/OPT:ICF"); // Identical COMDAT folding
    }
    
    // Object files
    for (const auto& obj : object_files_) {
        args.push_back(obj);
    }
    
    // Libraries
    for (const auto& lib : libraries_) {
        args.push_back(lib + ".lib");
    }
    
    // Entry point (default to main)
    args.push_back("/ENTRY:main");
    
    // Subsystem (console application)
    args.push_back("/SUBSYSTEM:CONSOLE");
#else
    // Unix: use ld or clang/gcc as linker
    // Try to use clang++ or g++ as linker (they handle everything)
    const char* linker = nullptr;
    if (system("which clang++ > /dev/null 2>&1") == 0) {
        linker = "clang++";
    } else if (system("which g++ > /dev/null 2>&1") == 0) {
        linker = "g++";
    } else {
        linker = "ld";
    }
    
    args.push_back(linker);
    
    // Output file
    args.push_back("-o");
    args.push_back(output_name_);
    
    // Dead code elimination
    if (dead_code_elimination_) {
        args.push_back("-Wl,--gc-sections"); // Remove unused sections
        args.push_back("-Wl,--as-needed");     // Only link needed libraries
    }
    
    // Object files
    for (const auto& obj : object_files_) {
        args.push_back(obj);
    }
    
    // Libraries
    for (const auto& lib : libraries_) {
        args.push_back("-l" + lib);
    }
    
    // C++ standard library (if using clang++/g++)
    if (std::string(linker) != "ld") {
        args.push_back("-lc");
        args.push_back("-lm");
    }
#endif

    return execute_linker(args);
}

bool LinkerDriver::execute_linker(const std::vector<std::string>& args) {
    if (args.empty()) return false;

    std::cout << "[linker] Linking: ";
    for (const auto& arg : args) {
        std::cout << arg << " ";
    }
    std::cout << "\n";

#ifdef _WIN32
    // Build command line string
    std::string cmd_line;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd_line += " ";
        // Quote arguments with spaces
        if (args[i].find(' ') != std::string::npos) {
            cmd_line += "\"" + args[i] + "\"";
        } else {
            cmd_line += args[i];
        }
    }
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES;
    
    int result = system(cmd_line.c_str());
    return (result == 0);
#else
    // Build argv array
    std::vector<const char*> argv;
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    
    // Fork and execute
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(argv[0], const_cast<char* const*>(argv.data()));
        exit(1); // Should not reach here
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    } else {
        return false; // Fork failed
    }
#endif
}

} // namespace driver
} // namespace cimple
