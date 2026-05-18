# Style Reference

## CMake Style Guidelines

(c) Copyright 2015-2026 Zephyr Project members and individual contributors.

### General Formatting

* **Indentation**: Use 2 spaces for indentation. Avoid tabs to ensure consistency across different environments.
* **Line Length**: Limit line length to 80 characters where possible.
* **Empty Lines**: Use empty lines to separate logically distinct sections within a CMake file.
* **No Space Before Opening Brackets**: Do not add a space between a command and the opening parenthesis. Use `if(...)` instead of `if (...)`.

### Commands and Syntax

* **Lowercase Commands**: Always use lowercase CMake commands (e.g., `add_executable`, `find_package`). This improves readability and consistency.
* **One File Argument per Line**: Break the file arguments across multiple lines to make it easier to scan and identify each source file or item.

### Variable Naming

* **Use Uppercase for Cache Variables or variables shared across CMake files**: When defining cache variables using `option` or `set(... CACHE ...)`, use UPPERCASE names.
* **Use Lowercase for Local Variables**: For local variables within CMake files, use *lowercase* or *snake_case*.
* **Consistent Prefixing**: Use consistent prefixes for variables to avoid name conflicts, especially in large projects.

### Quoting

* **Quote Strings and Variables**: Always quote string literals and variables to prevent unintended behavior, especially when dealing with paths or arguments that may contain spaces.
* **Do Not Quote Boolean Values**: For boolean values (`ON`, `OFF`, `TRUE`, `FALSE`), avoid quoting them.

### Avoid Hardcoding Paths

* Use CMake variables (`CMAKE_SOURCE_DIR`, `CMAKE_BINARY_DIR`, `CMAKE_CURRENT_SOURCE_DIR`) instead of hardcoding paths.

### Conditional Logic

* Use `if`, `elseif`, and `else`  with proper indentation, and close with `endif()`.

### Documentation

* Use comments to document complex logic in the CMake files.
