#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "FlowUi/Error.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace FlowUi::devSystems::agentic {

/** Owned JSON value. Number tokens remain decimal text, avoiding lossy integer conversion. */
struct Json {
	enum class Kind : std::uint8_t { null, boolean, number, string, array, object };
	std::vector<std::pair<std::string, Json>> object;
	std::vector<Json> array;
	std::string text;
	Kind kind = Kind::null;
	bool boolean = false;

	Json() = default;
	Json(std::string value) : text(std::move(value)), kind(Kind::string) {}
	Json(std::string_view value) : text(value), kind(Kind::string) {}
	Json(const char* value) : text(value), kind(Kind::string) {}
	Json(bool value) : kind(Kind::boolean), boolean(value) {}
	explicit Json(double value);
	/** Build an owned object, replacing no fields implicitly. */
	[[nodiscard]] static Json
	make_object(std::initializer_list<std::pair<std::string, Json>> fields = {});
	/** Build an owned array. */
	[[nodiscard]] static Json make_array(std::initializer_list<Json> values = {});
	/** Encode a bounded JSON numeric count; identifiers should use decimal(). */
	[[nodiscard]] static Json number(std::uint64_t value);
	/** Look up an object member without allocating. */
	[[nodiscard]] const Json* find(std::string_view key) const noexcept;
	[[nodiscard]] Json* find(std::string_view key) noexcept;
	/** Insert or replace one member. */
	void set(std::string key, Json value);
	[[nodiscard]] std::string_view string_or(std::string_view fallback = {}) const noexcept;
	[[nodiscard]] double number_or(double fallback = 0.0) const noexcept;
	[[nodiscard]] std::uint64_t uint64_or(std::uint64_t fallback = 0) const noexcept;
	[[nodiscard]] bool bool_or(bool fallback = false) const noexcept;
};

/** Parse strict JSON, rejecting duplicate fields, excessive nesting and invalid Unicode. */
[[nodiscard]] Result<Json> parse_json(std::string_view input,
									  std::size_t maximum_bytes = 16u * 1024u * 1024u) noexcept;
/** Serialize canonical compact JSON, with object keys sorted for stable request hashes. */
[[nodiscard]] std::string write_json(const Json& value);
/** Encode a lossless unsigned 64-bit field as a schema-constrained decimal string. */
[[nodiscard]] Json decimal(std::uint64_t value);
/** Decode exactly a canonical decimal unsigned 64-bit string. */
[[nodiscard]] Result<std::uint64_t> parse_decimal(const Json& value) noexcept;

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
