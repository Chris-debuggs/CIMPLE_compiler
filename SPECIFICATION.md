# Cimple Language — Final Core Specification (Python Syntax, Native Compiled)

## 1. Language Identity
Cimple is a statically compiled, high-performance systems programming language that uses Python syntax and compiles directly to native machine code via LLVM, with optional GPU acceleration designed to outperform Mojo.

- Input: `.cimp` source files
- Output: native binaries (no interpreter)

## 2. File Structure
Source file:
```
program.cimp
```

Compiled outputs — examples:

**Windows**
```
program.exe
program.obj
program.dll (optional)
program.lib (optional)
```

**Linux**
```
program          (ELF binary)
program.o
program.so (optional)
program.appimage (optional)
```

**MacOS (later)**
```
program
program.o
program.dylib
```

## 3. Syntax Model — Pure Python Syntax
Cimple uses Python-compatible syntax. Example:
```cimple
def add(a, b):
    return a + b

x = add(10, 20)
print(x)
```

No syntax changes required; the compiler treats this as source for a statically compiled pipeline.

## 4. Key Differences From Python
While the surface syntax is Python, Cimple is:
- statically compiled
- type-inferred at compile time
- aggressively optimized
- produces native machine code

Example:
```cimple
x = 10
```
The compiler infers:
```
x : int32
```
Zero dynamic dispatch unless explicitly requested.

## 5. Type System
### 5.1 Type Inference (Primary)
Local variables and expressions are inferred at compile time:
```cimple
x = 10        # int32
y = 3.14      # float64
name = "abc"  # string
```

### 5.2 Optional Explicit Types
Type annotations are supported but optional:
```cimple
x: int32 = 10
y: float64 = 5.5
```

### 5.3 No Dynamic Rebinding Across Types
Reassigning a different type to the same name is a compile-time error:
```cimple
x = 10
x = "hello"   # compile error
```

## 6. Compilation Pipeline
```
.cimp source
   ↓
Lexer
   ↓
Parser
   ↓
AST
   ↓
Semantic analysis
   ↓
Type inference
   ↓
LLVM IR generation
   ↓
Optimization passes (aggressive)
   ↓
Object file generation
   ↓
Native linking
   ↓
Final binary
```
LLVM handles CPU optimization, vectorization, register allocation, and instruction scheduling.

## 7. Memory Model — Hybrid (Stack First, Heap Optional)
Default allocation is stack-first for local values and small objects. Heap allocation is used only when required (large objects, dynamic structures, explicit allocation).

Automatic scope cleanup (RAII-like) ensures deterministic destruction without GC pauses.

Example:
```cimple
def test():
    x = 10
# x destroyed automatically after leaving scope
```

Manual control is available via `del x` for immediate destruction.

## 8. Functions
Standard Python syntax compiles to native functions and supports inlining, vectorization, and optimizations such as tail-call elimination where applicable.

## 9. Modules
Multiple files are supported, compiled to object files and linked. `import` follows Python syntax but is resolved at compile/link time.

## 10. Performance Model
Cimple aims to match or exceed C/C++ performance via:
- LLVM aggressive optimization
- zero interpreter/runtime overhead
- stack-first memory layout
- zero-cost abstractions

## 11. Native Interoperability
C ABI compatibility is provided for calling C and system APIs:
```cimple
extern def printf(fmt: string, ...): int
```

## 12. GPU Programming (Core Feature)
GPU kernels are marked explicitly:
```cimple
@gpu
def add_kernel(a, b, out, i):
    out[i] = a[i] + b[i]
```
Compiler can generate CUDA/PTX or SPIR-V and provide `gpu_launch(...)`-style APIs to compile and execute kernels natively.

## 13. Optimization Strategy
Default is aggressive optimization: inlining, loop unrolling, SIMD vectorization, constant folding, DCE, and memory optimizations.

## 14. Compiler Architecture
Modular components: lexer, parser, type checker, optimizer, LLVM backend, linker, GPU backend.

## 15. No Runtime Interpreter
Cimple follows a compile-only model: `script -> compiler -> native binary`.

## 16. Execution Model
User command example:
```
cimple build program.cimp
```
Produces a native executable.

## 17. Security Advantages
Static compilation reduces runtime injection risks and enables stronger memory-safety tooling.

## 18. What Makes Cimple Unique
Python syntax usability + native compilation + LLVM optimizations + native GPU support + stack-first memory model + modular compiler + zero-overhead abstractions.

## 19. Example Full Program
```python
def main():
    x = 10
    y = 20
    print(x + y)
main()
```

## 20. Technical Positioning
Cimple combines Python syntax ergonomics with C performance, Rust-level control, LLVM backend power, and native GPU support.
