#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cimple {
namespace semantic {

// Scoped lexical environment with function-boundary isolation.
//
// Lookup behavior:
// - At top-level, names resolve through all active scopes (nearest first).
// - Inside a function scope, names resolve in the current function chain first.
//   If not found there, only the global scope is consulted (not caller frames).
template <typename T> class ScopeStack {
public:
  enum class ScopeKind { Block, Function };

  ScopeStack() {
    frames_.push_back(
        Frame{/*values=*/{}, /*function_boundary=*/true}); // global frame
  }

  void push_scope(ScopeKind kind = ScopeKind::Block) {
    frames_.push_back(
        Frame{/*values=*/{}, /*function_boundary=*/kind == ScopeKind::Function});
  }

  void pop_scope() {
    if (frames_.size() > 1) {
      frames_.pop_back();
    }
  }

  void set_local(const std::string &name, const T &value) {
    frames_.back().values[name] = value;
  }

  void set_local(const std::string &name, T &&value) {
    frames_.back().values[name] = std::move(value);
  }

  void set_global(const std::string &name, const T &value) {
    frames_.front().values[name] = value;
  }

  const T *lookup(const std::string &name) const {
    const std::size_t floor = current_function_floor_index();
    for (std::size_t i = frames_.size(); i-- > floor;) {
      auto it = frames_[i].values.find(name);
      if (it != frames_[i].values.end()) {
        return &it->second;
      }
    }

    if (floor > 0) {
      auto it = frames_.front().values.find(name);
      if (it != frames_.front().values.end()) {
        return &it->second;
      }
    }

    return nullptr;
  }

  T *lookup_mut(const std::string &name) {
    const std::size_t floor = current_function_floor_index();
    for (std::size_t i = frames_.size(); i-- > floor;) {
      auto it = frames_[i].values.find(name);
      if (it != frames_[i].values.end()) {
        return &it->second;
      }
    }

    if (floor > 0) {
      auto it = frames_.front().values.find(name);
      if (it != frames_.front().values.end()) {
        return &it->second;
      }
    }

    return nullptr;
  }

  const T *lookup_current(const std::string &name) const {
    auto it = frames_.back().values.find(name);
    if (it == frames_.back().values.end()) {
      return nullptr;
    }
    return &it->second;
  }

  T *lookup_current_mut(const std::string &name) {
    auto it = frames_.back().values.find(name);
    if (it == frames_.back().values.end()) {
      return nullptr;
    }
    return &it->second;
  }

  bool in_function_scope() const { return current_function_floor_index() > 0; }

  const std::unordered_map<std::string, T> &global_values() const {
    return frames_.front().values;
  }

private:
  struct Frame {
    std::unordered_map<std::string, T> values;
    bool function_boundary = false;
  };

  std::vector<Frame> frames_;

  std::size_t current_function_floor_index() const {
    for (std::size_t i = frames_.size(); i > 0; --i) {
      if (frames_[i - 1].function_boundary) {
        return i - 1;
      }
    }
    return 0;
  }
};

} // namespace semantic
} // namespace cimple
