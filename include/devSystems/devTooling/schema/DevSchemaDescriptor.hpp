#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"

namespace FlowUi::devSystems::tooling {

template <typename T>
/** @brief ADL customization tag for flowUiDevSchema() or flowUiDevEnumSchema() declarations; T is the reflected C++ type. */
struct DevSchemaTag {};

/** @brief Compile-time field visibility, editor hints, and validation constraints. */
struct DevFieldOptions {
	/** @brief Optional numeric min/max/step metadata. */
	DevNumericConstraint numeric{};
	/** @brief Borrowed static help text copied into published schema metadata. */
	std::string_view hint{};

	/** @brief Return a copy of these options with borrowed static help text; publication copies the text. */
	consteval DevFieldOptions withHint(std::string_view value) const {
		auto result = *this;
		result.hint = value;
		return result;
	}
	/** @brief Return a copy explicitly forbidding ordinary field edits. */
	consteval DevFieldOptions readOnly() const {
		auto result = *this;
		result.access = DevFieldAccess::ReadOnly;
		return result;
	}
	/** @brief Return a copy excluding the field from visible editors. */
	consteval DevFieldOptions hidden() const {
		auto result = *this;
		result.access = DevFieldAccess::Hidden;
		return result;
	}
	/** @brief Return a copy selecting an explicit semantic editor. */
	consteval DevFieldOptions withEditor(DevEditorKind value) const {
		auto result = *this;
		result.editor = value;
		return result;
	}
	/** @brief Return a copy selecting the resource/font/action catalogue used by the editor. */
	consteval DevFieldOptions withChoiceDomain(DevChoiceDomain value) const {
		auto result = *this;
		result.choiceDomain = value;
		return result;
	}
	/** @brief Return a copy declaring inclusive numeric bounds; incompatible member types are diagnosed at publication. */
	consteval DevFieldOptions numericRange(double minimum, double maximum) const {
		auto result = *this;
		result.numeric.minimum = minimum;
		result.numeric.maximum = maximum;
		result.numeric.hasMinimum = true;
		result.numeric.hasMaximum = true;
		return result;
	}
	/** @brief Return a copy declaring an explicit numeric editor increment. */
	consteval DevFieldOptions withStep(double value) const {
		auto result = *this;
		result.numeric.step = value;
		result.numeric.hasStep = true;
		return result;
	}
	/** @brief Return a copy declaring a maximum text length; incompatible member types are diagnosed at publication. */
	consteval DevFieldOptions textLimit(std::uint32_t value) const {
		auto result = *this;
		result.textMaximum = value;
		result.hasTextMaximum = true;
		return result;
	}
	/** @brief Text maximum applied when hasTextMaximum is true. */
	std::uint32_t textMaximum = 0;
	/** @brief Declared access policy, or Inherit for the role/type default. */
	DevFieldAccess access = DevFieldAccess::Inherit;
	/** @brief Explicit editor choice, or None for adapter inference. */
	DevEditorKind editor = DevEditorKind::None;
	/** @brief Catalogue domain for semantic resource/action/font editors. */
	DevChoiceDomain choiceDomain = DevChoiceDomain::None;
	/** @brief Whether a text-length maximum was explicitly supplied. */
	bool hasTextMaximum = false;
};

template <typename>
struct MemberPointerTraits;

template <typename OwnerT, typename ValueT>
struct MemberPointerTraits<ValueT OwnerT::*> {
	using Owner = OwnerT;
	using Value = ValueT;
};

template <auto Member>
struct DevMemberOps {
	using Traits = MemberPointerTraits<decltype(Member)>;
	using Owner = typename Traits::Owner;
	using Value = typename Traits::Value;

	static const void* constAddress(const void* owner) noexcept {
		if (owner == nullptr) return nullptr;
		return std::addressof(static_cast<const Owner*>(owner)->*Member);
	}

	static void* mutableAddress(void* owner) noexcept {
		if (owner == nullptr) return nullptr;
		return std::addressof(static_cast<Owner*>(owner)->*Member);
	}

	static DevValueOperationStatus copyConstructMember(
		const void* owner,
		void* destination) noexcept {
		if (owner == nullptr) return DevValueOperationStatus::NullSource;
		if (destination == nullptr) return DevValueOperationStatus::NullDestination;
		if constexpr (!std::is_copy_constructible_v<Value>) {
			return DevValueOperationStatus::Unsupported;
		} else {
			try {
				std::construct_at(
					static_cast<Value*>(destination),
					static_cast<const Owner*>(owner)->*Member);
				return DevValueOperationStatus::Success;
			} catch (...) {
				return DevValueOperationStatus::Failed;
			}
		}
	}

