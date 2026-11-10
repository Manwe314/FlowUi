#pragma once
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Private/storageSystem/AlignedRecord.hpp"
#include "FlowUi/Private/storageSystem/IStorageSystem.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#include "FlowUi/Private/TypeOperations.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#endif

namespace FlowUi::devSystems { class MemorySampleSink; }
namespace FlowUi::detail::manager_storage {

#if FLOW_UI_DEV_MODE
/* Typed baseline storage preserves owning fields and their alignment. */
struct DevThemeBaseline {
	virtual ~DevThemeBaseline() = default;
	[[nodiscard]] virtual const void* payload() const noexcept = 0;
};
template <typename T>
struct DevTypedThemeBaseline final : DevThemeBaseline {
	T value;
	explicit DevTypedThemeBaseline(const T& source) : value(source) {}
	[[nodiscard]] const void* payload() const noexcept override { return &value; }
};
#endif

struct ThemeVariantRegistrationRecord {
	storage::ManagerRecordHandle handle{};
	storage::ResourceKey resourceKey{};
#if FLOW_UI_DEV_MODE
	std::unique_ptr<DevThemeBaseline> authored{};
	std::unique_ptr<DevThemeBaseline> startup{};
#endif
};

class ThemeStorageController;

struct TypeRegistrationRecord {
	uint64_t typeHash = 0;
#if FLOW_UI_DEV_MODE
	storage::ManagerRecordHandle (*clone_variant)(ThemeStorageController&, storage::StringId,
												  storage::StringId) = nullptr;
#endif
	storage::StringId typeNameId = 0;
	storage::StringId activeVariantNameId = 0;
	std::unordered_map<storage::StringId, ThemeVariantRegistrationRecord> variants{};
};

struct StagedThemeMutation {
	uint64_t typeHash = 0;
	storage::StringId variantNameId = 0;
	std::function<void(void* payload)> mutator = nullptr;
};

class ThemeStorageController {
public:
	ThemeStorageController() = default;
	~ThemeStorageController() = default;

	ThemeStorageController(const ThemeStorageController&) = delete;
	ThemeStorageController& operator=(const ThemeStorageController&) = delete;

	void init(storage::IStorageSystem& storage);
	void shutdown() noexcept;

	template <typename T>
	storage::ManagerRecordHandle registerThemeVariant(storage::StringId variantNameId, T themeData,
													  bool makeActive
#if FLOW_UI_DEV_MODE
													  ,
													  const T* authored = nullptr
#endif
	) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (!storage_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeRegisterVariant));
		}

		const uint64_t typeHash = detail::typeHash<T>();
		const storage::StringId typeNameId = storage_->intern(detail::typeToken<T>());
		const storage::ResourceKey key = makeThemeResourceKey(typeHash, variantNameId);

		auto [type_iterator, new_type] = typeRegistry_.try_emplace(typeHash);
		TypeRegistrationRecord& typeRecord = type_iterator->second;
		if (new_type)
			::FlowUi::detail::reserve_initial_capacity(
				typeRecord.variants, storage_->initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::themes_variants));
		typeRecord.typeHash = typeHash;
		typeRecord.typeNameId = typeNameId;
#if FLOW_UI_DEV_MODE
		typeRecord.clone_variant = [](ThemeStorageController& controller, storage::StringId source,
									  storage::StringId destination) {
			const T* payload = controller.getThemeVariant<T>(source);
			if (!payload)
				return storage::ManagerRecordHandle{};
			return controller.registerThemeVariant<T>(destination, *payload, false);
		};
#endif

#if FLOW_UI_DEV_MODE
		auto authored_baseline =
			std::make_unique<DevTypedThemeBaseline<T>>(authored ? *authored : themeData);
		auto startup_baseline = std::make_unique<DevTypedThemeBaseline<T>>(themeData);
