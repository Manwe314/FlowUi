"""Exercise paired CPU-provider configuration, frozen macros and public include isolation."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


PUBLIC_SOURCE = r'''
#include "FlowUi.hpp"
#include "FlowUi/Public/Flow.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
bool publicTemplates(FlowUi::ActionManager& actions, FlowUi::ThemeManager& themes) {
    auto bound = actions.appActions().bind(FlowUi::AppAction("cpu.providers.header"), [] {});
    auto registered = themes.registerTheme<FlowUi::FlowUiTheme>("cpu.providers", FlowUi::FlowUiTheme::dark());
    auto theme = themes.getTheme<FlowUi::FlowUiTheme>("cpu.providers");
    return bool(bound) && bool(registered) && bool(theme);
}
'''

FONT_HEADER = r'''
#pragma once
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontMSDFProvider.hpp"
namespace ProviderChecks {
struct CapabilityFontProvider {
    static constexpr uint32_t contractRevision = FlowUi::detail::font::fontMsdfContractRevision;
    static constexpr bool supportsBaking = BAKING_VALUE;
    static constexpr bool supportsEncoding = ENCODING_VALUE;
    FlowUi::Result<FlowUi::detail::font::FontCpuAsset> loadBaked(std::span<const std::byte>) noexcept;
    FlowUi::Result<FlowUi::detail::font::FontBakeOutput> bake(const FlowUi::detail::font::FontBakeRequest&) noexcept;
    FlowUi::Result<std::vector<std::byte>> encodeBaked(const FlowUi::detail::font::FontCpuAsset&) noexcept;
};
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
    arguments.compiler = str(Path(arguments.compiler).resolve())
    evidence = arguments.source / ".flowui/agentic/verification/phase3-providers/selection"
    evidence.mkdir(parents=True, exist_ok=True)
    records = []

    def check(name, command, expected=True, diagnostic=None):
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (evidence / f"{name}.log").write_text(result.stdout)
        records.append({"case": name, "exit_code": result.returncode, "expected_success": expected})
        if (result.returncode == 0) != expected:
            raise AssertionError(f"Unexpected provider result {name}\n{result.stdout[-5000:]}")
        if diagnostic and diagnostic not in " ".join(result.stdout.split()):
            raise AssertionError(f"Missing intended provider diagnostic {name}\n{result.stdout[-5000:]}")

    configuration = (arguments.build / "include/FlowUi/Public/BuildConfig.hpp").read_text()
    includes = [f"-I{arguments.build / 'include'}", f"-I{arguments.source / 'include'}",
                f"-I{arguments.source / 'external'}"]
    translation = evidence / "Selection.cpp"
    translation.write_text('#include "FlowUi/Public/BuildConfig.hpp"\n')
    domains = ("TEXT_LAYOUT", "FONT_MSDF", "ICON_RASTER")
    with tempfile.TemporaryDirectory(prefix="flowui-phase3-provider-selection-") as configuration_root:
        for domain in domains:
            prefix = f"FLOWUI_{domain}_PROVIDER"
            base = [arguments.cmake, "-S", str(arguments.source), "-G", "Ninja",
                    f"-DCMAKE_CXX_COMPILER={arguments.compiler}", "-DFLOWUI_BUILD_TESTS=OFF",
                    "-DFLOWUI_BUILD_FONT_BAKER=OFF", "-DFLOW_UI_ENABLE_BAKED_CHANGES=OFF"]
            for field, value in (("HEADER", "Invalid.hpp"), ("TYPE", "::Invalid")):
                name = f"{domain}-half-{field}"
                check(name, [*base, "-B", str(Path(configuration_root) / name),
                             f"-D{prefix}_{field}={value}"], False,
                      f"{prefix}_HEADER and {prefix}_TYPE")
            for label, header, type_name in (("bad-header", 'bad"Header.hpp', "::Valid"),
                                            ("bad-type", "Valid.hpp", "::Bad<Type>")):
                name = f"{domain}-{label}"
                check(name, [*base, "-B", str(Path(configuration_root) / name),
                             f"-D{prefix}_HEADER={header}", f"-D{prefix}_TYPE={type_name}"],
                      False, f"FLOWUI_{domain}_PROVIDER requires a header spelling and a qualified C++ type name")
            for macro in (f"{prefix}_HEADER", f"{prefix}_TYPE",
                          f"FLOWUI_CONFIG_CUSTOM_{domain}_PROVIDER",
                          f"FLOWUI_CONFIG_{domain}_PROVIDER_HEADER",
                          f"FLOWUI_CONFIG_{domain}_PROVIDER_TYPE"):
                check(macro, [arguments.compiler, "-std=c++23", "-fsyntax-only", *includes,
                              f"-D{macro}=Invalid", str(translation)], False,
                      f"{domain} provider selection is fixed")
        check("icons-disabled-conflict", [arguments.cmake, "-S", str(arguments.source),
              "-B", str(Path(configuration_root) / "icons-disabled"), "-G", "Ninja",
              f"-DCMAKE_CXX_COMPILER={arguments.compiler}", "-DFLOWUI_INCLUDE_ICON_MANAGER=OFF",
              "-DFLOWUI_ICON_RASTER_PROVIDER_HEADER=Complete.hpp",
              "-DFLOWUI_ICON_RASTER_PROVIDER_TYPE=::Complete"], False,
              "A custom icon raster provider requires FLOWUI_INCLUDE_ICON_MANAGER=ON")

    def overlay(name, selected_domains):
        directory = evidence / name
        destination = directory / "FlowUi/Public/BuildConfig.hpp"
        destination.parent.mkdir(parents=True, exist_ok=True)
        contents = configuration
        for domain, header, type_name in selected_domains:
            pattern = (rf"#define FLOWUI_CONFIG_CUSTOM_{domain}_PROVIDER[^\n]*\n"
                       rf"#if FLOWUI_CONFIG_CUSTOM_{domain}_PROVIDER\n.*?#endif")
            replacement = (f"#define FLOWUI_CONFIG_CUSTOM_{domain}_PROVIDER 1\n"
                           f'#define FLOWUI_CONFIG_{domain}_PROVIDER_HEADER "{header}"\n'
                           f"#define FLOWUI_CONFIG_{domain}_PROVIDER_TYPE {type_name}")
            contents, count = re.subn(pattern, replacement, contents, count=1, flags=re.S)
            if count != 1:
                raise AssertionError(f"Could not locate frozen {domain} selection")
        destination.write_text(contents)
        return directory

    check("default-font-baking-availability-frozen", [arguments.compiler, "-std=c++23", "-fsyntax-only", *includes,
          "-DFLOWUI_CONFIG_DEFAULT_FONT_BAKING=1", str(translation)], False,
          "Default font baking availability is fixed")

    frozen = overlay("unavailable-provider-headers", [(domain, f"unavailable/private/{domain}.hpp", "::Unavailable::Provider") for domain in domains])
    translation.write_text(PUBLIC_SOURCE)
    check("ordinary-public-header-isolation", [arguments.compiler, "-std=c++23", "-fsyntax-only",
          f"-I{frozen}", *includes, str(translation)])

    check("ordinary-development-public-header-isolation", [arguments.compiler, "-std=c++23", "-fsyntax-only",
          "-DFLOW_UI_DEV_MODE=1", f"-I{frozen}", *includes, str(translation)])

    capability_directory = overlay("font-capabilities", [("FONT_MSDF", "CapabilityFontProvider.hpp", "::ProviderChecks::CapabilityFontProvider")])
    font_header = capability_directory / "CapabilityFontProvider.hpp"
    font_translation = evidence / "FontCapability.cpp"
    font_translation.write_text('#include "FlowUi/Private/resourceService/fontMSDFProvider/FontMSDFProviderSelection.hpp"\n')
    for name, baking, encoding, source, definitions, success, diagnostic in (
        ("runtime-disabled-custom-incapable", False, False, font_translation, [], True, None),
        ("runtime-enabled-custom-incapable", False, True, font_translation, ["-DFLOWUI_RUNTIME_FONT_BAKING=1"], False, "Runtime font baking requires a baking-capable FontMSDFProvider"),
        ("offline-tool-baking-mismatch", False, True, arguments.source / "tools/flowui_font_baker/main.cpp", [], False, "Offline font baker requires a baking-capable FontMSDFProvider"),
        ("offline-tool-encoding-mismatch", True, False, arguments.source / "tools/flowui_font_baker/main.cpp", [], False, "Offline font baker requires an encoding-capable FontMSDFProvider"),
        ("offline-tool-complete", True, True, arguments.source / "tools/flowui_font_baker/main.cpp", [], True, None),
    ):
        font_header.write_text(FONT_HEADER.replace("BAKING_VALUE", str(baking).lower()).replace("ENCODING_VALUE", str(encoding).lower()))
        check(name, [arguments.compiler, "-std=c++23", "-fsyntax-only", f"-I{capability_directory}", *includes,
                     *definitions, str(source)], success, diagnostic)
    (evidence / "results.json").write_text(json.dumps(records, indent=2) + "\n")
    print(f"CPU provider selections: {len(records)} intended configuration/compilation cases passed.")


if __name__ == "__main__":
    main()