	static DevValueOperationStatus assignMemberFromCopy(
		void* owner,
		const void* source) noexcept {
		if (owner == nullptr) return DevValueOperationStatus::NullDestination;
		if (source == nullptr) return DevValueOperationStatus::NullSource;
		if constexpr (std::is_const_v<Value> || !std::is_copy_assignable_v<Value>) {
			return DevValueOperationStatus::Unsupported;
		} else {
			try {
				static_cast<Owner*>(owner)->*Member = *static_cast<const Value*>(source);
				return DevValueOperationStatus::Success;
			} catch (...) {
				return DevValueOperationStatus::Failed;
			}
		}
	}

	static constexpr DevFieldOps operations{
		.constAddress = &constAddress,
		.mutableAddress = std::is_const_v<Value> ? nullptr : &mutableAddress,
		.copyConstructMember = &copyConstructMember,
		.assignMemberFromCopy = &assignMemberFromCopy,
	};
};

template <auto Member>
/** @brief Compile-time declaration of a typed non-static member and its source metadata. */
struct DevStaticFieldDescriptor {
	/** @brief Member pointer owner/value traits for this declaration. */
	using Traits = MemberPointerTraits<decltype(Member)>;
	/** @brief Exact C++ owner type of the declared member. */
	using Owner = typename Traits::Owner;
	/** @brief Exact declared C++ member value type. */
	using Value = typename Traits::Value;
	/** @brief Typed C++ member pointer retained in the descriptor type. */
	static constexpr auto member = Member;

