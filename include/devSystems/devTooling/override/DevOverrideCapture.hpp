#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

namespace FlowUi { class ThemeManager; }
namespace FlowUi::devSystems { class DevTimingRecorder; }

namespace FlowUi::devSystems::tooling {

class DevOverrideApply;

/** @brief Copies schema-typed authored/effective values into immutable window/theme publications; observing values never changes application state. */
class DevOverrideCapture {
public:
	/** @brief Begin building window-local value capture using a retained immutable schema generation. */
	void beginWindowFrame(
		WindowId window,
		std::uint64_t frameNumber,
		tooling::DevSchemaView schema);
	/** Copy declared application inputs before any tooling assignment is applied. */
	[[nodiscard]] DevAuthoredElementCapture captureAuthoredParameters(
		FlowDefinitionID definition, tooling::DevSchemaView schema,
		const void* parameters) const noexcept;
	/** @brief Copy the current invocation's effective parameters and optional state/resources; pointer arguments are correctly typed borrowed inputs consumed synchronously. */
	void captureElement(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		std::uint32_t flowNode,
		const void* effectiveParameters,
		const DevOverrideApply& overrides,
		DevTimingRecorder* timing = nullptr,
		DevAuthoredElementCapture authored = {},
		const void* state = nullptr,
		const void* resources = nullptr,
		bool isDevInternal = false) noexcept;
	/** @brief Publish the completed window capture and expire prior borrowed views for that window. */
	void endWindowFrame(WindowId window) noexcept;
	/** @brief Discard an in-progress capture and preserve the previous successful window publication. */
	void cancelWindowFrame(WindowId window) noexcept;
	/** Retire both capture buffers when their application window is destroyed. */
	void release_window(WindowId window) noexcept;

	/** @brief Copy reflected theme values; the borrowed override callback annotates the contributing layer without mutating themes. */
	void captureThemes(
		const ThemeManager& themes,
		tooling::DevSchemaView schema,
		const void* overrideOwner,
		bool (*isOverridden)(
			const void* owner,
			tooling::DevTypeId themeType,
			std::string_view variant,
			tooling::DevFieldId field,
			DevOverrideLayer& layer) noexcept,
		DevTimingRecorder* timing = nullptr) noexcept;

	/** @brief Borrow the latest successful capture for window, or an empty snapshot when absent. */
	[[nodiscard]] const DevElementCaptureSnapshot& elements(WindowId window) const noexcept;
	/** Visit current immutable window publications; borrowed views expire at the next frame. */
	void visitElementSnapshots(void* context,
		bool (*visitor)(void*, const DevElementCaptureSnapshot&) noexcept) const noexcept;
	/** @brief Borrow the latest theme publication until the next capture or destruction. */
	[[nodiscard]] const DevThemeCaptureSnapshot& themes() const noexcept {
		return publishedThemes_;
	}
	/** @brief Return retained capture values and publication backing bytes. */
	[[nodiscard]] std::size_t memoryFootprintBytes() const noexcept;
	/** @brief Return cumulative copied element field values. */
	[[nodiscard]] std::uint64_t capturedElementFieldCount() const noexcept {
		return capturedElementFieldCount_;
	}
	/** @brief Return cumulative copied theme field values. */
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
