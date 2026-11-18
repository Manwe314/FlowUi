"""Compile complete storage providers and intended private-contract failures."""

import argparse
import json
from pathlib import Path
import subprocess


PREAMBLE = r'''
#include "FlowUi/Private/storageSystem/StorageSystemValidation.hpp"
#include "RecordingStorageSystem.hpp"
using namespace FlowUi;
using namespace FlowUi::detail::storage;
using Complete = FlowUi::test::RecordingStorageSystem;
static_assert(StorageSystemConcept<FlowStorageSystem>);
static_assert(validateStorageSystem<FlowStorageSystem>());
static_assert(StorageSystemConcept<Complete>);
static_assert(validateStorageSystem<Complete>());
static_assert(!std::is_move_constructible_v<Complete>);
static_assert(!std::is_default_constructible_v<Complete>);

struct MissingOperation : Complete { using Complete::Complete; void collect() = delete; };
struct MissingOverload : Complete { using Complete::Complete; void trackUse(const FrameToken&, BufferHandle); };
struct MissingShortCall : Complete {
    using Complete::Complete;
    void releaseBlob(BlobHandle, SubmissionSerial);
};
struct MissingWriteDefault : Complete {
    using Complete::Complete;
    BufferWriteView beginBufferWrite(const FrameToken&, BufferHandle, uint64_t, uint64_t, BufferWriteMode);
};
struct WrongReturn : Complete { using Complete::Complete; bool collect(); };
struct MissingConst : Complete { using Complete::Complete; uint64_t capabilities() noexcept; };
struct ThrowingObserver : Complete { using Complete::Complete; uint64_t capabilities() const; };
struct ThrowingShutdown : Complete { using Complete::Complete; void shutdown(); };
struct AbsentRevision : Complete { using Complete::Complete; private: using Complete::contractRevision; };
struct WrongRevision : Complete { using Complete::Complete; static constexpr uint32_t contractRevision = 2u; };
struct RuntimeRevision : Complete { using Complete::Complete; inline static uint32_t contractRevision = 1u; };
struct WrongRevisionType : Complete { using Complete::Complete; static constexpr uint64_t contractRevision = 1u; };
struct WrongVersion : Complete { using Complete::Complete; static constexpr uint32_t interfaceVersionValue = 7u; };
struct WrongVersionType : Complete { using Complete::Complete; static constexpr uint64_t interfaceVersionValue = 8u; };
struct RuntimeCapabilities : Complete { using Complete::Complete; inline static uint64_t supportedCapabilities = flowStorageSupportedCapabilities; };
struct WrongCapabilityType : Complete { using Complete::Complete; static constexpr uint32_t supportedCapabilities = 0xffffffffu; };
struct MissingCapabilities : Complete { using Complete::Complete; static constexpr uint64_t supportedCapabilities = 0u; };
struct WrongConstructor : Complete { explicit WrongConstructor(int); };
struct Polymorphic : Complete { using Complete::Complete; virtual ~Polymorphic() noexcept = default; };
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    arguments = parser.parse_args()
    evidence = arguments.source / ".flowui/agentic/verification/concepts-phase2/concepts"
    evidence.mkdir(parents=True, exist_ok=True)
    includes = [f"-I{arguments.build / 'include'}", f"-I{arguments.source / 'include'}",
                f"-I{arguments.source / 'external'}",
                f"-I{arguments.source / 'tests/StorageTests/RecordingProvider'}"]
    records = []
    fixtures = ["MissingOperation", "MissingOverload", "MissingShortCall", "MissingWriteDefault",
                "WrongReturn", "MissingConst", "ThrowingObserver", "ThrowingShutdown", "AbsentRevision",
                "WrongRevision", "RuntimeRevision", "WrongRevisionType", "WrongVersion", "WrongVersionType",
                "RuntimeCapabilities", "WrongCapabilityType", "MissingCapabilities", "WrongConstructor"]
    cases = [("complete", "static_assert(validateStorageSystem<Complete>());", True, [])]
    cases.extend((f"complete-dev-{level}", "static_assert(validateStorageSystem<Complete>());", True,
                  ["-DFLOW_UI_DEV_MODE=1", f"-DFLOWUI_DEV_MEMORY_LEVEL={level}"]) for level in (0, 1, 2, 4))
    cases.extend((fixture, f'static_assert(StorageSystemConcept<{fixture}>, "INTENDED_STORAGE_REJECTION");', False, [])
                 for fixture in fixtures)
    cases.append(("polymorphic", "static_assert(validateStorageSystem<Polymorphic>());", False, []))
    for name, assertion, should_pass, definitions in cases:
        path = evidence / f"{name}.cpp"
        path.write_text(PREAMBLE + "\n" + assertion + "\n")
        command = [arguments.compiler, "-std=c++23", "-fsyntax-only", *includes, *definitions, str(path)]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (evidence / f"{name}.log").write_text(result.stdout)
        records.append({"fixture": name, "exit_code": result.returncode, "expected_success": should_pass})
        if should_pass and result.returncode != 0:
            raise AssertionError(f"Complete storage fixture failed: {name}\n{result.stdout[-6000:]}")
        expected = "concrete nonvirtual dispatch" if name == "polymorphic" else "INTENDED_STORAGE_REJECTION"
        if not should_pass and (result.returncode == 0 or expected not in result.stdout):
            raise AssertionError(f"Missing intended storage diagnostic: {name}\n{result.stdout[-6000:]}")
    (evidence / "results.json").write_text(json.dumps(records, indent=2) + "\n")
    print(f"Storage contract: {len(cases)} production/dev positive and intended negative compilation cases passed.")


if __name__ == "__main__":
    main()