	/** @brief Borrowed static member name copied during schema publication. */
	std::string_view name{};
	/** @brief Compile-time editor, access, and constraint options. */
	DevFieldOptions options{};
	/** @brief Borrowed declaration source path. */
	std::string_view sourceFile{};
	/** @brief Borrowed declaration function name. */
	std::string_view sourceFunction{};
	/** @brief Declaration source line, or zero when unavailable. */
	std::uint32_t sourceLine = 0;
	/** @brief Declaration source column, or zero when unavailable. */
	std::uint32_t sourceColumn = 0;
};

/** @brief Declare a typed non-static data member without source metadata; name/help strings must have static lifetime until ingestion. */
template <auto Member>
consteval auto devField(
	std::string_view name,
	DevFieldOptions options = {}) {
	static_assert(std::is_member_object_pointer_v<decltype(Member)>,
		"FlowUi DEV fields must name a non-static data member.");
	return DevStaticFieldDescriptor<Member>{
		.name = name,
		.options = options,
		.sourceFile = {},
		.sourceFunction = {},
		.sourceLine = 0,
		.sourceColumn = 0,
	};
}

/** @brief Declare a typed non-static member with borrowed source metadata copied at publication; normally called by FLOWUI_DEV_FIELD. */
template <auto Member>
consteval auto devFieldAt(
	std::string_view name,
	std::string_view sourceFile,
	std::uint32_t sourceLine,
	std::uint32_t sourceColumn,
	std::string_view sourceFunction,
	DevFieldOptions options = {}) {
	static_assert(std::is_member_object_pointer_v<decltype(Member)>,
		"FlowUi DEV fields must name a non-static data member.");
	return DevStaticFieldDescriptor<Member>{
		.name = name,
		.options = options,
		.sourceFile = sourceFile,
		.sourceFunction = sourceFunction,
		.sourceLine = sourceLine,
		.sourceColumn = sourceColumn,
	};
}

template <typename... Fields>
/** @brief Compile-time object/semantic-type descriptor produced by devStruct() and related factories. */
struct DevStaticStructDescriptor {
	/** @brief Borrowed static display name copied during ingestion. */
	std::string_view name{};
	/** @brief Borrowed static help text copied during ingestion. */
	std::string_view hint{};
	/** @brief Preferred semantic editor for this object. */
	DevEditorKind editor = DevEditorKind::ObjectGroup;
	/** @brief Declared metadata/value capture capability. */
	DevCaptureCapability capture = DevCaptureCapability::Value;
	/** @brief Declared edit capability before field/role restrictions. */
	DevEditCapability edit = DevEditCapability::Editable;
	/** @brief Explanation for reduced capability. */
	DevCapabilityReason reason = DevCapabilityReason::None;
	/** @brief Typed member descriptors in declaration order. */
	std::tuple<Fields...> fields{};
};

/** @brief Declare an object schema with typed fields in source order; names/help strings are borrowed until registry ingestion. */
template <typename... Fields>
consteval auto devStruct(std::string_view name, Fields... fields) {
	return DevStaticStructDescriptor<Fields...>{
		.name = name,
		.fields = std::tuple<Fields...>{fields...},
	};
}

/** @brief Declare a leaf semantic editor and capture/edit capabilities without exposing object members. */
consteval auto devSemanticLeaf(
	std::string_view name,
	DevEditorKind editor,
	DevCaptureCapability capture,
	DevEditCapability edit,
	DevCapabilityReason reason = DevCapabilityReason::None) {
	return DevStaticStructDescriptor<>{
		.name = name,
		.editor = editor,
		.capture = capture,
		.edit = edit,
		.reason = reason,
	};
}

/** @brief Declare a compound semantic editor with typed member descriptors in source order. */
template <typename... Fields>
consteval auto devSemanticStruct(
	std::string_view name,
	DevEditorKind editor,
	Fields... fields) {
	return DevStaticStructDescriptor<Fields...>{
		.name = name,
		.editor = editor,
		.fields = std::tuple<Fields...>{fields...},
	};
}

template <typename T>
/** @brief Fallback customization for reflected non-enum types. Specialize with enabled=true and a consteval schema() descriptor, or provide an ADL/intrusive declaration instead. */
struct DevTypeAdapter {
	/** @brief False until a user/built-in specialization supplies an enabled schema adapter. */
	static constexpr bool enabled = false;
};

template <typename T>
/** @brief Fallback customization for reflected enum choices. Specialize with enabled=true and consteval schema(), or provide an ADL enum declaration. */
struct DevEnumAdapter {
	/** @brief False until a user/built-in specialization supplies an enabled schema adapter. */
	static constexpr bool enabled = false;
};

template <typename Enum>
/** @brief Compile-time named value belonging to a single enum type. */
struct DevStaticEnumValueDescriptor {
	static_assert(std::is_enum_v<Enum>);
	/** @brief Borrowed static enumerator display name. */
	std::string_view name{};
	/** @brief Typed enum value exposed by the choice editor. */
	Enum value{};
};

/** @brief Declare one named typed enum value; the name is borrowed until registry ingestion. */
template <typename Enum>
consteval auto devEnumValue(std::string_view name, Enum value) {
	return DevStaticEnumValueDescriptor<Enum>{name, value};
}

template <typename Enum, typename... Values>
/** @brief Compile-time named enum choices produced by devEnum(). */
struct DevStaticEnumDescriptor {
	static_assert(std::is_enum_v<Enum>);
	/** @brief Borrowed static enum display name. */
	std::string_view name{};
	/** @brief Typed enumerator descriptors in display order. */
	std::tuple<Values...> values{};
};

/** @brief Declare an enum choice schema; every value must belong to Enum and names/values are checked for duplicates during ingestion. */
template <typename Enum, typename... Values>
consteval auto devEnum(std::string_view name, Values... values) {
	static_assert(std::is_enum_v<Enum>);
	static_assert((std::is_same_v<Values, DevStaticEnumValueDescriptor<Enum>> && ...),
		"Every FlowUi DEV enum value must belong to the declared enum type.");
	return DevStaticEnumDescriptor<Enum, Values...>{name, std::tuple<Values...>{values...}};
}

namespace schema_detail {

template <typename T>
concept HasAdlDevSchema = requires {
	flowUiDevSchema(DevSchemaTag<T>{});
};

template <typename T>
concept HasIntrusiveDevSchema = requires {
	T::devSchema();
};

template <typename T>
concept HasAdlDevEnumSchema = requires {
	flowUiDevEnumSchema(DevSchemaTag<T>{});
};

template <typename T>
consteval auto declaredSchema() {
	static_assert(!(HasAdlDevSchema<T> && HasIntrusiveDevSchema<T>),
		"FlowUi DEV type declares both ADL flowUiDevSchema and T::devSchema().");
	if constexpr (HasAdlDevSchema<T>) {
		return flowUiDevSchema(DevSchemaTag<T>{});
	} else if constexpr (HasIntrusiveDevSchema<T>) {
		return T::devSchema();
	} else {
		return DevTypeAdapter<T>::schema();
	}
}

template <typename T>
inline constexpr bool hasDeclaredSchema =
	DevTypeAdapter<T>::enabled || HasAdlDevSchema<T> || HasIntrusiveDevSchema<T>;

template <typename T>
inline constexpr bool hasDeclaredEnumSchema =
	DevEnumAdapter<T>::enabled || HasAdlDevEnumSchema<T>;

template <typename T>
consteval auto declaredEnumSchema() {
	static_assert(std::is_enum_v<T>);
	static_assert(!(HasAdlDevEnumSchema<T> && DevEnumAdapter<T>::enabled),
		"FlowUi DEV enum declares both an ADL schema and a built-in adapter.");
	if constexpr (HasAdlDevEnumSchema<T>) {
		return flowUiDevEnumSchema(DevSchemaTag<T>{});
	} else {
		return DevEnumAdapter<T>::schema();
	}
}

template <typename T>
struct IsOptional : std::false_type {};
template <typename T>
struct IsOptional<std::optional<T>> : std::true_type { using Value = T; };

template <typename T>
struct IsVector : std::false_type {};
template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type { using Value = T; };

template <typename T>
struct IsVectorBool : std::false_type {};
template <typename Allocator>
struct IsVectorBool<std::vector<bool, Allocator>> : std::true_type {};

template <typename T>
struct IsArray : std::false_type {};
template <typename T, std::size_t Size>
struct IsArray<std::array<T, Size>> : std::true_type {
	using Value = T;
	static constexpr std::size_t extent = Size;
};

template <typename T>
inline constexpr bool isString =
	std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>;

} // namespace schema_detail

template <typename... Elements>
/** @brief Compile-time pack of Flow element definition types; no runtime instances are stored. */
struct DevElementCatalogue {};
template <typename... Themes>
/** @brief Compile-time pack of theme payload types; no runtime instances are stored. */
struct DevThemeCatalogue {};

template <typename... Structs>
/** @brief Compile-time pack of additional reflected struct types; no runtime instances are stored. */
struct DevStructCatalogue {};

/** @brief Create a compile-time pack of element types for devCatalogue(). */
template <typename... Elements>
consteval auto elements() { return DevElementCatalogue<Elements...>{}; }
/** @brief Create a compile-time pack of theme types for devCatalogue(). */
template <typename... Themes>
consteval auto themes() { return DevThemeCatalogue<Themes...>{}; }
/** @brief Create a compile-time pack of additional reflected structs for devCatalogue(). */
template <typename... Structs>
consteval auto structs() { return DevStructCatalogue<Structs...>{}; }

template <typename ElementList, typename ThemeList, typename StructList>
/** @brief Compile-time pack grouping elements, themes, and additional reflected structs. */
struct DevCatalogue {
	/** @brief Element type pack to register. */
	ElementList elementTypes{};
	/** @brief Theme type pack to register. */
	ThemeList themeTypes{};
	/** @brief Additional struct type pack to register. */
	StructList structTypes{};
};

/** @brief Combine compile-time element/theme/struct packs for DevSchemaRegistry::ingest(). */
template <typename ElementList, typename ThemeList, typename StructList>
consteval auto devCatalogue(ElementList elements, ThemeList themes, StructList structs) {
	return DevCatalogue<ElementList, ThemeList, StructList>{elements, themes, structs};
}

} // namespace FlowUi::devSystems::tooling

