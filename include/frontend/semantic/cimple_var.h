#pragma once

#include <variant>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace cimple {
namespace semantic {

// CimpleVar - RAII-based tagged union for variable representation
// Uses std::variant for zero-cost type-safe memory management
// When reassigning, C++ destructor automatically cleans up old type
struct CimpleVar {
    // Tagged union using std::variant
    // Supports: integers, floats, strings, and vectors (for future collections)
    std::variant<
        std::int64_t,      // long integer
        double,            // float
        std::string,       // string
        std::vector<std::shared_ptr<CimpleVar>>  // vector of variables (for lists/arrays)
    > data;

    // Default constructor - uninitialized (holds int64_t(0))
    CimpleVar() : data(std::int64_t(0)) {}

    // Constructors for each type
    explicit CimpleVar(std::int64_t val) : data(val) {}
    explicit CimpleVar(double val) : data(val) {}
    explicit CimpleVar(const std::string& val) : data(val) {}
    explicit CimpleVar(std::string&& val) : data(std::move(val)) {}
    explicit CimpleVar(const std::vector<std::shared_ptr<CimpleVar>>& vec) : data(vec) {}
    explicit CimpleVar(std::vector<std::shared_ptr<CimpleVar>>&& vec) : data(std::move(vec)) {}

    // Copy constructor - std::variant handles deep copy automatically
    CimpleVar(const CimpleVar& other) = default;

    // Move constructor - std::variant handles move automatically
    CimpleVar(CimpleVar&& other) noexcept = default;

    // Copy assignment - old type destructor called automatically, then new type constructed
    CimpleVar& operator=(const CimpleVar& other) = default;

    // Move assignment - old type destructor called automatically, then new type moved
    CimpleVar& operator=(CimpleVar&& other) noexcept = default;

    // Destructor - std::variant automatically calls destructor of active type
    // This provides RAII semantics: cleanup happens immediately when variable goes out of scope
    ~CimpleVar() = default;

    // Type checking helpers
    bool is_int() const { return std::holds_alternative<std::int64_t>(data); }
    bool is_float() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_vector() const { return std::holds_alternative<std::vector<std::shared_ptr<CimpleVar>>>(data); }

    // Value accessors (with type checking)
    std::int64_t get_int() const {
        if (is_int()) return std::get<std::int64_t>(data);
        throw std::runtime_error("CimpleVar is not an integer");
    }

    double get_float() const {
        if (is_float()) return std::get<double>(data);
        if (is_int()) return static_cast<double>(std::get<std::int64_t>(data));
        throw std::runtime_error("CimpleVar is not a float");
    }

    const std::string& get_string() const {
        if (is_string()) return std::get<std::string>(data);
        throw std::runtime_error("CimpleVar is not a string");
    }

    const std::vector<std::shared_ptr<CimpleVar>>& get_vector() const {
        if (is_vector()) return std::get<std::vector<std::shared_ptr<CimpleVar>>>(data);
        throw std::runtime_error("CimpleVar is not a vector");
    }

    // String representation for debugging
    std::string to_string() const {
        if (is_int()) return std::to_string(std::get<std::int64_t>(data));
        if (is_float()) return std::to_string(std::get<double>(data));
        if (is_string()) return std::get<std::string>(data);
        if (is_vector()) return "[vector of " + std::to_string(std::get<std::vector<std::shared_ptr<CimpleVar>>>(data).size()) + " elements]";
        return "<unknown>";
    }
};

} // namespace semantic
} // namespace cimple
