#pragma once

#include "FlowUi/BuildConfig.hpp"

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

namespace FlowUi::devMode {

template <typename T>
struct DevSchemaTag {};

struct DevFieldOptions {
	std::string_view hint{};
	DevFieldAccess access = DevFieldAccess::Inherit;
	DevEditorKind editor = DevEditorKind::None;
	DevNumericConstraint numeric{};
	std::uint32_t textMaximum = 0;
	bool hasTextMaximum = false;

	consteval DevFieldOptions withHint(std::string_view value) const {
		auto result = *this;
		result.hint = value;
		return result;
	}
	consteval DevFieldOptions readOnly() const {
		auto result = *this;
		result.access = DevFieldAccess::ReadOnly;
		return result;
	}
	consteval DevFieldOptions hidden() const {
		auto result = *this;
		result.access = DevFieldAccess::Hidden;
		return result;
	}
	consteval DevFieldOptions withEditor(DevEditorKind value) const {
		auto result = *this;
		result.editor = value;
		return result;
	}
	consteval DevFieldOptions numericRange(double minimum, double maximum) const {
		auto result = *this;
		result.numeric.minimum = minimum;
		result.numeric.maximum = maximum;
		result.numeric.hasMinimum = true;
		result.numeric.hasMaximum = true;
		return result;
	}
	consteval DevFieldOptions withStep(double value) const {
		auto result = *this;
		result.numeric.step = value;
		result.numeric.hasStep = true;
		return result;
	}
	consteval DevFieldOptions textLimit(std::uint32_t value) const {
		auto result = *this;
		result.textMaximum = value;
		result.hasTextMaximum = true;
		return result;
	}
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
struct DevStaticFieldDescriptor {
	using Traits = MemberPointerTraits<decltype(Member)>;
	using Owner = typename Traits::Owner;
	using Value = typename Traits::Value;
	static constexpr auto member = Member;

	std::string_view name{};
	DevFieldOptions options{};
	std::string_view sourceFile{};
	std::string_view sourceFunction{};
	std::uint32_t sourceLine = 0;
	std::uint32_t sourceColumn = 0;
};

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
struct DevStaticStructDescriptor {
	std::string_view name{};
	std::string_view hint{};
	DevEditorKind editor = DevEditorKind::ObjectGroup;
	DevCaptureCapability capture = DevCaptureCapability::Value;
	DevEditCapability edit = DevEditCapability::Editable;
	DevCapabilityReason reason = DevCapabilityReason::None;
	std::tuple<Fields...> fields{};
};

template <typename... Fields>
consteval auto devStruct(std::string_view name, Fields... fields) {
	return DevStaticStructDescriptor<Fields...>{
		.name = name,
		.fields = std::tuple<Fields...>{fields...},
	};
}

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
struct DevTypeAdapter {
	static constexpr bool enabled = false;
};

template <typename T>
struct DevEnumAdapter {
	static constexpr bool enabled = false;
};

template <typename Enum>
struct DevStaticEnumValueDescriptor {
	static_assert(std::is_enum_v<Enum>);
	std::string_view name{};
	Enum value{};
};

template <typename Enum>
consteval auto devEnumValue(std::string_view name, Enum value) {
	return DevStaticEnumValueDescriptor<Enum>{name, value};
}

template <typename Enum, typename... Values>
struct DevStaticEnumDescriptor {
	static_assert(std::is_enum_v<Enum>);
	std::string_view name{};
	std::tuple<Values...> values{};
};

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
struct DevElementCatalogue {};
template <typename... Themes>
struct DevThemeCatalogue {};
template <typename... Structs>
struct DevStructCatalogue {};

template <typename... Elements>
consteval auto elements() { return DevElementCatalogue<Elements...>{}; }
template <typename... Themes>
consteval auto themes() { return DevThemeCatalogue<Themes...>{}; }
template <typename... Structs>
consteval auto structs() { return DevStructCatalogue<Structs...>{}; }

template <typename ElementList, typename ThemeList, typename StructList>
struct DevCatalogue {
	ElementList elementTypes{};
	ThemeList themeTypes{};
	StructList structTypes{};
};

template <typename ElementList, typename ThemeList, typename StructList>
consteval auto devCatalogue(ElementList elements, ThemeList themes, StructList structs) {
	return DevCatalogue<ElementList, ThemeList, StructList>{elements, themes, structs};
}

} // namespace FlowUi::devMode

#define FLOWUI_DEV_FIELD(TYPE, MEMBER, ...) \
	::FlowUi::devMode::devFieldAt<&TYPE::MEMBER>( \
		#MEMBER, __FILE__, static_cast<std::uint32_t>(__LINE__), 0u, __func__ \
		__VA_OPT__(,) __VA_ARGS__)

#define FLOWUI_DEV_SCHEMA(TYPE, ...) \
	inline consteval auto flowUiDevSchema( \
		::FlowUi::devMode::DevSchemaTag<TYPE>) { \
		return ::FlowUi::devMode::devStruct(#TYPE __VA_OPT__(,) __VA_ARGS__); \
	}

#define FLOWUI_DEV_ENUM_VALUE(TYPE, VALUE) \
	::FlowUi::devMode::devEnumValue<TYPE>(#VALUE, VALUE)

#define FLOWUI_DEV_ENUM_SCHEMA(TYPE, ...) \
	inline consteval auto flowUiDevEnumSchema( \
		::FlowUi::devMode::DevSchemaTag<TYPE>) { \
		return ::FlowUi::devMode::devEnum<TYPE>(#TYPE __VA_OPT__(,) __VA_ARGS__); \
	}

#else

#define FLOWUI_DEV_FIELD(...)
#define FLOWUI_DEV_SCHEMA(...)
#define FLOWUI_DEV_ENUM_VALUE(...)
#define FLOWUI_DEV_ENUM_SCHEMA(...)

#endif
