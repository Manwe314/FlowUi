"""Verify paired storage selection, frozen consumer settings, and template isolation."""

import argparse
import json
from pathlib import Path
import subprocess


BOUNDARY_SOURCE = r'''
#include "FlowUi.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
bool exercisePublicTemplates(FlowUi::ActionManager& actions, FlowUi::ThemeManager& themes) {
    auto action = actions.appActions().bind(FlowUi::AppAction("storage.header.boundary"), [] {});
    auto registered = themes.registerTheme<FlowUi::FlowUiTheme>("storage", FlowUi::FlowUiTheme::dark());
    auto theme = themes.getTheme<FlowUi::FlowUiTheme>("storage");
    auto active = themes.getActiveTheme<FlowUi::FlowUiTheme>();
    return bool(action) && bool(registered) && bool(theme) && bool(active);
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--cmake", required=True)
    arguments = parser.parse_args()
    arguments.source = arguments.source.resolve()
    arguments.build = arguments.build.resolve()
    if Path(arguments.compiler).is_file():
        arguments.compiler = str(Path(arguments.compiler).resolve())
    evidence = arguments.source / ".flowui/agentic/verification/concepts-phase2/selection"
    evidence.mkdir(parents=True, exist_ok=True)
    records = []

    def check(name, command, success=True, diagnostic=None):
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (evidence / f"{name}.log").write_text(result.stdout)
        records.append({"case": name, "exit_code": result.returncode, "expected_success": success})
        if (result.returncode == 0) != success or (diagnostic and diagnostic not in " ".join(result.stdout.split())):
            raise AssertionError(f"Unexpected storage selection result: {name}\n{result.stdout[-6000:]}")

    for field, value in (("HEADER", "Invalid.hpp"), ("TYPE", "::Invalid")):
        check(f"half-{field}", [arguments.cmake, "-S", str(arguments.source), "-B", str(evidence / f"half-{field}"),
                               "-G", "Ninja", f"-DCMAKE_CXX_COMPILER={arguments.compiler}",
                               f"-DFLOWUI_STORAGE_SYSTEM_{field}={value}"], False,
              "FLOWUI_STORAGE_SYSTEM_HEADER and FLOWUI_STORAGE_SYSTEM_TYPE")
    translation = evidence / "Selection.cpp"
    translation.write_text('#include "FlowUi/Public/BuildConfig.hpp"\n')
    includes = [f"-I{arguments.build / 'include'}", f"-I{arguments.source / 'include'}",
                f"-I{arguments.source / 'external'}"]
    for macro in ("FLOWUI_STORAGE_SYSTEM_HEADER", "FLOWUI_STORAGE_SYSTEM_TYPE",
                  "FLOWUI_CONFIG_CUSTOM_STORAGE_SYSTEM", "FLOWUI_CONFIG_STORAGE_SYSTEM_HEADER",
                  "FLOWUI_CONFIG_STORAGE_SYSTEM_TYPE"):
        check(macro, [arguments.compiler, "-std=c++23", "-fsyntax-only", *includes,
                      f"-D{macro}=Invalid", str(translation)], False, "Storage selection is fixed")
    frozen_include = evidence / "frozen-include"
    destination = frozen_include / "FlowUi/Public/BuildConfig.hpp"
    destination.parent.mkdir(parents=True, exist_ok=True)
    config = (arguments.build / "include/FlowUi/Public/BuildConfig.hpp").read_text()
    start = config.index("#define FLOWUI_CONFIG_CUSTOM_STORAGE_SYSTEM")
    end = config.index("#endif", start) + len("#endif")
    config = config[:start] + ('#define FLOWUI_CONFIG_CUSTOM_STORAGE_SYSTEM 1\n'
                              '#define FLOWUI_CONFIG_STORAGE_SYSTEM_HEADER "unavailable/private/Storage.hpp"\n'
                              '#define FLOWUI_CONFIG_STORAGE_SYSTEM_TYPE ::Unavailable::PrivateStorage\n') + config[end:]
    destination.write_text(config)
    translation.write_text(BOUNDARY_SOURCE)
    check("public-template-isolation", [arguments.compiler, "-std=c++23", "-fsyntax-only",
                                         f"-I{frozen_include}", *includes, str(translation)])
    (evidence / "results.json").write_text(json.dumps(records, indent=2) + "\n")
    print("Storage selection: paired-setting failures, five frozen override guards, and public action/theme templates passed.")


if __name__ == "__main__":
    main()
