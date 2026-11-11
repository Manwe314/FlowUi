"""Check stable contract diagnostics and non-diagnostic concept rejection."""

import argparse
import concurrent.futures
import pathlib
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--generated-include", type=pathlib.Path, required=True)
    parser.add_argument("--output-dir", type=pathlib.Path, required=True)
    parser.add_argument("--standard", default="c++23")
    arguments = parser.parse_args()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    fixture = arguments.source_root / "tests/ElementResourceTests/CompileDiagnostics/ElementDiagnosticFixture.cpp"
    command = [arguments.compiler, f"-std={arguments.standard}", "-fsyntax-only",
               "-fdiagnostics-color=never", "-I", str(arguments.generated_include),
               "-I", str(arguments.source_root / "include"),
               "-I", str(arguments.source_root / "external"), str(fixture)]
    expected = {case_number: f"FE{case_number:03d}" for case_number in range(1, 27)}
    expected.update({27: "FE001", 28: "FE001", 29: "FE002", 30: "FE006",
                     31: "FE009", 32: "FE015", 33: "FE017", 34: "FE018",
                     35: "FE019", 36: "FE011", 37: "FE011", 38: "FE012",
                     39: "FE025", 40: "FE025", 41: ("FE011", "FE026"),
                     42: {"FE002", "FE006", "FE009", "FE015"}, 43: "FE001"})
    fragments = {
        "FE001": "definitionId must be a nonzero constexpr",
        "FE002": "Parameters must name an object payload",
        "FE003": "Parameters must be complete",
        "FE004": "State must be default constructible",
        "FE005": "Parameters must be default constructible",
        "FE006": "State must name an object payload",
        "FE007": "State must be complete",
        "FE008": "State destruction must be noexcept",
        "FE009": "Resources must name an object payload",
        "FE010": "Resources must be complete",
        "FE011": "buildElement must be callable as static void",
        "FE012": "constructElement must be callable as static Clay_ElementDeclaration",
        "FE013": "Resources must be default constructible or constructible",
        "FE014": "Resources destruction must be noexcept",
        "FE015": "Parts must name a class payload",
        "FE016": "Parts must be complete",
        "FE017": "statePolicy must be a constexpr value",
        "FE018": "debugName must be a constexpr value",
        "FE019": "isDevInternal must be a constexpr value",
        "FE020": "onHovered must be callable as static void",
        "FE021": "onPressed must be callable as static void",
        "FE022": "onHeld must be callable as static void",
        "FE023": "onReleased must be callable as static void",
        "FE024": "runLogic must be callable as static void",
        "FE025": "the element definition tag must be empty",
        "FE026": "provide static void buildElement",
    }

    def compile_case(case_number, query_only=False):
        case_command = command + [f"-DFLOWUI_DIAGNOSTIC_CASE={case_number}"]
        if query_only:
            case_command.append("-DFLOWUI_QUERY_ONLY=1")
        result = subprocess.run(case_command, capture_output=True, text=True, timeout=30)
        diagnostic_output = result.stdout + result.stderr
        label = f"case-{case_number:02d}" + ("-query" if query_only else "")
        (arguments.output_dir / f"{label}.log").write_text(diagnostic_output)
        if case_number == 0 or query_only:
            if result.returncode != 0:
                return f"{label}: expected successful compilation (exit {result.returncode})\n{diagnostic_output}"
        else:
            diagnostics = set(re.findall(r"FlowUi (FE\d{3}):", diagnostic_output))
            # Compilers differ in whether &Element::buildElement detects an
            # unsupported overload set; either root-cause message is valid.
            allowed = expected[case_number]
            if isinstance(allowed, tuple):
                matches = len(diagnostics) == 1 and diagnostics.issubset(allowed)
            else:
                allowed = {allowed} if isinstance(allowed, str) else allowed
                matches = diagnostics == allowed
            matches = matches and all(fragments[identifier] in diagnostic_output for identifier in diagnostics)
            if result.returncode == 0 or not matches:
                return (f"{label}: expected only {expected[case_number]}, received {sorted(diagnostics)} "
                        f"(exit {result.returncode})\n{diagnostic_output}")
        return None

    preflight_failure = compile_case(0)
    if preflight_failure:
        print(preflight_failure, file=sys.stderr)
        return 1
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        futures = []
        for case_number in expected:
            futures.extend([executor.submit(compile_case, case_number),
                            executor.submit(compile_case, case_number, True)])
        failures = [failure for future in futures if (failure := future.result())]
    for failure in failures:
        print(failure, file=sys.stderr)
    if failures:
        return 1
    print(f"Passed {len(expected)} expected failures, {len(expected)} query-only rejections, "
          f"and valid validation ({arguments.standard}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
