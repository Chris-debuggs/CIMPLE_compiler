cmake_minimum_required(VERSION 3.15)
project(cimple_cli)

set(CIMPLE_CLI_SOURCES
    ${CMAKE_SOURCE_DIR}/tools/cimple_cli/main.cpp
    ${CMAKE_SOURCE_DIR}/tools/cimple_cli/cli_commands.cpp
    ${CMAKE_SOURCE_DIR}/tools/cimple_cli/lex_parse_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer/lexer.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer/token.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer/token_stream.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/expression_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/statement_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/python_indent_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/semantic/type_infer.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/eval/evaluator.cpp
)

add_executable(cimple ${CIMPLE_CLI_SOURCES})
target_include_directories(cimple PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(cimple PROPERTIES CXX_STANDARD 17)
