#pragma once

#include "FlowUi/Public/errorManagement/ErrorEvents.hpp"
#include <cstdio>
#include <type_traits>
#include <utility>

namespace FlowUi {

/** App-creation policies for deliberately recoverable error families. */
enum class DefaultFontFailurePolicy : std::uint8_t {
	/** Try fallback fonts, then continue with text rendering disabled. */
	TryFallbackThenDisableText = 0,
	/** Try fallback fonts, then fail application initialization. */
	TryFallbackThenFailApp,
	/** Fail application initialization without trying fallback fonts. */
	FailAppImmediately,
};

/** Fallback behavior for unavailable image or viewport visuals. */
enum class MissingVisualPolicy : std::uint8_t {
	/** Emit the runtime fallback texture. */
	UseFallbackTexture = 0,
	/** Emit no draw for the missing optional visual. */
	SkipVisual,
};

/** Fallback behavior when an SVG cannot be generated. */
enum class IconGenerationFailurePolicy : std::uint8_t {
	/** Emit the runtime fallback texture. */
	UseFallbackTexture = 0,
	/** Emit no draw for the missing optional visual. */
	SkipVisual,
	/** Return a failure for the icon request. */
	FailRequest,
};

/** Response when a storage operation exceeds its current capacity. */
enum class StorageCapacityPolicy : std::uint8_t {
	/** Allow storage growth subject to configured growth and budget limits. */
	GrowWithinBudget = 0,
	/** Reject the operation instead of increasing or reclaiming capacity. */
	RejectOperation,
};

/** Response when a bounded resource cache is full. */
enum class CacheCapacityPolicy : std::uint8_t {
	/** Evict eligible cache entries, then retry the request. */
	EvictAndRetry = 0,
	/** Reject the operation instead of increasing or reclaiming capacity. */
	RejectOperation,
};

/** Response when the same popup ID is submitted twice in one frame. */
enum class PopupDuplicatePolicy : std::uint8_t {
	/** Keep the first popup submission and ignore later duplicates. */
	FirstSubmissionWins = 0,
	/** Report and reject a duplicate popup submission. */
	RejectDuplicate,
};

/** Response when a popup anchor cannot be resolved. */
enum class PopupMissingAnchorPolicy : std::uint8_t {
	/** Skip the popup when its anchor is unavailable. */
	SkipPopup = 0,
	/** Defer visibility until the anchor can be resolved. */
	DeferUntilAnchorExists,
	/** Use the viewport as the fallback anchor. */
	AnchorToViewport,
};

/** Response when a popup layer exceeds its available capacity. */
enum class PopupCapacityPolicy : std::uint8_t {
	/** Clamp the popup to the supported layer range. */
	ClampLayer = 0,
	/** Skip popups that exceed supported capacity. */
	SkipOverflowingPopup,
};

/** Which queued Unicode input is discarded when the bounded platform text queue fills. */
enum class InputQueueOverflowPolicy : std::uint8_t {
	/** Keep queued text and discard incoming codepoints when full. */
	DropNewest = 0,
	/** Discard the oldest queued codepoint to admit new input. */
	DropOldest,
};

/**
 * Recovery choices copied at app creation; reporting destinations are in ErrorObserverConfig.
 * Storage growth also requires MemoryCapacityProfile::allowRuntimeGrowth and available budget.
 */
struct ErrorPolicy {
	/** Default-font startup recovery policy. */
	DefaultFontFailurePolicy defaultFont = DefaultFontFailurePolicy::TryFallbackThenDisableText;
	/** Rendering policy for missing images. */
	MissingVisualPolicy missingImage = MissingVisualPolicy::UseFallbackTexture;
	/** Rendering policy for missing viewports. */
	MissingVisualPolicy missingViewport = MissingVisualPolicy::UseFallbackTexture;
	/** SVG rasterization failure policy. */
	IconGenerationFailurePolicy iconGeneration = IconGenerationFailurePolicy::UseFallbackTexture;
	/** Transient CPU/GPU storage growth policy. */
	StorageCapacityPolicy transientCapacity = StorageCapacityPolicy::GrowWithinBudget;
	/** Persistent storage growth policy. */
	StorageCapacityPolicy persistentCapacity = StorageCapacityPolicy::GrowWithinBudget;
	/** Texture descriptor-cache exhaustion response; does not increase the cache capacity. */
	CacheCapacityPolicy descriptorCapacity = CacheCapacityPolicy::RejectOperation;
	/** Duplicate popup-submission policy. */
	PopupDuplicatePolicy duplicatePopup = PopupDuplicatePolicy::FirstSubmissionWins;
	/** Unavailable popup-anchor policy. */
	PopupMissingAnchorPolicy missingPopupAnchor = PopupMissingAnchorPolicy::SkipPopup;
	/** Popup layer-capacity policy. */
	PopupCapacityPolicy popupCapacity = PopupCapacityPolicy::ClampLayer;
	/** Unicode text queue overflow policy. */
	InputQueueOverflowPolicy inputQueueOverflow = InputQueueOverflowPolicy::DropNewest;
	/** Per-window Unicode input queue capacity in codepoints, rather than UTF-8 bytes. */
	std::uint32_t inputTextQueueCapacity = 4096;
};

/** Non-owning application-provided destination for structured error reports. */
struct ErrorSink {
	/** Borrowed context forwarded unchanged to callback. */
	void* userData = nullptr;
	/** Nonthrowing callback; null disables the sink. */
	ErrorSinkCallback callback = nullptr;

	/** Return whether a callback is installed. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return callback != nullptr; }

	/** Invoke the sink synchronously if installed; event and its borrowed views expire after this call. */
	void notify(const ErrorEventView& event) const noexcept {
		if (callback)
			callback(userData, event);
	}
};

/** Routing of error events between the configured sink and compact default writer. */
enum class ErrorReportingMode : std::uint8_t {
	/** Deliver to both installed sink and default writer. */
	SinkAndDefault = 0,
	/** Use the sink when installed; otherwise use the default writer. */
	SinkOrDefault = 1,
	/** Use only the installed sink; no sink means no output. */
	SinkOnly = 2,
	/** Use only the default writer. */
	DefaultOnly = 3,
};

/**
 * Synchronous reporting destinations retained for the App lifetime. The sink context and output stream
 * are borrowed and must remain valid while used.
 */
struct ErrorObserverConfig {
	/** Borrowed structured event destination. */
	ErrorSink sink{};
	/** Event routing mode. */
	ErrorReportingMode mode = ErrorReportingMode::SinkOrDefault;
	/** Borrowed output stream used by the compact default writer. */
	std::FILE* output = stderr;
};

/** Error recovery policies and synchronous observers selected when an App is created. */
struct ErrorContract {
	/** Recoverable failure choices fixed for the App lifetime. */
	ErrorPolicy policy{};
	/** Reporting destinations and routing fixed for the App lifetime. */
	ErrorObserverConfig observer{};
};

static_assert(std::is_trivially_copyable_v<ErrorSink>);
static_assert(std::is_trivially_copyable_v<ErrorObserverConfig>);
static_assert(
	noexcept(std::declval<const ErrorSink&>().notify(std::declval<const ErrorEventView&>())));

} // namespace FlowUi
