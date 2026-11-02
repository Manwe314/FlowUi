"""Compile generated general/specific theme defaults and verify runtime resolution."""
import argparse
import copy
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
arguments.generator = arguments.generator.resolve()
arguments.source = arguments.source.resolve()
arguments.build = arguments.build.resolve()
with tempfile.TemporaryDirectory(prefix="theme-codegen-", dir=arguments.build) as temporary:
    root = pathlib.Path(temporary)
    header = root / "TestTheme.hpp"
    header.write_text('#pragma once\n#include "FlowUi/PublicStructs.hpp"\n'
                      'namespace theme_test { struct Theme { float radius = 1; float alpha = 1; FlowUi::TextureRef texture{}; }; }\n')
    entries = []
    # Specific records precede the wildcard in the input: emission must still layer it last.
    for variant, field, value in (("dark", "radius", "9.0f"), ("*", "radius", "5.0f"),
                                  ("*", "alpha", "0.5f")):
        entries.append({"targetKind": "Theme", "targetScope": "Definition", "themeType": "0x1234",
                        "themeVariant": variant, "fieldId": "0x1" if field == "radius" else "0x2",
                        "fieldPath": field, "ownerCppType": "theme_test::Theme",
                        "sourceHeader": str(header), "cppValue": value})
    resource_key = 'icons/save"button;#key'
    texture_initializer = ('FlowUi::TextureRef::fromStable(FlowUi::ResourceDomain::Icon,' +
                           json.dumps(resource_key) + ',static_cast<FlowUi::TextureFitMode>(0),'
                           'static_cast<FlowUi::TextureSamplingMode>(0),true)')
    texture_entry = copy.deepcopy(entries[-1])
    texture_entry.update(fieldId="0x3", fieldPath="texture", cppValue=texture_initializer,
                         valueType="TextureRef", valueTypeId="0x45", valueJson=json.dumps({"key": resource_key}))
    entries.append(texture_entry)
    manifest = root / "active.flowchanges"
    manifest.write_text(json.dumps({"manifestVersion": 1, "schemaFingerprint": "0x0", "bakedChanges": entries}))
    subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--output-dir", str(root)], check=True)
    # A global schema change must preserve valid assignments rather than erase saved work.
    schema_header = root / "schema.hpp"
    schema_header.write_text("#define FLOWUI_DEV_SCHEMA_FINGERPRINT 0x1234ull\n")
    stale = subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--output-dir", str(root),
                            "--schema-header", str(schema_header)], capture_output=True, text=True, check=True)
    assert "preserving 4 saved entries" in stale.stderr, stale.stderr
    digest = 14695981039346656037
    for character in manifest.read_bytes():
        digest = ((digest ^ character) * 1099511628211) & ((1 << 64) - 1)
    driver = root / "main.cpp"
    driver.write_text('''#include "TestTheme.hpp"
#include "FlowUiBakedChanges.hpp"
#include "devSystems/devTooling/bake/DevBakeValidation.hpp"
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
    assert(dark.texture.sourceKey == "icons/save\\\"button;#key");
    assert(dark.texture.tintEnabled);
#if FLOW_UI_DEV_MODE
    assert(FlowUi::baked::compiled_manifest_digest() == EXPECTED_DIGEST);
    const auto entries = FlowUi::baked::compiled_change_entries();
    assert(entries.size() == 4);
    assert(entries.back().field_path == "texture");
    assert(entries.back().value_type == 0x45 && entries.back().theme);
    assert(entries.back().theme_variant == "*");
    assert(entries.back().cpp_value.find("fromStable") != std::string_view::npos);
    assert(FlowUi::baked::compiled_memory_capacity_manifest_json().empty());
#endif
    using FlowUi::devSystems::tooling::generator_cpp_expression_valid;
    assert(generator_cpp_expression_valid("FlowUi::TextureRef::fromStable(FlowUi::ResourceDomain::Icon,\\\"key\\\")"));
    assert(!generator_cpp_expression_valid("value; injected()"));
    assert(!generator_cpp_expression_valid("Thing{.value=2"));
    assert(!generator_cpp_expression_valid("Thing{.value=\\\"unterminated}"));
}
''')
    executable = root / "check"
    subprocess.run([arguments.compiler, "-std=c++23", "-DEXPECTED_DIGEST=" + str(digest) + "ull", "-I" + str(root),
                    "-I" + str(arguments.build / "include"), "-I" + str(arguments.source / "include"),
                    "-I" + str(arguments.source / "external"), str(root / "FlowUiBakedChanges.cpp"),
                    str(driver), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    # A production build keeps applying defaults without the development catalogue.
    subprocess.run([arguments.compiler, "-std=c++23", "-DFLOW_UI_DEV_MODE=0", "-I" + str(root),
                    "-I" + str(arguments.build / "include"), "-I" + str(arguments.source / "include"),
                    "-I" + str(arguments.source / "external"), str(root / "FlowUiBakedChanges.cpp"),
                    str(driver), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

    # Diagnose unsafe metadata with a concrete entry identity, while inspection/removal still works.
    original = json.loads(manifest.read_text())
    invalid = copy.deepcopy(original)
    invalid["bakedChanges"][1]["cppValue"] = "0; malicious()"
    invalid["customMetadata"] = {"preserve": "older tooling should retain unknown fields"}
    invalid["manifestVersion"] = 2
    invalid["memoryCapacity"] = {"version": 1, "enabled": False, "targets": []}
    manifest.write_text(json.dumps(invalid))
    failed = subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--output-dir", str(root)],
                            capture_output=True, text=True)
    assert failed.returncode == 1 and "bakedChanges[1]" in failed.stderr and "radius" in failed.stderr, failed.stderr
    inspected = subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--inspect"],
                               capture_output=True, text=True, check=True)
    assert "[1] Theme" in inspected.stdout and "unsafe or malformed" in inspected.stdout
    # CLI repair participates in the runtime's same-path writer lock.
    lock = pathlib.Path(str(manifest) + ".lock")
    lock.mkdir()
    locked_contents = manifest.read_bytes()
    rejected = subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--remove-entry", "1"],
                              capture_output=True, text=True)
    assert rejected.returncode == 1 and "writer lock" in rejected.stderr and manifest.read_bytes() == locked_contents
    lock.rmdir()
    for removal_index in (1, 0, 1, 0):
        before = json.loads(manifest.read_text())
        expected = copy.deepcopy(before)
        del expected["bakedChanges"][removal_index]
        subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--remove-entry", str(removal_index)],
                       check=True, capture_output=True)
        assert json.loads(manifest.read_text()) == expected, "Repair changed unrelated saved data"
    subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--output-dir", str(root)], check=True)
    before_rejected_removal = manifest.read_bytes()
    rejected = subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--remove-entry", "0"],
                              capture_output=True, text=True)
    assert rejected.returncode == 1 and manifest.read_bytes() == before_rejected_removal

    duplicates = copy.deepcopy(original)
    duplicates["bakedChanges"].append(copy.deepcopy(duplicates["bakedChanges"][0]))
    manifest.write_text(json.dumps(duplicates))
    rejected = subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--output-dir", str(root)],
                              capture_output=True, text=True)
    assert rejected.returncode == 1 and "duplicate assignment destination" in rejected.stderr, rejected.stderr
    inspected = subprocess.run([str(arguments.generator), "--manifest", str(manifest), "--inspect"],
                               capture_output=True, text=True, check=True)
    assert "duplicate assignment destination" in inspected.stdout
print("General and specific baked themes resolve correctly, including future variants")
