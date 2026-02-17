#pragma once

#include <string>
#include <vector>

namespace cimple {
namespace driver {

// Linker driver - links object files into executables
class LinkerDriver {
public:
    LinkerDriver();

    // Add an object file to link
    void add_object_file(const std::string& obj_file);

    // Add a library to link against
    void add_library(const std::string& lib_name);

    // Set output executable name
    void set_output(const std::string& output_name);

    // Link all object files and libraries into executable
    // Returns true on success, false on failure
    bool link();

    // Enable dead code elimination (strip unused symbols)
    void enable_dead_code_elimination(bool enable = true);

private:
    std::vector<std::string> object_files_;
    std::vector<std::string> libraries_;
    std::string output_name_;
    bool dead_code_elimination_;

    // Execute linker command
    bool execute_linker(const std::vector<std::string>& args);
};

} // namespace driver
} // namespace cimple
