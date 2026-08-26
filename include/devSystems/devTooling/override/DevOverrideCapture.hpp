#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

namespace FlowUi { class ThemeManager; }
namespace FlowUi::devSystems { class DevTimingRecorder; }

namespace FlowUi::devSystems::tooling {

class DevOverrideApply;

class DevOverrideCapture {
public:
	void beginWindowFrame(
		WindowId window,
		std::uint64_t frameNumber,
		devMode::DevSchemaView schema);
	void captureElement(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		std::uint32_t flowNode,
		const void* effectiveParameters,
		const DevOverrideApply& overrides,
		DevTimingRecorder* timing = nullptr) noexcept;
	void endWindowFrame(WindowId window) noexcept;
	void cancelWindowFrame(WindowId window) noexcept;

	void captureThemes(
		const ThemeManager& themes,
		devMode::DevSchemaView schema,
		const void* overrideOwner,
		bool (*isOverridden)(
			const void* owner,
			devMode::DevTypeId themeType,
			std::string_view variant,
			devMode::DevFieldId field,
			DevOverrideLayer& layer) noexcept,
		DevTimingRecorder* timing = nullptr) noexcept;

	[[nodiscard]] const DevElementCaptureSnapshot& elements(WindowId window) const noexcept;
	[[nodiscard]] const DevThemeCaptureSnapshot& themes() const noexcept {
		return publishedThemes_;
	}
	[[nodiscard]] std::size_t memoryFootprintBytes() const noexcept;
	[[nodiscard]] std::uint64_t capturedElementFieldCount() const noexcept {
		return capturedElementFieldCount_;
	}
	[[nodiscard]] std::uint64_t capturedThemeFieldCount() const noexcept {
		return capturedThemeFieldCount_;
	}

private:
	struct WindowBuffers {
		DevElementCaptureSnapshot building{};
		DevElementCaptureSnapshot published{};
		bool active = false;
	};

	static void clear(DevElementCaptureSnapshot& snapshot) noexcept;
	static void clear(DevThemeCaptureSnapshot& snapshot) noexcept;

	std::unordered_map<WindowId, WindowBuffers> windows_{};
	DevThemeCaptureSnapshot buildingThemes_{};
	DevThemeCaptureSnapshot publishedThemes_{};
	std::uint64_t nextElementGeneration_ = 1;
	std::uint64_t nextThemeGeneration_ = 1;
	std::uint64_t capturedElementFieldCount_ = 0;
	std::uint64_t capturedThemeFieldCount_ = 0;
};

} // namespace FlowUi::devSystems::tooling

#endif
