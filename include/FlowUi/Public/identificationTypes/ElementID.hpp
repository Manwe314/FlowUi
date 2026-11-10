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

#include "FlowUi/Private/elementSystem/ElementIdentity.hpp"
#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/identificationTypes/ElementIDTypes.hpp"

namespace FlowUi {

/** Stable semantic parent used for top-level local elements in every window. */
inline constexpr FlowElementID RootFlowScopeID{
	.value = detail::identity_hash::compose(detail::element_id::kRootFlowScopeDomain),
#if FLOW_UI_DEV_MODE
	.debugName = {},
#endif
};

/** Compile-time prehashed local name accepted by the typed element builder. */
struct LocalElementName {
	/** Prehashed identity token; zero is invalid. */
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed authored label retained in developer builds; backing storage must outlive its uses. */
	std::string_view debugName{};
#endif

	/** Construct an invalid local-name token. */
	constexpr LocalElementName() noexcept = default;

	template <std::size_t N>
	/** Hash an authored literal at compile time; the literal label is borrowed in developer builds. */
	consteval LocalElementName(const char (&name)[N])
		: token(detail::identity_hash::authoredNameToken(
			detail::element_id::kLocalElementDomain,
			detail::identity_hash::literalView(name)))
#if FLOW_UI_DEV_MODE
		, debugName(detail::identity_hash::literalView(name))
#endif
	{}

	/** Return whether the name has a nonzero token. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}

};

/** Explicit runtime-hashed local name created with RuntimeName(). Developer
 * builds borrow the supplied string; keep its bytes alive while the name is used. */
struct RuntimeElementName {
	/** Prehashed identity token; zero is invalid. */
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed authored label retained in developer builds; backing storage must outlive its uses. */
	std::string_view debugName{};
#endif

	/** Return whether the name has a nonzero token. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}
};

/** Precomposed numeric/keyed local-name token. */
struct IndexedElementName {
	/** Prehashed identity token; zero is invalid. */
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed authored label retained in developer builds; backing storage must outlive its uses. */
	std::string_view debugName{};
	/** Numeric index or key retained for developer inspection. */
	uint64_t index = 0;
#endif

	/** Return whether the name has a nonzero token. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}

	/** Compare hashed tokens, ignoring developer metadata. */
	[[nodiscard]] friend constexpr bool operator==(IndexedElementName lhs,
												   IndexedElementName rhs) noexcept {
		return lhs.token == rhs.token;
	}
};

/** Local positional ID sequence for compact, stable-order repeated UI. */
class IndexedElementIDSequence {
public:
	/**
	 * Start a positional sequence for baseName at firstIndex. Use data keys instead when identity must
	 * survive reordering.
	 */
	constexpr explicit IndexedElementIDSequence(
		LocalElementName baseName,
		uint64_t firstIndex = 0) noexcept
		: baseName_(baseName), nextIndex_(firstIndex) {}

	/** Return the next positional name and increment the local counter. */
	[[nodiscard]] constexpr IndexedElementName next() noexcept;

	/** Return the numeric index that next() will consume. */
	[[nodiscard]] constexpr uint64_t nextIndex() const noexcept {
		return nextIndex_;
	}

private:
	LocalElementName baseName_{};
	uint64_t nextIndex_ = 0;
};

/** Compile-time callsite token for an explicitly automatic element ID. */
struct AutoElementName {
	/** Prehashed identity token; zero is invalid. */
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed source path captured in developer builds. */
	std::string_view fileName{};
	/** Source line captured in developer builds. */
	uint32_t line = 0;
	/** Source column captured in developer builds. */
	uint32_t column = 0;
#endif

	/** Return whether the name has a nonzero token. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}
};

/** Create a strong definition ID from a stable authored literal. */
template <std::size_t N>
[[nodiscard]] consteval FlowDefinitionID DefinitionID(const char (&name)[N]) noexcept {
	return FlowDefinitionID{detail::identity_hash::authoredNameToken(
		detail::element_id::kDefinitionDomain,
		detail::identity_hash::literalView(name))};
}

/** Hash a runtime local name; an empty name produces an invalid token.
 * @param name UTF-8/name bytes; developer builds borrow this view.
 * @return Name token later combined with the current parent and definition. */
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

/** Hash file, line, and column into an automatic callsite token. An empty
 * fileName returns an invalid token. Developer builds borrow fileName; do not
 * use the same callsite repeatedly under one parent without an index/key. */
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

/** Hash a stable literal declaring one definition-owned semantic part.
 * Bind the declaration to an instance with PartID() or ElementBuildContext::part(). */
template <std::size_t N>
[[nodiscard]] consteval FlowElementPart Part(const char (&name)[N]) noexcept {
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
[[nodiscard]] consteval GlobalFlowID Global(const char (&name)[N]) noexcept {
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
[[nodiscard]] consteval GlobalFlowID GlobalID(const char (&name)[N]) noexcept {
	return Global<Element>(name);
}

/** Resolve a semantic part from its owner definition, local instance, and
 * declaration. Any invalid owner, declaration, or definition returns an invalid
 * part ID; the element argument supplies its type and is not retained. */
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

/** Resolve a semantic part from a global owner using the same definition and
 * part hashing rules as the local overload. Invalid input returns an invalid ID. */
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


/** Deterministically fold a local Flow identity into Clay's 32-bit domain.
 * The result is a layout identifier, not a reversible encoding of the Flow ID. */
[[nodiscard]] constexpr uint32_t FlowIDToClayID(FlowElementID id) noexcept {
	return detail::element_id::toClayValue(id.value);
}

/** Fold a global Flow identity into the Clay layout identifier domain. */
[[nodiscard]] constexpr uint32_t FlowIDToClayID(GlobalFlowID id) noexcept {
	return detail::element_id::toClayValue(id.value);
}

/** Fold a resolved part identity into the Clay layout identifier domain. */
[[nodiscard]] constexpr uint32_t FlowIDToClayID(FlowElementPartID id) noexcept {
	return detail::element_id::toClayValue(id.value);
}

/** Hash adapter for FlowDefinitionID; developer labels do not affect hashing. */
struct FlowDefinitionIDHash {
	/** Return a container hash of the numeric identity. */
	[[nodiscard]] constexpr std::size_t operator()(FlowDefinitionID id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};

/** Hash adapter for FlowElementID; developer labels do not affect hashing. */
struct FlowElementIDHash {
	/** Return a container hash of the numeric identity. */
	[[nodiscard]] constexpr std::size_t operator()(FlowElementID id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};

/** Hash adapter for GlobalFlowID; developer labels do not affect hashing. */
struct GlobalFlowIDHash {
	/** Return a container hash of the numeric identity. */
	[[nodiscard]] constexpr std::size_t operator()(GlobalFlowID id) const noexcept {
		return static_cast<std::size_t>(id.value);
	}
};

/** Hash adapter for FlowElementPartID; developer labels do not affect hashing. */
struct FlowElementPartIDHash {
	/** Return a container hash of the numeric identity. */
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
