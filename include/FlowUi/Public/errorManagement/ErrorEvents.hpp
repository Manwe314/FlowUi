#pragma once

#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace FlowUi {

/** What in-process evidence access remains trustworthy after a fatal error. */
enum class FatalInspectionCapability : std::uint8_t {
	/** No supported inspection capability. */
	None = 0,
	/** Only terminal/default-writer output remains available. */
	TerminalOnly,
	/** A previously captured immutable snapshot may be inspected. */
	ImmutableSnapshot,
	/** A read-only diagnostic interface may be inspected. */
	ReadOnlyInterface,
};

/** Why an error occurrence entered the centralized reporting path. */
enum class ErrorEventKind : std::uint8_t {
	/** A recoverable occurrence was resolved. */
	Resolved = 0,
	/** An occurrence was explicitly reported. */
	Reported = 1,
	/** A fatal occurrence entered reporting. */
	Fatal = 2,
	/** An underlying backend emitted a diagnostic. */
	BackendDiagnostic = 3,
};

/**
 * Allocation-free report delivered to the configured sink and default writer.
 * nativeMessage is borrowed and remains valid only for the duration of a sink
 * callback. It is empty for ordinary FlowUi errors.
 */
struct ErrorEventView {
	/** Compact production error and subject attribution. */
	FlowUiError error{};
	/** Borrowed backend text valid only during the sink callback. */
	std::string_view nativeMessage{};
	/** Backend-specific severity/category bits. */
	std::uint32_t nativeCategory = 0u;
	/** Reason this event entered centralized reporting. */
	ErrorEventKind kind = ErrorEventKind::Reported;
	/** Resolution selected for this occurrence. */
	ErrorResolution resolution = ErrorResolution::None;
	/** Evidence access that remains supported after a fatal occurrence. */
	FatalInspectionCapability inspection = FatalInspectionCapability::None;
};

/**
 * Synchronous nonthrowing error observer. userData is the configured borrowed sink context; copy event
 * text during the callback to retain it.
 */
using ErrorSinkCallback = void (*)(void* userData, const ErrorEventView& event) noexcept;

static_assert(std::is_trivially_copyable_v<ErrorEventView>);

} // namespace FlowUi
