#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#if defined(__has_include)
#if __has_include(<source_location>)
#include <source_location>
#if defined(__cpp_lib_source_location) && (__cpp_lib_source_location >= 201907L)
#define FLOWUI_ELEMENT_ID_HAS_STD_SOURCE_LOCATION 1
#endif
#endif
#endif

#include "FlowUi/BuildConfig.hpp"
#include "internal/IdentityHash.hpp"

namespace FlowUi {

/** Strong identity of a Flow element definition. */
struct FlowDefinitionID {
	uint64_t value = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	friend constexpr bool operator==(FlowDefinitionID, FlowDefinitionID) noexcept = default;
};

/** Resolved identity of one local Flow element instance or named Clay node. */
struct FlowElementID {
	uint64_t value = 0;
#if FLOW_UI_DEV_MODE
	std::string_view debugName{};
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	friend constexpr bool operator==(FlowElementID lhs, FlowElementID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

/** Explicit globally named Flow element identity. */
struct GlobalFlowID {
	uint64_t value = 0;
#if FLOW_UI_DEV_MODE
	std::string_view debugName{};
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	friend constexpr bool operator==(GlobalFlowID lhs, GlobalFlowID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

/** Definition-time declaration of one semantic part owned by an element. */
struct FlowElementPart {
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	std::string_view debugName{};
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}

	friend constexpr bool operator==(FlowElementPart lhs, FlowElementPart rhs) noexcept {
		return lhs.token == rhs.token;
	}
};

/** Semantic part declaration bound to one concrete owner element instance. */
struct FlowElementPartID {
	uint64_t value = 0;
#if FLOW_UI_DEV_MODE
	std::string_view debugName{};
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	friend constexpr bool operator==(
		FlowElementPartID lhs,
		FlowElementPartID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

inline constexpr FlowDefinitionID InvalidFlowDefinitionID{};
inline constexpr FlowElementID InvalidFlowElementID{};
inline constexpr GlobalFlowID InvalidGlobalFlowID{};
inline constexpr FlowElementPart InvalidFlowElementPart{};
inline constexpr FlowElementPartID InvalidFlowElementPartID{};

namespace detail::element_id {

// These constants are part of the stable FlowUi identity format. Changing one
// intentionally changes the corresponding identity domain.
inline constexpr uint64_t kDefinitionDomain = 0x3d8f6a19c247e5b1ull;
inline constexpr uint64_t kRootFlowScopeDomain = 0x57c31e8ab604d92full;
inline constexpr uint64_t kLocalElementDomain = 0xa40bf1d7629c3e85ull;
inline constexpr uint64_t kIndexedNameDomain = 0x6e21c9b4f83a507dull;
inline constexpr uint64_t kAutoElementDomain = 0xd17a4e835bc2609full;
inline constexpr uint64_t kGlobalElementDomain = 0x8c53f02ab971d64eull;
inline constexpr uint64_t kPartDeclarationDomain = 0x25e6a1c94f0b73d8ull;
inline constexpr uint64_t kPartInstanceDomain = 0xf4097bc15e2a86d3ull;
inline constexpr uint64_t kClayBridgeDomain = 0x71c8e534ad09f26bull;

template <typename Element>
concept HasIdentityDefinition = requires {
	requires std::same_as<
		std::remove_cv_t<decltype(std::remove_cvref_t<Element>::definitionId)>,
		FlowDefinitionID>;
	requires (std::remove_cvref_t<Element>::definitionId.value != 0);
};

template <typename Element>
constexpr uint64_t definitionValue() noexcept {
	using ElementType = std::remove_cvref_t<Element>;
	static_assert(
		HasIdentityDefinition<ElementType>,
		"FlowUi identity factories require a nonzero FlowDefinitionID definitionId.");
	return ElementType::definitionId.value;
}

[[nodiscard]] constexpr FlowElementID resolveLocal(
	FlowElementID parent,
	FlowDefinitionID definition,
	uint64_t nameToken
#if FLOW_UI_DEV_MODE
	, std::string_view debugName = {}
#endif
	) noexcept {
	if (!parent || !definition || nameToken == 0) return {};
	return FlowElementID{
		.value = detail::identity_hash::compose(
			kLocalElementDomain, parent.value, definition.value, nameToken),
#if FLOW_UI_DEV_MODE
		.debugName = debugName,
#endif
	};
}

[[nodiscard]] constexpr FlowElementID resolveAutomatic(
	FlowElementID parent,
	FlowDefinitionID definition,
	uint64_t callsiteToken
#if FLOW_UI_DEV_MODE
	, std::string_view debugName = {}
#endif
	) noexcept {
	if (!parent || !definition || callsiteToken == 0) return {};
	return FlowElementID{
		.value = detail::identity_hash::compose(
			kAutoElementDomain, parent.value, definition.value, callsiteToken),
#if FLOW_UI_DEV_MODE
		.debugName = debugName,
#endif
	};
}

} // namespace detail::element_id

/** Stable semantic parent used for top-level local elements in every window. */
inline constexpr FlowElementID RootFlowScopeID{
	.value = detail::identity_hash::compose(detail::element_id::kRootFlowScopeDomain),
#if FLOW_UI_DEV_MODE
	.debugName = {},
#endif
};

/** Compile-time prehashed local name accepted by the typed element builder. */
struct LocalElementName {
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	std::string_view debugName{};
#endif

	constexpr LocalElementName() noexcept = default;

	template <std::size_t N>
	consteval LocalElementName(const char (&name)[N])
		: token(detail::identity_hash::authoredNameToken(
			detail::element_id::kLocalElementDomain,
			detail::identity_hash::literalView(name)))
#if FLOW_UI_DEV_MODE
		, debugName(detail::identity_hash::literalView(name))
#endif
	{}

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}

};

/** Explicitly runtime-hashed local name. */
struct RuntimeElementName {
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	std::string_view debugName{};
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}
};

/** Precomposed numeric/keyed local-name token. */
struct IndexedElementName {
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	std::string_view debugName{};
	uint64_t index = 0;
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}

	friend constexpr bool operator==(
		IndexedElementName lhs,
		IndexedElementName rhs) noexcept {
		return lhs.token == rhs.token;
	}
};

/** Local positional ID sequence for compact, stable-order repeated UI. */
class IndexedElementIDSequence {
public:
	constexpr explicit IndexedElementIDSequence(
		LocalElementName baseName,
		uint64_t firstIndex = 0) noexcept
		: baseName_(baseName), nextIndex_(firstIndex) {}

	/** Return the next positional name and increment the local counter. */
	[[nodiscard]] constexpr IndexedElementName next() noexcept;

	[[nodiscard]] constexpr uint64_t nextIndex() const noexcept {
		return nextIndex_;
	}

private:
	LocalElementName baseName_{};
	uint64_t nextIndex_ = 0;
};

/** Compile-time callsite token for an explicitly automatic element ID. */
struct AutoElementName {
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	std::string_view fileName{};
	uint32_t line = 0;
	uint32_t column = 0;
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}
};

/** Create a strong definition ID from a stable authored literal. */
template <std::size_t N>
[[nodiscard]] consteval FlowDefinitionID DefinitionID(const char (&name)[N]) {
	return FlowDefinitionID{detail::identity_hash::authoredNameToken(
		detail::element_id::kDefinitionDomain,
		detail::identity_hash::literalView(name))};
}

/** Mark a string as the explicit runtime-hashed local-name path. */
[[nodiscard]] constexpr RuntimeElementName RuntimeName(std::string_view name) noexcept {
	return RuntimeElementName{
		.token = detail::identity_hash::authoredNameToken(
			detail::element_id::kLocalElementDomain,
			name),
#if FLOW_UI_DEV_MODE
		.debugName = name,
#endif
	};
}

/** Create a stable numeric positional local-name token. */
[[nodiscard]] constexpr IndexedElementName Indexed(
	LocalElementName name,
	uint64_t index) noexcept {
	if (!name) return {};
	return IndexedElementName{
		.token = detail::identity_hash::compose(
			detail::element_id::kIndexedNameDomain,
			name.token,
			index),
#if FLOW_UI_DEV_MODE
		.debugName = name.debugName,
		.index = index,
#endif
	};
}

/** Create a stable numeric data-keyed local-name token. */
[[nodiscard]] constexpr IndexedElementName Keyed(
	LocalElementName name,
	uint64_t key) noexcept {
	return Indexed(name, key);
}

inline constexpr IndexedElementName IndexedElementIDSequence::next() noexcept {
	return Indexed(baseName_, nextIndex_++);
}

/** Create a local positional sequence; state follows sequence position. */
[[nodiscard]] constexpr IndexedElementIDSequence IndexedIDs(
	LocalElementName baseName,
	uint64_t firstIndex = 0) noexcept {
	return IndexedElementIDSequence(baseName, firstIndex);
}

/** Create an automatic callsite token from an explicit callsite payload. */
[[nodiscard]] constexpr AutoElementName AutoIDAt(
	std::string_view fileName,
	uint32_t line,
	uint32_t column) {
	if (fileName.empty()) return {};
	return AutoElementName{
		.token = detail::identity_hash::compose(
			detail::element_id::kAutoElementDomain,
			detail::identity_hash::hashBytes(fileName),
			line,
			column),
#if FLOW_UI_DEV_MODE
		.fileName = fileName,
		.line = line,
		.column = column,
#endif
	};
}

/**
 * Capture the caller as an automatic identity token.
 *
 * This is constexpr, rather than consteval, so source_location propagates
 * through UiManager::createElement()'s default argument to the user callsite.
 * Literal/direct uses remain valid constant expressions.
 */
#if defined(FLOWUI_ELEMENT_ID_HAS_STD_SOURCE_LOCATION)
[[nodiscard]] constexpr AutoElementName AutoID(
	std::source_location location = std::source_location::current()) {
	return AutoIDAt(location.file_name(), location.line(), location.column());
}
#else
[[nodiscard]] constexpr AutoElementName AutoID(
	const char* fileName = __builtin_FILE(),
	uint32_t line = __builtin_LINE(),
	uint32_t column = 0) {
	return AutoIDAt(fileName ? std::string_view{fileName} : std::string_view{}, line, column);
}
#endif

/** Declare one semantic element part. */
template <std::size_t N>
[[nodiscard]] consteval FlowElementPart Part(const char (&name)[N]) {
	const std::string_view view = detail::identity_hash::literalView(name);
	return FlowElementPart{
		.token = detail::identity_hash::authoredNameToken(
			detail::element_id::kPartDeclarationDomain,
			view),
#if FLOW_UI_DEV_MODE
		.debugName = view,
#endif
	};
}

/**
 * Create a definition-scoped global element identity.
 *
 * The value is parent-independent but remains window-local when used as an
 * element address because WindowId is the outer storage/interaction scope.
 */
template <auto& Element, std::size_t N>
	requires detail::element_id::HasIdentityDefinition<decltype(Element)>
[[nodiscard]] consteval GlobalFlowID Global(const char (&name)[N]) {
	const std::string_view view = detail::identity_hash::literalView(name);
	constexpr uint64_t definition =
		detail::element_id::definitionValue<decltype(Element)>();
	static_assert(definition != 0, "FlowUi global IDs require a nonzero definitionId.");
	return GlobalFlowID{
		.value = detail::identity_hash::compose(
			detail::element_id::kGlobalElementDomain,
			definition,
			detail::identity_hash::hashBytes(view)),
#if FLOW_UI_DEV_MODE
		.debugName = view,
#endif
	};
}

/** Long-form alias for Global(), useful in declarations that emphasize identity. */
template <auto& Element, std::size_t N>
	requires detail::element_id::HasIdentityDefinition<decltype(Element)>
[[nodiscard]] consteval GlobalFlowID GlobalID(const char (&name)[N]) {
	return Global<Element>(name);
}

/** Bind a semantic part to a resolved local owner instance. */
template <typename OwnerElement>
	requires detail::element_id::HasIdentityDefinition<OwnerElement>
[[nodiscard]] constexpr FlowElementPartID PartID(
	const OwnerElement&,
	FlowElementID owner,
	FlowElementPart part) noexcept {
	const uint64_t definition = detail::element_id::definitionValue<OwnerElement>();
	if (!owner || !part || definition == 0) return {};
	return FlowElementPartID{
		.value = detail::identity_hash::compose(
			detail::element_id::kPartInstanceDomain,
			definition,
			owner.value,
			part.token),
#if FLOW_UI_DEV_MODE
		.debugName = part.debugName,
#endif
	};
}

/** Bind a semantic part to a global owner instance. */
template <typename OwnerElement>
	requires detail::element_id::HasIdentityDefinition<OwnerElement>
[[nodiscard]] constexpr FlowElementPartID PartID(
	const OwnerElement& element,
	GlobalFlowID owner,
	FlowElementPart part) noexcept {
	if (!owner) return {};
	return PartID(
		element,
		FlowElementID{
			.value = owner.value,
#if FLOW_UI_DEV_MODE
			.debugName = owner.debugName,
#endif
		},
		part);
}

namespace detail::element_id {

/** Internal numeric implementation of the deterministic Clay bridge. */
[[nodiscard]] constexpr uint32_t toClayValue(uint64_t flowValue) noexcept {
	if (flowValue == 0) return 0;
	const uint64_t mixed = detail::identity_hash::avalanche64(
		flowValue ^ kClayBridgeDomain);
	const uint32_t folded = static_cast<uint32_t>(mixed) ^
		static_cast<uint32_t>(mixed >> 32);
	return folded == 0 ? 1u : folded;
}

} // namespace detail::element_id

/** Deterministically fold a strong Flow identity into Clay's 32-bit domain. */
[[nodiscard]] constexpr uint32_t FlowIDToClayID(FlowElementID id) noexcept {
	return detail::element_id::toClayValue(id.value);
}

[[nodiscard]] constexpr uint32_t FlowIDToClayID(GlobalFlowID id) noexcept {
	return detail::element_id::toClayValue(id.value);
}

[[nodiscard]] constexpr uint32_t FlowIDToClayID(FlowElementPartID id) noexcept {
	return detail::element_id::toClayValue(id.value);
}

struct FlowDefinitionIDHash {
	[[nodiscard]] constexpr std::size_t operator()(FlowDefinitionID id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};

struct FlowElementIDHash {
	[[nodiscard]] constexpr std::size_t operator()(FlowElementID id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};

struct GlobalFlowIDHash {
	[[nodiscard]] constexpr std::size_t operator()(GlobalFlowID id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};

struct FlowElementPartIDHash {
	[[nodiscard]] constexpr std::size_t operator()(FlowElementPartID id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};

static_assert(std::is_trivially_copyable_v<FlowDefinitionID>);
static_assert(std::is_trivially_copyable_v<FlowElementID>);
static_assert(std::is_trivially_copyable_v<GlobalFlowID>);
static_assert(std::is_trivially_copyable_v<FlowElementPart>);
static_assert(std::is_trivially_copyable_v<FlowElementPartID>);
static_assert(std::is_trivially_copyable_v<LocalElementName>);
static_assert(std::is_trivially_copyable_v<RuntimeElementName>);
static_assert(std::is_trivially_copyable_v<IndexedElementName>);
static_assert(std::is_trivially_copyable_v<IndexedElementIDSequence>);
static_assert(std::is_trivially_copyable_v<AutoElementName>);

#if !FLOW_UI_DEV_MODE
static_assert(sizeof(FlowDefinitionID) == sizeof(uint64_t));
static_assert(sizeof(FlowElementID) == sizeof(uint64_t));
static_assert(sizeof(GlobalFlowID) == sizeof(uint64_t));
static_assert(sizeof(FlowElementPart) == sizeof(uint64_t));
static_assert(sizeof(FlowElementPartID) == sizeof(uint64_t));
static_assert(sizeof(LocalElementName) == sizeof(uint64_t));
static_assert(sizeof(RuntimeElementName) == sizeof(uint64_t));
static_assert(sizeof(IndexedElementName) == sizeof(uint64_t));
static_assert(sizeof(IndexedElementIDSequence) == sizeof(uint64_t) * 2);
static_assert(sizeof(AutoElementName) == sizeof(uint64_t));
#endif

} // namespace FlowUi

#if defined(FLOWUI_ELEMENT_ID_HAS_STD_SOURCE_LOCATION)
#undef FLOWUI_ELEMENT_ID_HAS_STD_SOURCE_LOCATION
#endif
