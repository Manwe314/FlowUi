"""Compile valid and intentionally incompatible provider/translator fixtures."""

import argparse
from pathlib import Path
import subprocess
import tempfile


PREAMBLE = r'''
#include "FlowUi/Private/platformProviderService/PlatformProviderServiceConcept.hpp"
#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwPlatformProviderService.hpp"
#include "FlowUi/Public/input/InputTranslatorConcept.hpp"
using namespace FlowUi;
using namespace FlowUi::detail::platform;
using Default = GlfwPlatformProviderService;
static_assert(PlatformProviderServiceConcept<Default>);
static_assert(!std::is_move_constructible_v<Default::WindowState>);

struct AlternateRuntime {
    Status initialize(const PlatformConfig&);
    Status pollEvents();
    void shutdown() noexcept;
    Status validate() const noexcept;
    bool ready() const noexcept;
    Result<std::vector<const char*>> requiredInstanceExtensions() const;
};
static_assert(PlatformRuntimeConcept<AlternateRuntime>);
static_assert(!PlatformWindowConcept<GlfwWindow, AlternateRuntime>);

struct MissingPump : Default { Status pollEvents() = delete; };
struct MissingSurface : Default { Result<VkSurfaceKHR> createSurface(WindowState&, VkInstance) = delete; };
struct MissingExtensions : Default { Result<std::vector<const char*>> requiredInstanceExtensions() const = delete; };
struct MissingRuntime { using PlatformWindow = GlfwWindow; };
struct WrongRevision : Default { static constexpr auto contractRevision = platformContractRevision + 1; };
struct WrongResult : Default { bool pollEvents(); };
struct WrongConst : Default { bool isFocused(WindowState&) const noexcept; };
struct WrongNoexcept : Default { void quiesceWindow(WindowState&); };
struct WrongRuntimeConnection : Default { using PlatformRuntime = AlternateRuntime; };
struct WrongInputConnection : Default { using PlatformWindowInput = GlfwWindow; };
struct WrongBindingConnection : Default { using EventBinding = GlfwWindowInput; };
struct OnlyComponentAliases {
    using PlatformRuntime = GlfwRuntime;
    using PlatformWindow = GlfwWindow;
    using PlatformWindowInput = GlfwWindowInput;
    using EventBinding = GlfwEventBinding;
    using WindowState = GlfwWindowState;
};
struct ValidTranslator {
    Status initialize(const InputTranslatorContext&);
    Status poll(const InputTranslatorContext&, InputSink&);
    void resetTarget(WindowId, InputSink&) noexcept;
    void shutdown() noexcept;
};
static_assert(InputTranslatorConcept<ValidTranslator>);
struct TranslatorMissingPoll : ValidTranslator { Status poll(const InputTranslatorContext&, InputSink&) = delete; };
struct TranslatorWrongReset : ValidTranslator { void resetTarget(WindowId, InputSink&); };
'''


def compile_fixture(arguments, path, expression, should_pass):
    path.write_text(PREAMBLE + f'\nstatic_assert({expression}, "INTENDED_CONTRACT_REJECTION");\n')
    command = [arguments.compiler, "-std=c++23", "-fsyntax-only",
               f"-I{arguments.build / 'include'}", f"-I{arguments.source / 'include'}",
               f"-I{arguments.source / 'external'}", str(path)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if should_pass:
        if result.returncode != 0:
            raise AssertionError(f"Valid provider fixture failed:\n{result.stdout[-10000:]}")
    elif result.returncode == 0 or "INTENDED_CONTRACT_REJECTION" not in result.stdout:
        raise AssertionError(f"Expected concept diagnostic for {expression}:\n{result.stdout[-10000:]}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    arguments = parser.parse_args()
    evidence = arguments.source / ".flowui/agentic/verification/concepts-phase1/concepts"
    evidence.mkdir(parents=True, exist_ok=True)
    provider_failures = ("MissingPump", "MissingSurface", "MissingExtensions", "MissingRuntime", "WrongRevision",
                         "WrongResult", "WrongConst", "WrongNoexcept", "WrongRuntimeConnection", "WrongInputConnection",
                         "WrongBindingConnection", "OnlyComponentAliases")
    with tempfile.TemporaryDirectory(prefix="compile-", dir=evidence) as directory:
        path = Path(directory) / "Concept.cpp"
        compile_fixture(arguments, path, "PlatformProviderServiceConcept<Default>", True)
        for fixture in provider_failures:
            compile_fixture(arguments, path, f"PlatformProviderServiceConcept<{fixture}>", False)
        for fixture in ("TranslatorMissingPoll", "TranslatorWrongReset"):
            compile_fixture(arguments, path, f"InputTranslatorConcept<{fixture}>", False)
    print("Valid nonmovable provider accepted; 14 intended provider/translator contract failures rejected.")


if __name__ == "__main__":
    main()
