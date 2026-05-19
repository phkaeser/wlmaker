# CMake Style Guidelines

This document outlines the style conventions used in the CMake configuration files for this project.

## 1. Command and Function Casing
* **Lowercase commands:** All built-in CMake commands and functions are written in lowercase (e.g., `cmake_minimum_required`, `set`, `add_executable`, `target_link_libraries`, `install`).

## 2. Indentation, Control Flow, and Line Wrapping
* **Indentation:** 2-space indentation is used for commands inside control blocks (like `if` / `endif`).
* **Control Flow:** Closing commands (e.g., `endif()`, `endforeach()`, `endmacro()`, `endfunction()`) must not repeat the condition or arguments of the opening command (e.g., use `endif()` instead of `endif(config_DEBUG)`).
* **Argument Wrapping:**
  * Short, simple commands are kept on a single line (e.g., `set(CMAKE_C_STANDARD 11)`).
  * Long commands or those with multiple arguments/keywords are wrapped, with each argument placed on a new line and indented by 2 spaces relative to the command (e.g., `add_executable`, `target_include_directories`).
  * The closing parenthesis `)` is either placed on the same line as the last argument or on its own line aligned with the command name (usually in block-style commands like `install` or `set_target_properties`).

## 3. Naming Conventions
* **Local and private variables:** Written in uppercase in legacy project code (e.g., `PUBLIC_HEADER_FILES`, `SOURCES`, `DOXYGEN_IN`), but official guidance recommends using **lowercase** or **snake_case** (e.g., `public_header_files`, `sources`) to distinguish them from CMake's built-in variables.
* **Options/Cache variables:** Formatted using either a lowercase/mixed-case prefix (e.g., `config_DEBUG`, `config_WERROR`) or full uppercase (e.g., `IWYU_MODE`).

## 4. Quoting
* **Variable expansion and paths:** Double quotes are used for file paths, generator expressions, and strings containing variable expansion (e.g., `"${CMAKE_CURRENT_BINARY_DIR}/analyzer.c"`, `"${CURSES_INCLUDE_DIRS}"`).
* **Identifiers and keywords:** Unquoted for target names, libraries, command keywords, and constant literals (e.g., `libbase`, `STATIC`, `PUBLIC`, `FATAL_ERROR`).

## 5. File Header and Comments
* **Headers:** Every CMake file begins with a standardized Apache 2.0 license and copyright header.
* **Comments:** Standard `#` comments are placed above the relevant block or line. Inline comments are generally lowercase-first or capitalized, providing short descriptions.

## 6. Target-Based Configuration
* **Prefer target-based configuration:** Define compile options, compile definitions, include directories, and link libraries on specific targets using `target_*` commands (e.g., `target_include_directories`, `target_compile_options`, `target_compile_definitions`, `target_link_libraries`) rather than modifying global/directory-wide settings (e.g., `add_compile_options`).
