#pragma once

/** @file
 * @brief Public AgenticJson declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "FlowUi/Public/errorManagement/Error.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace FlowUi::devSystems::agentic {

/** Owned JSON value. Number tokens remain decimal text, avoiding lossy integer conversion. */
struct Json {
	/**
	 * @brief Active JSON representation; only its associated payload member is interpreted.
	 */
	enum class Kind : std::uint8_t {
		null, /**< JSON null, with no active payload. */
		boolean, /**< Use the boolean payload. */
		number, /**< Use an exact decimal numeric token stored in text. */
		string, /**< Use the owned UTF-8 text payload. */
		array, /**< Use the owned array elements. */
		object /**< Use the owned object key/value entries. */
	};
	/** @brief Owned key/value entries, used when kind is Kind::object. */
	std::vector<std::pair<std::string, Json>> object;
	/** @brief Owned array elements, used when kind is Kind::array. */
	std::vector<Json> array;
	/**
	 * @brief Owned text payload; JSON number tokens also remain exact decimal text in this
	 * member.
	 */
	std::string text;
	/** @brief Active JSON payload representation. */
	Kind kind = Kind::null;
	/** @brief Boolean payload, used when kind is Kind::boolean. */
	bool boolean = false;

	/** @brief Construct JSON null with empty owned storage. */
	Json() = default;
	/** @brief Move a string into owned JSON string storage. */
	Json(std::string value) : text(std::move(value)), kind(Kind::string) {}
	/** @brief Copy a string view into owned JSON string storage. */
	Json(std::string_view value) : text(value), kind(Kind::string) {}
	/** @brief Copy a non-null NUL-terminated string into owned JSON string storage. */
	Json(const char* value) : text(value), kind(Kind::string) {}
	/** @brief Construct a JSON boolean. */
	Json(bool value) : kind(Kind::boolean), boolean(value) {}
	/**
	 * @brief Encode a finite double as a JSON number; non-finite input produces JSON null.
	 */
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
	/**
	 * @brief Borrow the first matching member from object storage, or return nullptr when no key
	 * matches.
	 */
	[[nodiscard]] Json* find(std::string_view key) noexcept;
	/** Insert or replace one member. */
	void set(std::string key, Json value);
	/**
	 * @brief Borrow the string payload or fallback when kind is not Kind::string; the selected
	 * storage must outlive the view.
	 */
	[[nodiscard]] std::string_view string_or(std::string_view fallback = {}) const noexcept;
	/**
	 * @brief Return the numeric payload as double, or fallback when it cannot be decoded.
	 */
	[[nodiscard]] double number_or(double fallback = 0.0) const noexcept;
	/**
	 * @brief Decode an unsigned 64-bit value from number or string text, or return fallback when
	 * decoding fails.
	 */
	[[nodiscard]] std::uint64_t uint64_or(std::uint64_t fallback = 0) const noexcept;
	/** @brief Return the boolean payload, or fallback when kind is not Kind::boolean. */
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
