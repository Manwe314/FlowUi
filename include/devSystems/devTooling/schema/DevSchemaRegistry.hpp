#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"
#include "devSystems/devTooling/schema/DevClayTypeAdapters.hpp"
#include "devSystems/devTooling/schema/DevFlowUiTypeAdapters.hpp"
#include "internal/TypeOperations.hpp"
#include "managers/structs/FlowUiElementConcepts.hpp"

namespace FlowUi::devMode {

class DevSchemaRegistry {
public:
	explicit DevSchemaRegistry(DevSchemaLimits limits = {});
	~DevSchemaRegistry();

	DevSchemaRegistry(const DevSchemaRegistry&) = delete;
	DevSchemaRegistry& operator=(const DevSchemaRegistry&) = delete;
	DevSchemaRegistry(DevSchemaRegistry&&) = delete;
	DevSchemaRegistry& operator=(DevSchemaRegistry&&) = delete;

	template <FlowElement Element>
	void ensureElement() {
		using E = std::remove_cvref_t<Element>;
		queueRoot(
			rootKey(DevRootKind::Element, ::FlowUi::detail::typeHash<E>()),
			PendingRoot{&ingestElementRoot<E>});
	}

	template <typename Theme>
	void ensureTheme() {
		using T = std::remove_cvref_t<Theme>;
		queueRoot(
			rootKey(DevRootKind::Theme, ::FlowUi::detail::typeHash<T>()),
			PendingRoot{&ingestThemeRoot<T>});
	}

	template <typename Struct>
	void ensureStruct() {
		using T = std::remove_cvref_t<Struct>;
		queueRoot(
			rootKey(DevRootKind::Struct, ::FlowUi::detail::typeHash<T>()),
			PendingRoot{&ingestStructRoot<T>});
	}

	template <typename... Elements, typename... Themes, typename... Structs>
	void ingest(const DevCatalogue<
		DevElementCatalogue<Elements...>,
		DevThemeCatalogue<Themes...>,
		DevStructCatalogue<Structs...>>&) {
		(ensureElement<Elements>(), ...);
		(ensureTheme<Themes>(), ...);
		(ensureStruct<Structs>(), ...);
	}

	/** Publish all queued roots. Returns true only when a new generation was made. */
	bool publishPendingAtSafePoint();

	[[nodiscard]] DevSchemaView view() const noexcept;
	[[nodiscard]] std::size_t pendingRootCount() const noexcept;
	[[nodiscard]] DevSchemaLimits limits() const noexcept { return limits_; }

private:
	enum class DevRootKind : std::uint8_t { Struct, Element, Theme };
	enum class ResolutionState : std::uint8_t { Visiting, Complete, Failed };

	struct PendingRoot {
		void (*ingest)(DevSchemaRegistry&) = nullptr;
	};

	struct MutableField {
		DevFieldId id = 0;
		std::string name{};
		std::string displayName{};
		std::string hint{};
		DevTypeId ownerType = 0;
		DevTypeId valueType = 0;
		DevFieldAccess declaredAccess = DevFieldAccess::Inherit;
		DevEditorKind editor = DevEditorKind::None;
		DevEditCapability effectiveEdit = DevEditCapability::Unsupported;
		DevCapabilityReason reason = DevCapabilityReason::None;
		DevConstraintRecord constraint{};
		bool hasConstraint = false;
		const DevFieldOps* operations = nullptr;
		std::uint32_t declarationOrder = 0;
		std::string sourceFile{};
		std::string sourceFunction{};
		std::uint32_t sourceLine = 0;
		std::uint32_t sourceColumn = 0;
	};

	struct MutableType {
		struct EnumValue {
			std::string name{};
			std::uint64_t bits = 0;
		};

		DevTypeId id = 0;
		std::string displayName{};
		std::string cppTypeName{};
		DevTypeKind kind = DevTypeKind::Invalid;
		DevCaptureCapability capture = DevCaptureCapability::None;
		DevEditCapability edit = DevEditCapability::Unsupported;
		DevEditorKind editor = DevEditorKind::None;
		DevCapabilityReason reason = DevCapabilityReason::None;
		DevTypeId elementType = 0;
		std::uint32_t sequenceExtent = 0;
		bool sequenceFixed = false;
		std::uint8_t enumWidthBytes = 0;
		bool enumIsSigned = false;
		std::uint32_t size = 0;
		std::uint32_t alignment = 0;
		const DevTypeOps* operations = nullptr;
		ResolutionState state = ResolutionState::Visiting;
		std::vector<MutableField> fields{};
		std::vector<EnumValue> enumValues{};
	};

