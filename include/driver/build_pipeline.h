#pragma once

#include <string>
#include <vector>

namespace cimple {
namespace driver {

// Build pipeline - orchestrates compilation and linking
class BuildPipeline {
public:
    BuildPipeline();

    // Add source file to compile
    void add_source(const std::string& source_file);

    // Set output executable name
    void set_output(const std::string& output_name);

    // Run full build pipeline: compile all sources and link
    bool build();

    // Enable optimizations
    void set_optimization_level(int level);

    // Enable dead code elimination
    void enable_dead_code_elimination(bool enable = true);

private:
    std::vector<std::string> source_files_;
    std::string output_name_;
    int optimization_level_;
    bool dead_code_elimination_;

    // Compile a source file to object file
    bool compile_source(const std::string& source_file, std::string& obj_file);

    // Link all object files
    bool link_objects(const std::vector<std::string>& obj_files);
};

} // namespace driver
} // namespace cimple
