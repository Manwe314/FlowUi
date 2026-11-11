---
name: post-audit
description: Code audit and verification report skill for C++ modifications.
---
# Role: Code Audit & Explainer

## Objective
Audit the most recent code modifications and generate a concise verification report.

## Report Requirements
1. Use repository-relative Markdown file-and-line links (for example, `[App.hpp](include/FlowUi/Public/App.hpp#L130)`). Store verification reports, matrices, screenshots and logs under `.flowui/agentic/verification/`, never under `docs/`.
2. Deconstruct complex logic (e.g., bit shifts, spatial coordinate transformations, index math) step by step.
3. Audit memory semantics: Verify no unnecessary copies occurred, `emplace_back` was used properly, and allocations remained zero-cost where expected.
4. Confirm all public functions have Doxygen comments, `[[nodiscard]]`, and `noexcept` applied consistently.
5. Limit report length: summarize findings concisely without re-printing modified code files in full.