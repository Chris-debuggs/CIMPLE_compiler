# Python PEG Grammar Integration Plan

## Overview

The Cimple parser should follow Python's official PEG grammar (`python.gram`) to ensure full Python syntax compatibility. This document outlines the plan for integrating Python's grammar rules.

## Current Status

The current parser (`src/frontend/parser/parser.cpp`) uses ad-hoc recursive descent parsing. It handles basic Python syntax but is not systematically derived from Python's official grammar.

## Python Grammar Source

Python's official grammar is defined in `python.gram` (PEG format) in the CPython repository:
- Location: https://github.com/python/cpython/blob/main/Grammar/python.gram
- Format: PEG (Parsing Expression Grammar)
- Rules: Each rule (e.g., `if_stmt`, `expr`, `atom`) defines valid syntax

## Implementation Strategy

### Option 1: Manual Translation (Current Approach)
- Manually translate each PEG rule from `python.gram` to a C++ parser function
- Pros: Full control, can optimize for Cimple's needs
- Cons: Time-consuming, must keep in sync with Python updates

### Option 2: Grammar Parser + Code Generator
- Parse `python.gram` file
- Generate C++ parser functions automatically
- Pros: Stays in sync with Python, less manual work
- Cons: Complex to implement, requires PEG parser

### Option 3: Hybrid Approach (Recommended)
- Start with manual translation of core rules
- Add grammar parser later for completeness
- Focus on commonly used Python constructs first

## Key Grammar Rules to Implement

From `python.gram`, these are the most important rules:

1. **file**: Top-level module structure
2. **stmt**: Statements (if, while, for, def, class, etc.)
3. **expr**: Expressions (operators, function calls, literals)
4. **atom**: Atomic expressions (identifiers, literals, parentheses)
5. **if_stmt**: If/elif/else statements
6. **while_stmt**: While loops
7. **for_stmt**: For loops
8. **funcdef**: Function definitions
9. **classdef**: Class definitions
10. **compound_stmt**: Compound statements (if, while, for, def, class, etc.)

## Current Parser Coverage

✅ Implemented:
- Basic expressions (binary ops, literals, function calls)
- Function definitions
- Return statements
- Assignments
- Variable references

❌ Missing:
- If/elif/else statements
- While/for loops
- Class definitions
- List/dict comprehensions
- Lambda expressions
- Decorators
- Import statements
- Exception handling (try/except/finally)
- With statements
- Match/case (Python 3.10+)

## Next Steps

1. **Phase 1**: Add missing statement types (if, while, for)
2. **Phase 2**: Add class definitions
3. **Phase 3**: Add comprehensions and lambda
4. **Phase 4**: Add exception handling
5. **Phase 5**: Full PEG grammar compliance

## Notes

- The parser should maintain Python's operator precedence
- Indentation handling (INDENT/DEDENT tokens) is already implemented
- Type checking happens after parsing, so grammar rules don't need type information
