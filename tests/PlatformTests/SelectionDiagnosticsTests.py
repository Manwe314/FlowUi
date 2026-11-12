"""Check selection configuration failures and public translator header isolation."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def run_checked(command, expected_success=True, diagnostic=None):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != expected_success:
        raise AssertionError(f"Unexpected exit {result.returncode}: {' '.join(command)}\n{result.stdout[-8000:]}")
    if diagnostic and diagnostic not in " ".join(result.stdout.split()):
        raise AssertionError(f"Missing diagnostic {diagnostic!r}\n{result.stdout[-8000:]}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--cmake", required=True)
    arguments = parser.parse_args()
    evidence = arguments.source / ".flowui/agentic/verification/concepts-phase1/selection"
    evidence.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="flowui-selection-", dir=evidence) as temporary:
        workspace = Path(temporary)
        for pair in ("PLATFORM_PROVIDER", "ADDITIONAL_TRANSLATORS"):
            for field, value in (("HEADER", "Invalid.hpp"), ("TYPE", "::Invalid")):
                run_checked([
                    arguments.cmake, "-S", str(arguments.source), "-B", str(workspace / f"{pair}-{field}"),
                    "-G", "Ninja", f"-DCMAKE_CXX_COMPILER={arguments.compiler}",
                    f"-DFLOWUI_{pair}_{field}={value}",
                ], False, f"FLOWUI_{pair}_HEADER and FLOWUI_{pair}_TYPE")
        glfw_package = workspace / "raw-glfw-package"
        glfw_package.mkdir()
        (glfw_package / "glfw3Config.cmake").write_text(
            'set(glfw3_FOUND TRUE)\nset(GLFW3_LIBRARY "raw-library-without-target")\n')
        run_checked([
            arguments.cmake, "-S", str(arguments.source), "-B", str(workspace / "raw-glfw-build"),
            "-G", "Ninja", f"-DCMAKE_CXX_COMPILER={arguments.compiler}",
            "-DFLOWUI_BUILD_TESTS=OFF", "-DFLOWUI_BUILD_FONT_BAKER=OFF",
            "-DFLOW_UI_ENABLE_BAKED_CHANGES=OFF", "-DFLOWUI_INSTALL=OFF",
            "-DFLOWUI_GLFW_PROVIDER=system", f"-Dglfw3_DIR={glfw_package}",
        ], False, "glfw3 package must provide the glfw or glfw3 target")
        translation_unit = workspace / "Header.cpp"
        includes = [f"-I{arguments.build / 'include'}", f"-I{arguments.source / 'include'}"]
        headers = arguments.source / "include/FlowUi/Public/input"
        for header in sorted(headers.glob("*.hpp")):
            translation_unit.write_text(f'#include "FlowUi/Public/input/{header.name}"\nint main() {{ return 0; }}\n')
            run_checked([arguments.compiler, "-std=c++23", "-fsyntax-only", *includes, str(translation_unit)])
        translation_unit.write_text('#include "FlowUi/Public/BuildConfig.hpp"\n')
        for macro in ("FLOWUI_CONFIG_PLATFORM_PROVIDER_TYPE", "FLOWUI_CONFIG_ADDITIONAL_TRANSLATORS_TYPE"):
            run_checked([arguments.compiler, "-std=c++23", "-fsyntax-only", *includes,
                         f"-D{macro}=Invalid", str(translation_unit)], False, "selection is fixed when FlowUi is built")
        (workspace / "InvalidSelection.hpp").write_text("struct InvalidSelection {};\n")
        for pair, selector, diagnostic in (
            ("PLATFORM_PROVIDER", "platformProviderService/PlatformProviderSelection.hpp",
             "Selected platform provider must satisfy"),
            ("ADDITIONAL_TRANSLATORS", "inputService/TranslatorSelection.hpp",
             "must be a FlowUi::InputTranslatorPack"),
        ):
            configured = workspace / f"invalid-{pair}"
            configure_command = [
                arguments.cmake, "-S", str(arguments.source), "-B", str(configured), "-G", "Ninja",
                f"-DCMAKE_CXX_COMPILER={arguments.compiler}", "-DFLOWUI_BUILD_TESTS=OFF",
                "-DFLOWUI_BUILD_FONT_BAKER=OFF", "-DFLOW_UI_ENABLE_BAKED_CHANGES=OFF",
                "-DFLOWUI_INSTALL=OFF", f"-DFLOWUI_{pair}_HEADER=InvalidSelection.hpp",
                f"-DFLOWUI_{pair}_TYPE=::InvalidSelection",
            ]
            if pair == "PLATFORM_PROVIDER":
                configure_command.append("-DCMAKE_DISABLE_FIND_PACKAGE_glfw3=ON")
            run_checked(configure_command)
            translation_unit.write_text(f'#include "FlowUi/Private/{selector}"\n')
            run_checked([arguments.compiler, "-std=c++23", "-fsyntax-only", f"-I{configured / 'include'}",
                         f"-I{arguments.source / 'include'}", f"-I{arguments.source / 'external'}", f"-I{workspace}", str(translation_unit)],
                        False, diagnostic)
    print("Selection pairs, package target diagnostics, selected types, public headers, and overrides checked.")


if __name__ == "__main__":
    main()
