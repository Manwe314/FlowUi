"""Compile and execute the standalone generator's memory interfaces with GCC/Clang."""
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

base = {"manifestVersion": 1, "schemaFingerprint": "0x0", "bakedChanges": []}
section = {
    "version": 1, "enabled": True, "growthFactor": 2.0, "allowRuntimeGrowth": False,
    "targets": [{"key": "flowui.tuning.input_fields.registry_entries", "value": "128",
                 "policy": "observed_maximum", "observedMaximum": "112", "headroom": "16"}],
    "evidence": {"kind": "recording", "complete": True, "sampleEveryTicks": 8},
}
with tempfile.TemporaryDirectory(prefix="memory-capacity-codegen-", dir=arguments.build) as temporary:
    root = pathlib.Path(temporary)
    for name, enabled in (("enabled", True), ("disabled", False), ("absent", False)):
        directory = root / name
        directory.mkdir()
        manifest = copy.deepcopy(base)
        if name != "absent":
            manifest["manifestVersion"] = 2
            manifest["memoryCapacity"] = copy.deepcopy(section)
            manifest["memoryCapacity"]["enabled"] = enabled
        manifest_path = directory / "active.flowchanges"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        generate = [str(arguments.generator), "--manifest", str(manifest_path), "--output-dir", str(directory)]
        subprocess.run(generate, check=True)
        source_path = directory / "FlowUiBakedChanges.cpp"
        first_mtime = source_path.stat().st_mtime_ns
        subprocess.run(generate, check=True)
        assert source_path.stat().st_mtime_ns == first_mtime, "Unchanged generated TU was rewritten"
        driver = directory / "main.cpp"
        driver.write_text('''#include "FlowUiBakedChanges.hpp"
#include <cassert>
#include <vector>
int main() {
    static_assert(FlowUi::baked::has_baked_memory_capacity() == ENABLED);
    const auto settings = FlowUi::baked::baked_memory_capacity_settings();
    assert(settings.size() == (ENABLED ? 1u : 0u));
    FlowUi::MemoryCapacityProfile profile;
    FlowUi::baked::apply_baked_memory_policy(profile);
    assert(profile.growthFactor == (ENABLED ? 2.f : 1.5f));
    assert(profile.allowRuntimeGrowth == !ENABLED);
    if constexpr (ENABLED) {
        assert(settings.front().target == FlowUi::makeMemoryCapacityTargetId("flowui.tuning.input_fields.registry_entries"));
        assert(settings.front().value == 128);
        std::vector<unsigned long long> values;
        values.reserve(settings.front().value);
        const auto capacity = values.capacity();
        for (unsigned long long index = 0; index < 112; ++index) values.emplace_back(index);
        assert(values.capacity() == capacity);
    }
}
''', encoding="utf-8")
        executable = directory / "check"
        subprocess.run([arguments.compiler, "-std=c++23", "-DENABLED=" + str(int(enabled)),
                        "-I" + str(directory), "-I" + str(arguments.build / "include"),
                        "-I" + str(arguments.source / "include"), "-I" + str(arguments.source / "external"),
                        str(source_path), str(driver), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    invalid_cases = []
    for value in ("-1", "1e2", "18446744073709551616", "4294967295"):
        invalid = copy.deepcopy(section)
        invalid["targets"][0]["value"] = value
        invalid_cases.append(invalid)
    for alteration in ("duplicate", "unknown", "synthetic", "unit", "boolean"):
        invalid = copy.deepcopy(section)
        if alteration == "duplicate":
            invalid["targets"].append(copy.deepcopy(invalid["targets"][0]))
        elif alteration == "unknown":
            invalid["targets"][0]["key"] = "flowui.tuning.elements"
        elif alteration == "synthetic":
            invalid["evidence"]["kind"] = "synthetic"
        elif alteration == "unit":
            invalid["targets"][0]["unit"] = "bytes"
        else:
            invalid["enabled"] = "true"
        invalid_cases.append(invalid)
    for index, invalid in enumerate(invalid_cases):
        manifest = copy.deepcopy(base)
        manifest.update(manifestVersion=2, memoryCapacity=invalid)
        manifest_path = root / ("invalid-" + str(index) + ".flowchanges")
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        rejected = subprocess.run([str(arguments.generator), "--manifest", str(manifest_path),
                                   "--output-dir", str(root / "rejected")], capture_output=True, text=True)
        assert rejected.returncode != 0 and "memoryCapacity" in rejected.stderr, rejected.stderr
print("Enabled, disabled, absent and invalid generated capacity interfaces passed.")

# The existing CMake path regenerates on a changed manifest and uses a stamp for unchanged output.
with tempfile.TemporaryDirectory(prefix="memory-capacity-cmake-", dir=arguments.build) as temporary:
    root = pathlib.Path(temporary)
    changes = root / ".flowui" / "changes"
    changes.mkdir(parents=True)
    manifest = copy.deepcopy(base)
    manifest.update(manifestVersion=2, memoryCapacity=copy.deepcopy(section))
    manifest_path = changes / "active.flowchanges"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    def cmake_path(path):
        return str(path.resolve()).replace("\\", "/").replace('"', '\\"')
    (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.25)
project(MemoryCapacityIncremental LANGUAGES CXX)
set(FLOW_UI_ENABLE_BAKED_CHANGES ON)
add_executable(flowui-dev-generate IMPORTED)
set_target_properties(flowui-dev-generate PROPERTIES IMPORTED_LOCATION "{cmake_path(arguments.generator)}")
include("{cmake_path(arguments.source / 'cmake' / 'FlowUiBaking.cmake')}")
add_library(capacity_probe STATIC probe.cpp)
target_compile_features(capacity_probe PRIVATE cxx_std_23)
target_include_directories(capacity_probe PRIVATE "{cmake_path(arguments.source / 'include')}" "{cmake_path(arguments.build / 'include')}" "{cmake_path(arguments.source / 'external')}")
flowui_configure_baking(capacity_probe)
add_executable(check main.cpp)
target_link_libraries(check PRIVATE capacity_probe)
''', encoding="utf-8")
    (root / "probe.cpp").write_text('''#include "FlowUiBakedChanges.hpp"
std::uint64_t capacity_value() { return FlowUi::baked::baked_memory_capacity_settings().front().value; }
''', encoding="utf-8")
    (root / "main.cpp").write_text('''#include <cstdint>
#include <iostream>
std::uint64_t capacity_value();
int main() { std::cout << capacity_value(); }
''', encoding="utf-8")
    build = root / "build"
    subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                    "-DCMAKE_CXX_COMPILER=" + arguments.compiler], check=True, capture_output=True, text=True)
    def build_fixture():
        return subprocess.run(["cmake", "--build", str(build), "--target", "check"],
                              check=True, capture_output=True, text=True).stdout
    build_fixture()
    assert subprocess.check_output([str(build / "check")], text=True) == "128"
    manifest["memoryCapacity"]["targets"][0]["value"] = "256"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    changed_build = build_fixture()
    assert "Generating FlowUi baked" in changed_build and "Building CXX object" in changed_build
    assert subprocess.check_output([str(build / "check")], text=True) == "256"
    generated = build / "flowui_generated" / "FlowUiBakedChanges.cpp"
    unchanged_timestamp = generated.stat().st_mtime_ns
    manifest["createdTimestamp"] = "metadata-only-change"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    unchanged_build = build_fixture()
    assert "Generating FlowUi baked" in unchanged_build and "Building CXX object" not in unchanged_build
    assert generated.stat().st_mtime_ns == unchanged_timestamp
    assert "Generating FlowUi baked" not in build_fixture()
print("Changed-manifest regeneration and unchanged-value incremental builds passed.")