/** @brief Declare a typed field using its member name and source location; optional arguments supply DevFieldOptions. */
#define FLOWUI_DEV_FIELD(TYPE, MEMBER, ...)                          \
	::FlowUi::devSystems::tooling::devFieldAt<&TYPE::MEMBER>(        \
		#MEMBER, __FILE__, static_cast<std::uint32_t>(__LINE__), 0u, \
		__func__ __VA_OPT__(, ) __VA_ARGS__)

/** @brief Declare an ADL struct schema in the reflected type's namespace. Types and fields are registered only in development builds. */
#define FLOWUI_DEV_SCHEMA(TYPE, ...)                                                           \
	inline consteval auto flowUiDevSchema(::FlowUi::devSystems::tooling::DevSchemaTag<TYPE>) { \
		return ::FlowUi::devSystems::tooling::devStruct(#TYPE __VA_OPT__(, ) __VA_ARGS__);     \
	}

/** @brief Declare a named typed enum choice using the supplied enum expression. */
#define FLOWUI_DEV_ENUM_VALUE(TYPE, VALUE) \
	::FlowUi::devSystems::tooling::devEnumValue<TYPE>(#VALUE, VALUE)

/** @brief Declare an ADL enum schema in the enum's namespace with typed named choices. */
#define FLOWUI_DEV_ENUM_SCHEMA(TYPE, ...)                                                          \
	inline consteval auto flowUiDevEnumSchema(::FlowUi::devSystems::tooling::DevSchemaTag<TYPE>) { \
		return ::FlowUi::devSystems::tooling::devEnum<TYPE>(#TYPE __VA_OPT__(, ) __VA_ARGS__);     \
	}

#else

#define FLOWUI_DEV_FIELD(...)
#define FLOWUI_DEV_SCHEMA(...)
#define FLOWUI_DEV_ENUM_VALUE(...)
#define FLOWUI_DEV_ENUM_SCHEMA(...)

#endif
