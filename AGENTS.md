# AGENTS.md

## Scope & Operational Boundaries
- Make the smallest coherent change that implements the requirement.
- Do not reformat untouched code, change unrelated APIs, or upgrade dependencies.
- Never claim a build or test passed without executing the command and verifying an exit code of 0.

## C++ Standards & Semantics
- Dialect: Modern C++20/C++23 with CMake 3.25+ (Ninja generator).
- Naming: Full descriptive camelCase names (`spatialIndex`, `workerThreadCount`). No single-letter variables (`i`, `j` permitted only in trivial loop counters).
- Ownership: Sole ownership via `std::unique_ptr`. Non-owning views via `std::span` or `std::string_view`. No raw `new`/`delete`.
- Layout: Enforce Data-Oriented Design (DOD). Keep hot data contiguous; place largest struct members first to minimize padding.
- Memory: Contiguous containers (`std::vector`, `std::array`). Use `emplace_back` and pre-reserve outside loops.
- Signatures: Mark all non-void queries and observers with `[[nodiscard]]` and `noexcept` (where applicable). Document public APIs using Doxygen comments (`/** ... */`).
- Error Model: Use explicit result wrappers (`FlowUi::Result<T>`, `FlowUi::Status`) for recoverable operations; do not throw exceptions across C/C++ API boundaries (e.g. Clay / GLFW).

## Token-Efficient Navigation & Inspection Protocols
- Headers First: Inspect header declarations (`include/**/*.hpp`) before reading heavy implementation files (`src/**/*.cpp`).
- Precise Line Ranges: Always specify `StartLine` and `EndLine` when reading files larger than 100 lines to preserve context tokens.
- Focused Grep Searches: Use `grep_search` with target path globs (`Includes: ["*.hpp"]`) to locate symbol declarations efficiently.
- Truncate Diagnostic Logs: Extract only failing assertion lines, source paths, and line numbers from build/test logs instead of parsing entire compile logs.

## Implementation Integrity & Completeness
- Strict Prohibition on Placeholders: Never leave functions stubbed with `(void)param;`, `// TODO`, `assert(false)`, `std::unimplemented`, or empty bodies unless explicitly commanded to create a mock/interface.
- Complete Logic: If a variable, parameter, or dependency is wired in, it must be actively utilized in the functional logic. 
- No Premature Completion: Do not report a task as complete if the core transformation, calculation, or data movement has not been implemented.
- If Scope is Too Large: If a full implementation exceeds reasonable single-turn output, implement the full logic for the first logical component and explicitly state what remains, rather than creating empty stubs for the rest.

## Validation & Compilation Commands
- CMake Setup: `cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- Build narrow target: `cmake --build build --target <target_name> -j`
- Run focused test: `ctest --test-dir build -R <test_filter> --output-on-failure`
- Format changed file: `clang-format -i <file_path>`

## Document Generation & Path Linking Standards
- Output Location: Place authored Markdown documentation, design docs, and implementation plans in the repository tree (`./` or `./docs/architecture/`). Reserve `docs/` for Markdown documentation and generated Doxygen output. Store all agentic screenshots, captures, logs, coverage data, verification matrices, and verification reports (including Markdown) under `.flowui/agentic/`; use `.flowui/agentic/verification/<suite>/` for harness exports. Never place runtime or verification artifacts in `docs/`. Never write documents to `.gemini/`, hidden scratchpads, or `brain/` folders unless specifically commanded.
- Link Formatting: 
  - Strictly use repo-relative Markdown links: `[Target](path/to/file.cpp)` or `[Header](include/module/header.hpp)`.
  - With line numbers: `[Target](path/to/file.cpp#L42)`.
  - Absolute URI schemes (`file://`, `file:///`) and machine-specific paths (`/home/...`, `C:\...`) are strictly prohibited in documentation and chat responses.

## Multi-Package / Nested Guidance
- If a subdirectory contains its own AGENTS.md, its local directory rules supersede this file.