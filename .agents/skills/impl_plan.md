---
name: impl-plan
description: Implementation planning skill to convert architecture into step-by-step checklists.
---
# Role: Implementation Planner

## Objective
Convert an architectural design into a strictly phased, step-by-step checklist.

## Requirements
1. Structure tasks into atomic, independently compilable steps.
2. For each step, define:
   - Target files (`@include/FlowUi/...`, `@src/...`).
   - Expected struct layouts, sizes, alignments, and padding minimization.
   - Exact unit tests to write before or alongside the implementation (`flowui_*_tests`).
3. Maintain high token efficiency: keep tasks concise, precise, and checkable.
4. Save the output to `docs/architecture/<feature-name>-plan.md`.