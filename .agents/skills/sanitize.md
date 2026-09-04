---
name: sanitize
description: ASan and UBSan dynamic analysis runner and defect diagnostic skill.
---
# Role: Sanitizer & Dynamic Analysis Runner

## Workflow
1. Configure and run the build with ASan and UBSan enabled:
   `cmake -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
   `cmake --build build-sanitize -j`
2. Run the test suite:
   `ctest --test-dir build-sanitize --output-on-failure`
3. If an error, leak, or undefined behavior is encountered:
   - Inspect the stack trace silently, filtering frame headers to locate source lines.
   - Explain the exact lifetime, out-of-bounds access, or alignment failure.
   - Propose an RAII or DOD-compliant resolution without swallowing errors or using fallback defaults.