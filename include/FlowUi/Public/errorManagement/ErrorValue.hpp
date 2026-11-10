#pragma once

#include "FlowUi/Public/errorManagement/ErrorCodes.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include <cstdint>
#include <type_traits>

namespace FlowUi {

/**
 * Small, allocation-free production error value.
 *
 * Debug names, source locations, stack traces, and state snapshots belong to the
 * optional development sidecar keyed by occurrence.
 */
struct FlowUiError final {
	/** Stable numeric subject interpreted according to siteDescriptor().expectedSubject. */
	std::uint64_t subject = 0u;
	/** Site-specific secondary identity, count, or size; zero when unused. */
	std::uint64_t auxiliary = 0u;
	/** Backend status code encoded in 32 bits; zero when no backend code applies. */
	std::uint32_t nativeCode = 0u;
	/** Stable failure code; None represents success/no error. */
	ErrorCode code = ErrorCode::None;
	/** Operation that detected the failure. */
	ErrorSite site = ErrorSite::None;

	/** Return whether code represents an error. */
	[[nodiscard]] constexpr bool hasError() const noexcept { return code != ErrorCode::None; }

	/** Return whether this value contains an error. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return hasError(); }

	/** Return static code metadata without allocating. */
	[[nodiscard]] constexpr ErrorDescriptor descriptor() const noexcept {
		return describeError(code);
	}

	/** Return static operation and subject-kind metadata. */
	[[nodiscard]] constexpr ErrorSiteDescriptor siteDescriptor() const noexcept {
		return describeErrorSite(site);
	}

	/** Decode the owning domain from the stable code value. */
	[[nodiscard]] constexpr ErrorDomain domain() const noexcept {
		return detail::errorDomainFromValue(code);
	}

	/** Return how to interpret the numeric subject for the detecting site. */
	[[nodiscard]] constexpr ErrorSubjectKind subjectKind() const noexcept {
		return siteDescriptor().expectedSubject;
	}

	/** Borrow the static, null-terminated message associated with code. */
	[[nodiscard]] constexpr std::string_view message() const noexcept { return errorMessage(code); }
};

static_assert(std::is_trivially_copyable_v<FlowUiError>);
static_assert(std::is_trivially_destructible_v<FlowUiError>);
static_assert(sizeof(FlowUiError) == 24u);

/** Construct a production error without allocating or formatting text.
 * @param code Stable error code.
 * @param site Detecting operation; determines how subject is interpreted.
 * @param subject Primary operation-specific numeric identity.
 * @param auxiliary Secondary identity, count, or size defined by the site.
 * @param nativeCode Backend status bits, or zero when unavailable.
 * @return Compact error containing the supplied fields unchanged.
 */
[[nodiscard]] constexpr FlowUiError makeError(ErrorCode code, ErrorSite site,
											  std::uint64_t subject = 0u,
											  std::uint64_t auxiliary = 0u,
											  std::uint32_t nativeCode = 0u) noexcept {
	return FlowUiError{
		.subject = subject,
		.auxiliary = auxiliary,
		.nativeCode = nativeCode,
		.code = code,
		.site = site,
	};
}

static_assert(noexcept(makeError(ErrorCode::None, ErrorSite::None)));

} // namespace FlowUi
