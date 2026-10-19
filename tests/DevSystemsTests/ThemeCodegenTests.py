"""Compile generated general/specific theme defaults and verify runtime resolution."""
import argparse
import json
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--generator", type=pathlib.Path, required=True)
parser.add_argument("--compiler", required=True)
parser.add_argument("--source", type=pathlib.Path, required=True)
parser.add_argument("--build", type=pathlib.Path, required=True)
arguments = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="theme-codegen-", dir=arguments.build) as temporary:
    root = pathlib.Path(temporary)
    header = root / "TestTheme.hpp"
    header.write_text("#pragma once\nnamespace theme_test { struct Theme { float radius = 1; float alpha = 1; }; }\n")
    entries = []
    # Specific records precede the wildcard in the input: emission must still layer it last.
    for variant, field, value in (("dark", "radius", "9.0f"), ("*", "radius", "5.0f"),
                                  ("*", "alpha", "0.5f")):
        entries.append({"targetKind": "Theme", "targetScope": "Definition", "themeType": "0x1234",
                        "themeVariant": variant, "fieldId": "0x1" if field == "radius" else "0x2",
                        "fieldPath": field, "ownerCppType": "theme_test::Theme",
                        "sourceHeader": str(header), "cppValue": value})
    manifest = root / "active.flowchanges"
    manifest.write_text(json.dumps({"manifestVersion": 1, "schemaFingerprint": "0x0", "bakedChanges": entries}))
    subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--output-dir", str(root)], check=True)
    driver = root / "main.cpp"
    driver.write_text('''#include "TestTheme.hpp"
#include "FlowUiBakedChanges.hpp"
#include <cassert>
int main() {
    static_assert(FlowUi::baked::hasBakedThemeChanges(0x1234));
    theme_test::Theme dark, light, future;
    FlowUi::baked::applyBakedThemeErased(0x1234, "dark", &dark);
    FlowUi::baked::applyBakedThemeErased(0x1234, "light", &light);
    FlowUi::baked::applyBakedThemeErased(0x1234, "brand-new-variant", &future);
    assert(dark.radius == 9 && dark.alpha == .5f);
    assert(light.radius == 5 && light.alpha == .5f);
    assert(future.radius == 5 && future.alpha == .5f);
}
''')
    executable = root / "check"
    subprocess.run([arguments.compiler, "-std=c++23", "-I" + str(root),
                    "-I" + str(arguments.build / "include"), "-I" + str(arguments.source / "include"),
                    "-I" + str(arguments.source / "external"), str(root / "FlowUiBakedChanges.cpp"),
                    str(driver), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("General and specific baked themes resolve correctly, including future variants")