#endif
		auto existingIt = typeRecord.variants.find(variantNameId);
		if (existingIt != typeRecord.variants.end()) {
			void* rawRecord = storage_->managerRecordData(existingIt->second.handle, storage::ResourceKind::UiTheme);
			auto* header = reinterpret_cast<storage::ThemeRecordHeader*>(rawRecord);
			auto* payload = reinterpret_cast<T*>(header->payload());
			*payload = std::move(themeData);
#if FLOW_UI_DEV_MODE
			existingIt->second.authored = std::move(authored_baseline);
			existingIt->second.startup = std::move(startup_baseline);
#endif
			header->revision++;

			if (makeActive || typeRecord.activeVariantNameId == 0) {
				typeRecord.activeVariantNameId = variantNameId;
			}
			return existingIt->second.handle;
		}

		const storage::AlignedRecordLayout layout =
			storage::makeAlignedRecordLayout<storage::ThemeRecordHeader, T>();

		struct ContextPayload {
			T data;
			storage::StringId typeName;
			storage::StringId variantName;
			uint64_t typeHash;
			storage::AlignedRecordLayout layout;
		};

		auto contextData = std::make_unique<ContextPayload>(ContextPayload{
			.data = std::move(themeData),
			.typeName = typeNameId,
			.variantName = variantNameId,
			.typeHash = typeHash,
			.layout = layout,
		});

		storage::ManagerRecordDesc desc{};
		desc.key = key;
		desc.kind = storage::ResourceKind::UiTheme;
		desc.bytes = layout.allocationBytes;
		desc.alignment = layout.allocationAlignment;
		desc.debugName = variantNameId;
		desc.userData = contextData.get();

		desc.construct = [](void* destination, void* userData) {
			auto* ctx = reinterpret_cast<ContextPayload*>(userData);
			auto* header = storage::constructAlignedRecord<storage::ThemeRecordHeader, T>(
				destination, ctx->layout, std::move(ctx->data));
			header->typeHash = ctx->typeHash;
			header->typeNameId = ctx->typeName;
			header->variantNameId = ctx->variantName;
			header->dataSize = sizeof(T);
			header->alignment = alignof(T);
			header->payloadOffset = ctx->layout.payloadOffset;
			header->revision = 1;

			header->copyConstruct = [](void* dest, const void* src) {
				new (dest) T(*reinterpret_cast<const T*>(src));
			};
			header->destroy = [](void* object) noexcept {
				reinterpret_cast<T*>(object)->~T();
			};
		};

		desc.destroy = [](void* object) noexcept {
			auto* header = reinterpret_cast<storage::ThemeRecordHeader*>(object);
			if (header && header->destroy) {
				header->destroy(header->payload());
			}
			header->~ThemeRecordHeader();
		};

		storage::ManagerRecordHandle handle = storage_->createManagerRecord(desc);
		typeRecord.variants[variantNameId] = ThemeVariantRegistrationRecord{
			.handle = handle,
			.resourceKey = key,
#if FLOW_UI_DEV_MODE
			.authored = std::move(authored_baseline),
			.startup = std::move(startup_baseline),
#endif
		};

		if (makeActive || typeRecord.activeVariantNameId == 0) {
			typeRecord.activeVariantNameId = variantNameId;
		}

		return handle;
	}

	template <typename T>
	[[nodiscard]] bool hasThemeType() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return typeRegistry_.contains(detail::typeHash<T>());
	}

	template <typename T>
	bool setActiveVariant(storage::StringId variantNameId) {
		std::lock_guard<std::mutex> lock(mutex_);
		const uint64_t typeHash = detail::typeHash<T>();

		auto it = typeRegistry_.find(typeHash);
		if (it == typeRegistry_.end()) return false;

		auto varIt = it->second.variants.find(variantNameId);
		if (varIt == it->second.variants.end()) return false;

		it->second.activeVariantNameId = variantNameId;
		return true;
	}

	template <typename T>
	[[nodiscard]] const T* getActiveThemeVariant() const noexcept {
		std::lock_guard<std::mutex> lock(mutex_);
		const uint64_t typeHash = detail::typeHash<T>();

		auto it = typeRegistry_.find(typeHash);
		if (it == typeRegistry_.end() || it->second.activeVariantNameId == 0) return nullptr;

		auto varIt = it->second.variants.find(it->second.activeVariantNameId);
		if (varIt == it->second.variants.end()) return nullptr;

		void* rawRecord = storage_->managerRecordData(varIt->second.handle, storage::ResourceKind::UiTheme);
		if (!rawRecord) return nullptr;

		const auto* header = reinterpret_cast<const storage::ThemeRecordHeader*>(rawRecord);
		return reinterpret_cast<const T*>(header->payload());
	}

	template <typename T>
	[[nodiscard]] const T* getThemeVariant(storage::StringId variantNameId) const noexcept {
		std::lock_guard<std::mutex> lock(mutex_);
		const uint64_t typeHash = detail::typeHash<T>();

		auto it = typeRegistry_.find(typeHash);
		if (it == typeRegistry_.end()) return nullptr;

		auto varIt = it->second.variants.find(variantNameId);
		if (varIt == it->second.variants.end()) return nullptr;

		void* rawRecord = storage_->managerRecordData(varIt->second.handle, storage::ResourceKind::UiTheme);
		if (!rawRecord) return nullptr;

		const auto* header = reinterpret_cast<const storage::ThemeRecordHeader*>(rawRecord);
		return reinterpret_cast<const T*>(header->payload());
	}

	template <typename T>
	bool queueThemeMutation(storage::StringId variantNameId, std::function<void(T&)> mutator) {
		static_assert(std::is_copy_constructible_v<T> && std::is_nothrow_move_assignable_v<T>,
			"Staged FlowUi themes must support an allocation-safe rollback copy.");
		std::lock_guard<std::mutex> lock(mutex_);
		const uint64_t typeHash = detail::typeHash<T>();
		const auto typeIt = typeRegistry_.find(typeHash);
		if (typeIt == typeRegistry_.end() || !typeIt->second.variants.contains(variantNameId)) return false;

		stagedMutations_.push_back(StagedThemeMutation{
			.typeHash = typeHash,
			.variantNameId = variantNameId,
			.mutator = [userMutator = std::move(mutator)](void* rawPayload) {
				auto* typedPayload = reinterpret_cast<T*>(rawPayload);
				T rollback(*typedPayload);
				try {
					userMutator(*typedPayload);
				} catch (...) {
					*typedPayload = std::move(rollback);
					throw;
				}
			}
		});
		return true;
	}

	template <typename T>
	bool queueActiveThemeMutation(std::function<void(T&)> mutator) {
		static_assert(std::is_copy_constructible_v<T> && std::is_nothrow_move_assignable_v<T>,
			"Staged FlowUi themes must support an allocation-safe rollback copy.");
		std::lock_guard<std::mutex> lock(mutex_);
		const uint64_t typeHash = detail::typeHash<T>();

		auto it = typeRegistry_.find(typeHash);
		if (it == typeRegistry_.end() || it->second.activeVariantNameId == 0) return false;

		stagedMutations_.push_back(StagedThemeMutation{
			.typeHash = typeHash,
			.variantNameId = it->second.activeVariantNameId,
			.mutator = [userMutator = std::move(mutator)](void* rawPayload) {
				auto* typedPayload = reinterpret_cast<T*>(rawPayload);
				T rollback(*typedPayload);
				try {
					userMutator(*typedPayload);
				} catch (...) {
					*typedPayload = std::move(rollback);
					throw;
				}
			}
		});
		return true;
	}

	void applyStagedMutations();
	storage::StringId internString(std::string_view str);