	struct MutableElement {
		FlowDefinitionID definitionId{};
		DevTypeId definitionType = 0;
		DevTypeId parametersType = 0;
		DevTypeId stateType = 0;
		DevTypeId resourcesType = 0;
		std::string displayName{};
		std::string hint{};
	};

	struct MutableTheme {
		DevTypeId themeType = 0;
		std::string displayName{};
		std::string hint{};
	};

	struct MutableDiagnostic {
		DevDiagnosticSeverity severity = DevDiagnosticSeverity::Info;
		DevDiagnosticCode code = DevDiagnosticCode::None;
		DevTypeId type = 0;
		DevFieldId field = 0;
		std::string message{};
	};

	template <typename T>
	struct TypeOperations {
		static constexpr bool isSequence =
			schema_detail::IsVector<T>::value || schema_detail::IsArray<T>::value;
		static constexpr bool hasAddressableSequenceElements =
			schema_detail::IsArray<T>::value ||
			(schema_detail::IsVector<T>::value && !schema_detail::IsVectorBool<T>::value);
		static constexpr bool hasAssignableText = requires(T& value, std::string_view text) {
			value.assign(text);
		};

		static DevValueOperationStatus copyConstruct(
			const void* source,
			void* destination) noexcept {
			if (source == nullptr) return DevValueOperationStatus::NullSource;
			if (destination == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (!std::is_copy_constructible_v<T>) {
				return DevValueOperationStatus::Unsupported;
			} else {
				try {
					std::construct_at(
						static_cast<T*>(destination), *static_cast<const T*>(source));
					return DevValueOperationStatus::Success;
				} catch (...) {
					return DevValueOperationStatus::Failed;
				}
			}
		}

		static DevValueOperationStatus copyAssign(
			void* destination,
			const void* source) noexcept {
			if (destination == nullptr) return DevValueOperationStatus::NullDestination;
			if (source == nullptr) return DevValueOperationStatus::NullSource;
			if constexpr (!std::is_copy_assignable_v<T>) {
				return DevValueOperationStatus::Unsupported;
			} else {
				try {
					*static_cast<T*>(destination) = *static_cast<const T*>(source);
					return DevValueOperationStatus::Success;
				} catch (...) {
					return DevValueOperationStatus::Failed;
				}
			}
		}

		static DevValueOperationStatus moveConstruct(
			void* source,
			void* destination) noexcept {
			if (source == nullptr) return DevValueOperationStatus::NullSource;
			if (destination == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (!std::is_move_constructible_v<T>) {
				return DevValueOperationStatus::Unsupported;
			} else {
				try {
					std::construct_at(
						static_cast<T*>(destination), std::move(*static_cast<T*>(source)));
					return DevValueOperationStatus::Success;
				} catch (...) {
					return DevValueOperationStatus::Failed;
				}
			}
		}

		static void destroy(void* value) noexcept {
			if (value != nullptr) std::destroy_at(static_cast<T*>(value));
		}

		static std::string_view textView(const void* value) noexcept {
			if (value == nullptr) return {};
			if constexpr (hasAssignableText) {
				return *static_cast<const T*>(value);
			} else {
				return {};
			}
		}

		static bool optionalHasValue(const void* value) noexcept {
			if constexpr (schema_detail::IsOptional<T>::value) {
				return value != nullptr && static_cast<const T*>(value)->has_value();
			} else {
				return false;
			}
		}

		static const void* optionalValueAddress(const void* value) noexcept {
			if constexpr (schema_detail::IsOptional<T>::value) {
				if (value == nullptr || !static_cast<const T*>(value)->has_value()) return nullptr;
				return std::addressof(static_cast<const T*>(value)->value());
			} else {
				return nullptr;
			}
		}

		static void* optionalMutableValueAddress(void* value) noexcept {
			if constexpr (schema_detail::IsOptional<T>::value) {
				if (value == nullptr || !static_cast<T*>(value)->has_value()) return nullptr;
				return std::addressof(static_cast<T*>(value)->value());
			} else {
				return nullptr;
			}
		}

		static std::size_t sequenceSize(const void* value) noexcept {
			if constexpr (isSequence) {
				return value == nullptr ? 0 : static_cast<const T*>(value)->size();
			} else {
				return 0;
			}
		}

		static const void* sequenceElementAddress(
			const void* value,
			std::size_t index) noexcept {
			if constexpr (hasAddressableSequenceElements) {
				if (value == nullptr || index >= static_cast<const T*>(value)->size()) return nullptr;
				return std::addressof((*static_cast<const T*>(value))[index]);
			} else {
				return nullptr;
			}
		}

		static void* sequenceMutableElementAddress(
			void* value,
			std::size_t index) noexcept {
			if constexpr (hasAddressableSequenceElements) {
				if (value == nullptr || index >= static_cast<T*>(value)->size()) return nullptr;
				return std::addressof((*static_cast<T*>(value))[index]);
			} else {
				return nullptr;
			}
		}

		static DevValueOperationStatus sequenceAppendDefault(void* value) noexcept {
			if (value == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (schema_detail::IsVector<T>::value &&
				!schema_detail::IsVectorBool<T>::value) {
				using Value = typename schema_detail::IsVector<T>::Value;
				if constexpr (std::is_default_constructible_v<Value>) {
					try {
						static_cast<T*>(value)->emplace_back();
						return DevValueOperationStatus::Success;
					} catch (...) { return DevValueOperationStatus::Failed; }
				}
			}
			return DevValueOperationStatus::Unsupported;
		}

		static DevValueOperationStatus sequenceErase(
			void* value, std::size_t index) noexcept {
			if (value == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (schema_detail::IsVector<T>::value &&
				!schema_detail::IsVectorBool<T>::value) {
				try {
					auto& sequence = *static_cast<T*>(value);
					if (index >= sequence.size()) return DevValueOperationStatus::Failed;
					sequence.erase(sequence.begin() + static_cast<std::ptrdiff_t>(index));
					return DevValueOperationStatus::Success;
				} catch (...) { return DevValueOperationStatus::Failed; }
			} else { return DevValueOperationStatus::Unsupported; }
		}

		static DevValueOperationStatus sequenceMove(
			void* value, std::size_t from, std::size_t to) noexcept {
			if (value == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (schema_detail::IsVector<T>::value &&
				!schema_detail::IsVectorBool<T>::value) {
				try {
					auto& sequence = *static_cast<T*>(value);
					if (from >= sequence.size() || to >= sequence.size()) return DevValueOperationStatus::Failed;
					if (from < to) std::rotate(sequence.begin() + from, sequence.begin() + from + 1, sequence.begin() + to + 1);
					else if (to < from) std::rotate(sequence.begin() + to, sequence.begin() + from, sequence.begin() + from + 1);
					return DevValueOperationStatus::Success;
				} catch (...) { return DevValueOperationStatus::Failed; }
			} else { return DevValueOperationStatus::Unsupported; }
		}

		static bool numericValue(const void* value, long double& result) noexcept {
			if (value == nullptr) return false;
			if constexpr (std::is_arithmetic_v<T>) {
				result = static_cast<long double>(*static_cast<const T*>(value));
				return true;
			} else if constexpr (std::is_enum_v<T>) {
				using Underlying = std::underlying_type_t<T>;
				result = static_cast<long double>(
					static_cast<Underlying>(*static_cast<const T*>(value)));
				return true;
			} else {
				return false;
			}
		}

		static DevValueOperationStatus assignNumericValue(
			void* value,
			long double candidate) noexcept {
			if (value == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (std::is_arithmetic_v<T>) {
				if (!std::isfinite(candidate)) return DevValueOperationStatus::Failed;
				if (candidate < static_cast<long double>(std::numeric_limits<T>::lowest()) ||
					candidate > static_cast<long double>(std::numeric_limits<T>::max())) {
					return DevValueOperationStatus::Failed;
				}
				if constexpr (std::is_integral_v<T>) {
					if (std::trunc(candidate) != candidate) return DevValueOperationStatus::Failed;
				}
				*static_cast<T*>(value) = static_cast<T>(candidate);
				return DevValueOperationStatus::Success;
			} else if constexpr (std::is_enum_v<T>) {
				using Underlying = std::underlying_type_t<T>;
				if (!std::isfinite(candidate) || std::trunc(candidate) != candidate ||
					candidate < static_cast<long double>(std::numeric_limits<Underlying>::lowest()) ||
					candidate > static_cast<long double>(std::numeric_limits<Underlying>::max())) {
					return DevValueOperationStatus::Failed;
				}
				*static_cast<T*>(value) = static_cast<T>(static_cast<Underlying>(candidate));
				return DevValueOperationStatus::Success;
			} else {
				return DevValueOperationStatus::Unsupported;
			}
		}

		static DevValueOperationStatus assignTextValue(
			void* value,
			std::string_view candidate) noexcept {
			if (value == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (hasAssignableText) {
				try {
					static_cast<T*>(value)->assign(candidate);
					return DevValueOperationStatus::Success;
				} catch (...) {
					return DevValueOperationStatus::Failed;
				}
			} else {
				return DevValueOperationStatus::Unsupported;
			}
		}

		static DevValueOperationStatus setOptionalPresence(
			void* value,
			bool present) noexcept {
			if (value == nullptr) return DevValueOperationStatus::NullDestination;
			if constexpr (schema_detail::IsOptional<T>::value) {
				try {
					T& optional = *static_cast<T*>(value);
					if (present && !optional.has_value()) optional.emplace();
					if (!present) optional.reset();
					return DevValueOperationStatus::Success;
				} catch (...) {
					return DevValueOperationStatus::Failed;
				}
			} else {
				return DevValueOperationStatus::Unsupported;
			}
		}

		static constexpr DevTypeOps operations{
			.type = ::FlowUi::detail::typeHash<T>(),
			.copyConstruct = &copyConstruct,
			.moveConstruct = &moveConstruct,
			.copyAssign = &copyAssign,
			.destroy = &destroy,
			.textView = schema_detail::isString<T> ? &textView : nullptr,
			.optionalHasValue = schema_detail::IsOptional<T>::value ? &optionalHasValue : nullptr,
			.optionalValueAddress = schema_detail::IsOptional<T>::value
				? &optionalValueAddress : nullptr,
			.optionalMutableValueAddress = schema_detail::IsOptional<T>::value
				? &optionalMutableValueAddress : nullptr,
			.sequenceSize = isSequence ? &sequenceSize : nullptr,
			.sequenceElementAddress =
				hasAddressableSequenceElements ? &sequenceElementAddress : nullptr,
			.sequenceMutableElementAddress =
				hasAddressableSequenceElements ? &sequenceMutableElementAddress : nullptr,
			.sequenceAppendDefault = schema_detail::IsVector<T>::value
				? &sequenceAppendDefault : nullptr,
			.sequenceErase = schema_detail::IsVector<T>::value ? &sequenceErase : nullptr,
			.sequenceMove = schema_detail::IsVector<T>::value ? &sequenceMove : nullptr,
			.numericValue = (std::is_arithmetic_v<T> || std::is_enum_v<T>)
				? &numericValue : nullptr,
			.assignNumericValue = (std::is_arithmetic_v<T> || std::is_enum_v<T>)
				? &assignNumericValue : nullptr,
			.assignTextValue = hasAssignableText ? &assignTextValue : nullptr,
			.setOptionalPresence = schema_detail::IsOptional<T>::value
				? &setOptionalPresence : nullptr,
		};
	};

	template <typename T>
	static void ingestStructRoot(DevSchemaRegistry& registry) {
		(void)registry.resolveType<T>(0, true);
	}

	template <typename T>
	static void ingestThemeRoot(DevSchemaRegistry& registry) {
		const DevTypeId type = registry.resolveType<T>(0, true);
		if (type == 0) return;
		const auto duplicate = std::find_if(
			registry.mutableThemes_.begin(), registry.mutableThemes_.end(),
			[type](const MutableTheme& theme) { return theme.themeType == type; });
		if (duplicate == registry.mutableThemes_.end()) {
			registry.mutableThemes_.push_back(MutableTheme{
				.themeType = type,
				.displayName = registry.displayNameForType(type),
			});
		}
	}

	template <FlowElement Element>
	static void ingestElementRoot(DevSchemaRegistry& registry) {
		using E = std::remove_cvref_t<Element>;
		using Parameters = ParametersOf<E>;
		const DevTypeId definitionType = registry.resolveType<E>(0, false);
		const DevTypeId parametersType = registry.resolveType<Parameters>(0, true);
		DevTypeId stateType = 0;
		DevTypeId resourcesType = 0;
		if constexpr (HasState<E>) stateType = registry.resolveType<StateOf<E>>(0, true);
		if constexpr (HasResources<E>) resourcesType = registry.resolveType<ResourcesOf<E>>(0, true);
		if (definitionType == 0 || parametersType == 0 ||
			(HasState<E> && stateType == 0) ||
			(HasResources<E> && resourcesType == 0)) return;

		const auto existing = std::find_if(
			registry.mutableElements_.begin(), registry.mutableElements_.end(),
			[](const MutableElement& element) {
				return element.definitionId == E::definitionId;
			});
		if (existing != registry.mutableElements_.end()) {
			if (existing->definitionType != definitionType ||
				existing->parametersType != parametersType ||
				existing->stateType != stateType ||
				existing->resourcesType != resourcesType) {
				registry.addDiagnostic(
					DevDiagnosticSeverity::Error,
					DevDiagnosticCode::ElementDefinitionConflict,
					definitionType,
					0,
					"Two element types claim one FlowDefinitionID.");
			}
			return;
		}

		std::string displayName;
		if constexpr (::FlowUi::detail::element::debugName<E>().empty()) {
			displayName = std::string(::FlowUi::detail::typeToken<E>());
		} else {
			displayName = std::string(::FlowUi::detail::element::debugName<E>());
		}
		registry.mutableElements_.push_back(MutableElement{
			.definitionId = E::definitionId,
			.definitionType = definitionType,
			.parametersType = parametersType,
			.stateType = stateType,
			.resourcesType = resourcesType,
			.displayName = std::move(displayName),
		});
	}

	template <typename T>
	DevTypeId resolveType(std::uint16_t depth, bool diagnoseMissingSchema) {
		using Type = std::remove_cvref_t<T>;
		const DevTypeId id = ::FlowUi::detail::typeHash<Type>();
		const std::string_view cppName = ::FlowUi::detail::cppTypeName<Type>();
		if (const auto found = mutableTypeIndex_.find(id); found != mutableTypeIndex_.end()) {
			MutableType& existing = mutableTypes_[found->second];
			if (existing.cppTypeName != cppName) {
				addDiagnostic(DevDiagnosticSeverity::Error,
					DevDiagnosticCode::TypeIdentityCollision, id, 0,
					"Two complete C++ type tokens produced the same DevTypeId.");
				existing.state = ResolutionState::Failed;
			}
			if (existing.state == ResolutionState::Visiting) {
				addDiagnostic(DevDiagnosticSeverity::Warning,
					DevDiagnosticCode::RecursiveCycle, id, 0,
					"Recursive schema traversal stopped at an already-visiting type.");
			}
			return id;
		}

		if (depth > limits_.maxDepth || mutableTypes_.size() >= limits_.maxTypes) {
			addDiagnostic(DevDiagnosticSeverity::Error,
				DevDiagnosticCode::CapacityExceeded, id, 0,
				"Schema type/depth capacity was exceeded.");
			return 0;
		}

		const std::size_t index = mutableTypes_.size();
		mutableTypeIndex_.emplace(id, index);
		mutableTypes_.push_back(MutableType{
			.id = id,
			.displayName = std::string(cppName),
			.cppTypeName = std::string(cppName),
			.size = static_cast<std::uint32_t>(sizeof(Type)),
			.alignment = static_cast<std::uint32_t>(alignof(Type)),
			.operations = &TypeOperations<Type>::operations,
		});

		// Recursive calls may reallocate mutableTypes_; never retain this reference.
		if constexpr (std::is_same_v<Type, NoElementParameters>) {
			configureLeaf(index, DevTypeKind::Object, DevEditorKind::ObjectGroup,
				DevCaptureCapability::Value, DevEditCapability::Editable);
		} else if constexpr (std::is_same_v<Type, bool>) {
			configureLeaf(index, DevTypeKind::Boolean, DevEditorKind::Toggle,
				DevCaptureCapability::Value, DevEditCapability::Editable);
		} else if constexpr (std::is_integral_v<Type> && std::is_signed_v<Type>) {
			configureLeaf(index, DevTypeKind::SignedInteger, DevEditorKind::SignedNumber,
				DevCaptureCapability::Value, DevEditCapability::Editable);
		} else if constexpr (std::is_integral_v<Type>) {
			configureLeaf(index, DevTypeKind::UnsignedInteger, DevEditorKind::UnsignedNumber,
				DevCaptureCapability::Value, DevEditCapability::Editable);
		} else if constexpr (std::is_floating_point_v<Type>) {
			configureLeaf(index, DevTypeKind::FloatingPoint, DevEditorKind::FloatingNumber,
				DevCaptureCapability::Value, DevEditCapability::Editable);
		} else if constexpr (schema_detail::isString<Type>) {
			if constexpr (TypeOperations<Type>::hasAssignableText) {
				configureLeaf(index, DevTypeKind::Text, DevEditorKind::Text,
					DevCaptureCapability::Value, DevEditCapability::Editable);
			} else {
				configureLeaf(index, DevTypeKind::Text, DevEditorKind::Text,
					DevCaptureCapability::Value, DevEditCapability::ViewOnly,
					DevCapabilityReason::NoEditAdapter);
			}
		} else if constexpr (std::is_enum_v<Type>) {
			resolveEnum<Type>(index);
		} else if constexpr (schema_detail::IsOptional<Type>::value) {
			using Value = typename schema_detail::IsOptional<Type>::Value;
			const DevTypeId child = resolveType<Value>(depth + 1, true);
			configureCompound(index, DevTypeKind::Optional, DevEditorKind::OptionalGroup, child);
		} else if constexpr (schema_detail::IsVector<Type>::value) {
			using Value = typename schema_detail::IsVector<Type>::Value;
			const DevTypeId child = resolveType<Value>(depth + 1, true);
			configureCompound(index, DevTypeKind::Sequence, DevEditorKind::Sequence, child);
		} else if constexpr (schema_detail::IsArray<Type>::value) {
			using Value = typename schema_detail::IsArray<Type>::Value;
			const DevTypeId child = resolveType<Value>(depth + 1, true);
			configureCompound(
				index, DevTypeKind::Sequence, DevEditorKind::Sequence, child,
				static_cast<std::uint32_t>(schema_detail::IsArray<Type>::extent), true);
		} else if constexpr (
			(std::is_pointer_v<Type> && std::is_function_v<std::remove_pointer_t<Type>>) ||
			std::is_member_function_pointer_v<Type>) {
			configureLeaf(index, DevTypeKind::Opaque, DevEditorKind::None,
				DevCaptureCapability::MetadataOnly, DevEditCapability::Unsupported,
				DevCapabilityReason::CallableType);
		} else if constexpr (std::is_pointer_v<Type>) {
			configureLeaf(index, DevTypeKind::Pointer, DevEditorKind::None,
				DevCaptureCapability::MetadataOnly, DevEditCapability::Unsupported,
				DevCapabilityReason::RawPointer);
		} else if constexpr (schema_detail::hasDeclaredSchema<Type>) {
			resolveDeclaredObject<Type>(index, depth);
		} else {
			configureLeaf(index, DevTypeKind::Opaque, DevEditorKind::None,
				DevCaptureCapability::MetadataOnly, DevEditCapability::Unsupported,
				DevCapabilityReason::NoSchemaDeclaration);
			if (diagnoseMissingSchema) {
				addDiagnostic(DevDiagnosticSeverity::Warning,
					DevDiagnosticCode::MissingStructSchema, id, 0,
					"A reachable compound C++ type has no FlowUi DEV schema declaration.");
			}
		}
		return id;
	}

	template <typename Enum>
	void resolveEnum(std::size_t typeIndex) {
		using Underlying = std::underlying_type_t<Enum>;
		using Unsigned = std::make_unsigned_t<Underlying>;
		MutableType& type = mutableTypes_[typeIndex];
		type.kind = DevTypeKind::Enumeration;
		type.capture = DevCaptureCapability::Value;
		type.enumWidthBytes = static_cast<std::uint8_t>(sizeof(Underlying));
		type.enumIsSigned = std::is_signed_v<Underlying>;
		if constexpr (!schema_detail::hasDeclaredEnumSchema<Enum>) {
			type.editor = DevEditorKind::None;
			type.edit = DevEditCapability::ViewOnly;
			type.reason = DevCapabilityReason::NoEditAdapter;
			type.state = ResolutionState::Complete;
		} else {
			constexpr auto descriptor = schema_detail::declaredEnumSchema<Enum>();
			type.displayName = std::string(descriptor.name);
			type.editor = DevEditorKind::EnumChoice;
			type.edit = DevEditCapability::Editable;
			std::apply([&](const auto&... value) {
				(appendEnumValue(
					type,
					value.name,
					static_cast<std::uint64_t>(static_cast<Unsigned>(value.value))), ...);
			}, descriptor.values);
			if (type.state != ResolutionState::Failed) {
				type.state = ResolutionState::Complete;
			}
		}
	}

	void appendEnumValue(MutableType& type, std::string_view name, std::uint64_t bits) {
		if (retainedEnumValueCount_ >= limits_.maxEnumValues) {
			addDiagnostic(DevDiagnosticSeverity::Error, DevDiagnosticCode::CapacityExceeded,
				type.id, 0, "Schema enum-value capacity was exceeded.");
			type.edit = DevEditCapability::ViewOnly;
			type.reason = DevCapabilityReason::CapacityExceeded;
			return;
		}
		if (std::any_of(type.enumValues.begin(), type.enumValues.end(),
			[&](const MutableType::EnumValue& value) { return value.name == name; })) {
			addDiagnostic(DevDiagnosticSeverity::Error, DevDiagnosticCode::DuplicateEnumName,
				type.id, 0, "A DEV enum schema contains a duplicate value name.");
			type.state = ResolutionState::Failed;
			return;
		}
		if (std::any_of(type.enumValues.begin(), type.enumValues.end(),
			[&](const MutableType::EnumValue& value) { return value.bits == bits; })) {
			addDiagnostic(DevDiagnosticSeverity::Error, DevDiagnosticCode::DuplicateEnumValue,
				type.id, 0, "A DEV enum schema contains a duplicate numeric value.");
			type.state = ResolutionState::Failed;
			return;
		}
		type.enumValues.push_back(MutableType::EnumValue{std::string(name), bits});
		++retainedEnumValueCount_;
	}

	template <typename Type>
	void resolveDeclaredObject(std::size_t typeIndex, std::uint16_t depth) {
		constexpr auto descriptor = schema_detail::declaredSchema<Type>();
		mutableTypes_[typeIndex].displayName = std::string(descriptor.name);
		mutableTypes_[typeIndex].kind = DevTypeKind::Object;
		mutableTypes_[typeIndex].editor = descriptor.editor;
		mutableTypes_[typeIndex].capture = descriptor.capture;
		mutableTypes_[typeIndex].edit = descriptor.edit;
		mutableTypes_[typeIndex].reason = descriptor.reason;

		std::vector<MutableField> fields;
		fields.reserve(std::tuple_size_v<decltype(descriptor.fields)>);
		std::uint32_t order = 0;
		std::apply([&](const auto&... field) {
			(appendDeclaredField<Type>(fields, field, order++, depth), ...);
		}, descriptor.fields);

		bool anyEditable = false;
		bool anyUnavailable = false;
		for (const MutableField& field : fields) {
			anyEditable |= field.effectiveEdit == DevEditCapability::Editable;
			anyUnavailable |= field.effectiveEdit != DevEditCapability::Editable &&
				field.effectiveEdit != DevEditCapability::Hidden;
		}
		const bool hasFields = !fields.empty();
		MutableType& type = mutableTypes_[typeIndex];
		type.fields = std::move(fields);
		if (hasFields && type.edit == DevEditCapability::Editable) {
			if (anyEditable && anyUnavailable) type.edit = DevEditCapability::PartiallyEditable;
			else if (!anyEditable && anyUnavailable) type.edit = DevEditCapability::ViewOnly;
		}
		type.state = ResolutionState::Complete;
	}

	template <typename Owner, auto Member>
	void appendDeclaredField(
		std::vector<MutableField>& destination,
		const DevStaticFieldDescriptor<Member>& field,
		std::uint32_t order,
		std::uint16_t depth) {
		using Traits = MemberPointerTraits<decltype(Member)>;
		using FieldOwner = typename Traits::Owner;
		using Value = std::remove_cvref_t<typename Traits::Value>;
		static_assert(std::is_same_v<Owner, FieldOwner>,
			"A FlowUi DEV schema field belongs to a different owner type.");
		const DevTypeId ownerId = ::FlowUi::detail::typeHash<Owner>();
		const DevTypeId valueId = resolveType<Value>(depth + 1, true);
		if (valueId == 0 || retainedFieldCount_ >= limits_.maxFields) {
			addDiagnostic(DevDiagnosticSeverity::Error,
				DevDiagnosticCode::CapacityExceeded, ownerId, 0,
				"Schema field capacity was exceeded while resolving a compound type.");
			return;
		}
		const DevFieldId fieldId = fieldIdentity(ownerId, field.name);
		const auto duplicate = std::find_if(destination.begin(), destination.end(),
			[&](const MutableField& existing) { return existing.name == field.name; });
		if (duplicate != destination.end()) {
			addDiagnostic(DevDiagnosticSeverity::Error,
				DevDiagnosticCode::DuplicateFieldName, ownerId, fieldId,
				"A struct schema contains duplicate field names.");
			return;
		}

		DevEditCapability effective = DevEditCapability::Unsupported;
		DevCapabilityReason reason = DevCapabilityReason::NoEditAdapter;
		if (field.options.access == DevFieldAccess::Hidden) {
			effective = DevEditCapability::Hidden;
			reason = DevCapabilityReason::None;
		} else if (field.options.access == DevFieldAccess::ReadOnly) {
			effective = DevEditCapability::ViewOnly;
			reason = DevCapabilityReason::FieldMarkedReadOnly;
		} else if (const MutableType* valueType = mutableType(valueId)) {
			effective = valueType->edit;
			reason = valueType->reason;
		}

		DevConstraintRecord constraint{};
		constraint.numeric = field.options.numeric;
		constraint.textMaximum = field.options.textMaximum;
		constraint.hasTextMaximum = field.options.hasTextMaximum;
		const bool hasNumericConstraint = field.options.numeric.hasMinimum ||
			field.options.numeric.hasMaximum || field.options.numeric.hasStep;
		const bool constraintMismatch =
			(hasNumericConstraint && !(std::is_arithmetic_v<Value> && !std::is_same_v<Value, bool>)) ||
			(field.options.hasTextMaximum && !schema_detail::isString<Value>);
		bool retainConstraint = hasNumericConstraint || field.options.hasTextMaximum;
		if (constraintMismatch) {
			addDiagnostic(DevDiagnosticSeverity::Error,
				DevDiagnosticCode::ConstraintMismatch, ownerId, fieldId,
				"A field constraint is incompatible with the declared member type.");
			effective = DevEditCapability::ViewOnly;
			reason = DevCapabilityReason::ConstraintMismatch;
			retainConstraint = false;
		} else if (retainConstraint && retainedConstraintCount_ >= limits_.maxConstraints) {
			addDiagnostic(DevDiagnosticSeverity::Error,
				DevDiagnosticCode::CapacityExceeded, ownerId, fieldId,
				"Schema constraint capacity was exceeded.");
			effective = DevEditCapability::ViewOnly;
			reason = DevCapabilityReason::CapacityExceeded;
			retainConstraint = false;
		} else if (retainConstraint) {
			++retainedConstraintCount_;
		}
		destination.push_back(MutableField{
			.id = fieldId,
			.name = std::string(field.name),
			.displayName = std::string(field.name),
			.hint = std::string(field.options.hint),
			.ownerType = ownerId,
			.valueType = valueId,
			.declaredAccess = field.options.access,
			.editor = field.options.editor == DevEditorKind::None
				? (mutableType(valueId) ? mutableType(valueId)->editor : DevEditorKind::None)
				: field.options.editor,
			.effectiveEdit = effective,
			.reason = reason,
			.constraint = constraint,
			.hasConstraint = retainConstraint,
			.operations = &DevMemberOps<Member>::operations,
			.declarationOrder = order,
			.sourceFile = std::string(field.sourceFile),
			.sourceFunction = std::string(field.sourceFunction),
			.sourceLine = field.sourceLine,
			.sourceColumn = field.sourceColumn,
		});
		++retainedFieldCount_;
	}

	void queueRoot(std::uint64_t key, PendingRoot root);
	static std::uint64_t rootKey(DevRootKind kind, std::uint64_t identity) noexcept;
	static DevFieldId fieldIdentity(DevTypeId owner, std::string_view name) noexcept;
	void configureLeaf(
		std::size_t index,
		DevTypeKind kind,
		DevEditorKind editor,
		DevCaptureCapability capture,
		DevEditCapability edit,
		DevCapabilityReason reason = DevCapabilityReason::None);
	void configureCompound(
		std::size_t index,
		DevTypeKind kind,
		DevEditorKind editor,
		DevTypeId elementType,
		std::uint32_t sequenceExtent = 0,
		bool sequenceFixed = false);
	void addDiagnostic(
		DevDiagnosticSeverity severity,
		DevDiagnosticCode code,
		DevTypeId type,
		DevFieldId field,
		std::string_view message);
	[[nodiscard]] MutableType* mutableType(DevTypeId id) noexcept;
	[[nodiscard]] const MutableType* mutableType(DevTypeId id) const noexcept;
	[[nodiscard]] std::string displayNameForType(DevTypeId id) const;
	[[nodiscard]] std::shared_ptr<const DevSchemaGeneration> buildGeneration();

	DevSchemaLimits limits_{};
	mutable std::mutex mutex_{};
	std::unordered_set<std::uint64_t> queuedOrIngestedRoots_{};
	std::vector<PendingRoot> pendingRoots_{};
	std::vector<MutableType> mutableTypes_{};
	std::unordered_map<DevTypeId, std::size_t> mutableTypeIndex_{};
	std::vector<MutableElement> mutableElements_{};
	std::vector<MutableTheme> mutableThemes_{};
	std::vector<MutableDiagnostic> mutableDiagnostics_{};
	std::uint32_t retainedFieldCount_ = 0;
	std::uint32_t retainedEnumValueCount_ = 0;
	std::uint32_t retainedConstraintCount_ = 0;
	std::shared_ptr<const DevSchemaGeneration> published_{};
	DevSchemaGenerationId nextGeneration_ = 1;
};

} // namespace FlowUi::devMode

#endif
