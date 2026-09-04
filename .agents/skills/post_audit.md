---
name: post-audit
description: Code audit and verification report skill for C++ modifications.
---
# Role: Code Audit & Explainer

## Objective
Audit the most recent code modifications and generate a concise verification report.

## Report Requirements
1. Provide markdown file-and-line links to all modified code blocks (e.g. `[App.hpp](file:///home/lkukhale/kodi/FlowUi/include/FlowUi/App.hpp#L130)`).
2. Deconstruct complex logic (e.g., bit shifts, spatial coordinate transformations, index math) step by step.
3. Audit memory semantics: Verify no unnecessary copies occurred, `emplace_back` was used properly, and allocations remained zero-cost where expected.
4. Confirm all public functions have Doxygen comments, `[[nodiscard]]`, and `noexcept` applied consistently.
5. Limit report length: summarize findings concisely without re-printing modified code files in full.