#if FLOW_UI_DEV_MODE
	struct DevPayloadView {
		std::uint64_t type = 0;
		std::string_view typeName{};
		std::string_view variant{};
		const void* payload = nullptr;
		const void* authored = nullptr;
		const void* startup = nullptr;
		std::size_t payloadSize = 0;
		std::uint64_t revision = 0;
		bool active = false;
	};
	using DevPayloadVisitor = bool (*)(void*, const DevPayloadView&) noexcept;

	/* Clone a registered variant without replacing an existing destination. */
	[[nodiscard]] bool clone_dev_variant(std::uint64_t type, std::string_view source,
										 std::string_view destination);
	/* Select an existing variant through its reflected type. */
	[[nodiscard]] bool set_dev_active_variant(std::uint64_t type,
											  std::string_view variant) noexcept;
	bool visitDevPayloads(void* userData, DevPayloadVisitor visitor) const noexcept;
	[[nodiscard]] std::size_t devPayloadCount() const noexcept;
	devSystems::tooling::DevValueOperationStatus assignDevField(
		std::uint64_t type,
		std::string_view variant,
		std::span<const devSystems::tooling::DevFieldOps* const> ownerPath,
		const devSystems::tooling::DevFieldOps& field,
		const void* source) noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(::FlowUi::devSystems::MemorySampleSink& sink) const noexcept;
#endif

private:
	storage::ResourceKey makeThemeResourceKey(uint64_t typeHash, storage::StringId variantNameId) const;

private:
	storage::IStorageSystem* storage_ = nullptr;
	mutable std::mutex mutex_{};
	std::unordered_map<uint64_t, TypeRegistrationRecord> typeRegistry_{};
	std::vector<StagedThemeMutation> stagedMutations_{};
};

} // namespace FlowUi::detail::manager_storage
