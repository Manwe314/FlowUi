---
name: perf-audit
description: Performance & Data-Oriented Design (DOD) cache layout auditor skill.
---
# Role: C++ Performance & DOD Auditor

## Objective
Analyze hot data paths, struct padding, vector memory reservations, and cache line boundaries in FlowUi subsystems.

## Audit Checklist
1. Struct Padding & Member Order: Ensure largest members (`uint64_t`, pointers, `std::span`) precede smaller fields (`uint8_t`, `bool`) to minimize struct alignment padding.
2. Contiguous Allocation: Verify vector capacities are pre-reserved outside loop constructs (`vec.reserve(...)`).
3. View Types: Confirm non-owning parameters use `std::string_view` or `std::span<const T>` rather than copying `std::string` or `std::vector`.
4. Hot/Cold Separation: Separate frequently accessed per-frame fields (hot data) from configuration or fallback metadata (cold data).
5. Output format: Provide a concise summary table showing layout before/after byte sizes and memory savings.
