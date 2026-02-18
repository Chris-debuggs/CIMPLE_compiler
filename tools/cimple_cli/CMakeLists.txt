cmake_minimum_required(VERSION 3.15)
project(cimple_cli)

set(CIMPLE_CLI_SOURCES
    # CLI entry point and commands
    ${CMAKE_SOURCE_DIR}/tools/cimple_cli/main.cpp
    ${CMAKE_SOURCE_DIR}/tools/cimple_cli/cli_commands.cpp
    ${CMAKE_SOURCE_DIR}/tools/cimple_cli/lex_parse_driver.cpp

    # Lexer (use the optimized string_view version only - token.cpp is the old version)
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer/lexer.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer/token_stream.cpp

    # Parser
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/expression_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/statement_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/python_indent_handler.cpp

    # Semantic analysis
    ${CMAKE_SOURCE_DIR}/src/frontend/semantic/type_infer.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/semantic/type_checker.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/semantic/cimple_var.cpp

    # Tree-walk evaluator (enables `cimple run` without LLVM)
    ${CMAKE_SOURCE_DIR}/src/frontend/eval/evaluator.cpp

    # Driver / linker
    ${CMAKE_SOURCE_DIR}/src/driver/linker_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/driver/build_pipeline.cpp
)

add_executable(cimple ${CIMPLE_CLI_SOURCES})
target_include_directories(cimple PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(cimple PROPERTIES CXX_STANDARD 17)

# Optional LLVM backend - enable with: cmake .. -DCIMPLE_USE_LLVM=ON
option(CIMPLE_USE_LLVM "Enable LLVM backend for native code generation" OFF)
if(CIMPLE_USE_LLVM)
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    include_directories(${LLVM_INCLUDE_DIRS})
    add_definitions(${LLVM_DEFINITIONS})
    target_compile_definitions(cimple PRIVATE CIMPLE_USE_LLVM)
    llvm_map_components_to_libnames(llvm_libs
        core support irreader passes
        x86codegen x86asmparser x86desc x86info
    )
    target_sources(cimple PRIVATE
        ${CMAKE_SOURCE_DIR}/src/backend/llvm/llvm_codegen.cpp
        ${CMAKE_SOURCE_DIR}/src/backend/llvm/llvm_context.cpp
        ${CMAKE_SOURCE_DIR}/src/backend/llvm/llvm_module_builder.cpp
        ${CMAKE_SOURCE_DIR}/src/backend/llvm/llvm_pass_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/backend/llvm/llvm_type_mapper.cpp
    )
    target_link_libraries(cimple ${llvm_libs})
endif()
