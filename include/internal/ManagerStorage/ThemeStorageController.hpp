#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/ResourceKey.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"
#include "internal/TypeOperations.hpp"

namespace FlowUi::detail::manager_storage {

struct TypeRegistrationRecord {
	uint64_t typeHash = 0;
	storage::StringId typeNameId = 0;
	storage::StringId activeVariantNameId = 0;
	std::unordered_map<storage::StringId, storage::ManagerRecordHandle> variants{};
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
	storage::ManagerRecordHandle registerThemeVariant(
		storage::StringId variantNameId,
		T themeData,
		bool makeActive) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (!storage_) {
			throw std::runtime_error("ThemeStorageController: storage system uninitialized.");
		}

		const uint64_t typeHash = detail::typeHash<T>();
		const storage::StringId typeNameId = storage_->intern(detail::typeToken<T>());
		const storage::ResourceKey key = makeThemeResourceKey(typeHash, variantNameId);

		TypeRegistrationRecord& typeRecord = typeRegistry_[typeHash];
		typeRecord.typeHash = typeHash;
		typeRecord.typeNameId = typeNameId;

		auto existingIt = typeRecord.variants.find(variantNameId);
		if (existingIt != typeRecord.variants.end()) {
			void* rawRecord = storage_->managerRecordData(existingIt->second, storage::ResourceKind::UiTheme);
			auto* header = reinterpret_cast<storage::ThemeRecordHeader*>(rawRecord);
			auto* payload = reinterpret_cast<T*>(header->payload());
			*payload = std::move(themeData);
			header->revision++;

			if (makeActive || typeRecord.activeVariantNameId == 0) {
				typeRecord.activeVariantNameId = variantNameId;
			}
			return existingIt->second;
		}

		const size_t headerSize = sizeof(storage::ThemeRecordHeader);
		const size_t align = alignof(T);
		const size_t totalBytes = headerSize + sizeof(T) + align;

		struct ContextPayload {
			T data;
			storage::StringId typeName;
			storage::StringId variantName;
			uint64_t typeHash;
		};

		auto contextData = std::make_unique<ContextPayload>(ContextPayload{
			.data = std::move(themeData),
			.typeName = typeNameId,
			.variantName = variantNameId,
			.typeHash = typeHash
		});

		storage::ManagerRecordDesc desc{};
		desc.key = key;
		desc.kind = storage::ResourceKind::UiTheme;
		desc.bytes = totalBytes;
		desc.alignment = alignof(storage::ThemeRecordHeader);
		desc.debugName = variantNameId;
		desc.userData = contextData.get();

		desc.construct = [](void* destination, void* userData) {
			auto* ctx = reinterpret_cast<ContextPayload*>(userData);
			auto* header = new (destination) storage::ThemeRecordHeader();
			header->typeHash = ctx->typeHash;
			header->typeNameId = ctx->typeName;
			header->variantNameId = ctx->variantName;
			header->dataSize = sizeof(T);
			header->alignment = alignof(T);
			header->revision = 1;

			header->copyConstruct = [](void* dest, const void* src) {
				new (dest) T(*reinterpret_cast<const T*>(src));
			};
			header->destroy = [](void* object) noexcept {
				reinterpret_cast<T*>(object)->~T();
			};

			new (header->payload()) T(std::move(ctx->data));
		};

		desc.destroy = [](void* object) noexcept {
			auto* header = reinterpret_cast<storage::ThemeRecordHeader*>(object);
			if (header && header->destroy) {
				header->destroy(header->payload());
			}
			header->~ThemeRecordHeader();
		};

		storage::ManagerRecordHandle handle = storage_->createManagerRecord(desc);
		typeRecord.variants[variantNameId] = handle;

		if (makeActive || typeRecord.activeVariantNameId == 0) {
			typeRecord.activeVariantNameId = variantNameId;
		}

		return handle;
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

		void* rawRecord = storage_->managerRecordData(varIt->second, storage::ResourceKind::UiTheme);
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

		void* rawRecord = storage_->managerRecordData(varIt->second, storage::ResourceKind::UiTheme);
		if (!rawRecord) return nullptr;

		const auto* header = reinterpret_cast<const storage::ThemeRecordHeader*>(rawRecord);
		return reinterpret_cast<const T*>(header->payload());
	}

	template <typename T>
	void queueThemeMutation(storage::StringId variantNameId, std::function<void(T&)> mutator) {
		std::lock_guard<std::mutex> lock(mutex_);
		const uint64_t typeHash = detail::typeHash<T>();

		stagedMutations_.push_back(StagedThemeMutation{
			.typeHash = typeHash,
			.variantNameId = variantNameId,
			.mutator = [userMutator = std::move(mutator)](void* rawPayload) {
				auto* typedPayload = reinterpret_cast<T*>(rawPayload);
				userMutator(*typedPayload);
			}
		});
	}

	template <typename T>
	void queueActiveThemeMutation(std::function<void(T&)> mutator) {
		std::lock_guard<std::mutex> lock(mutex_);
		const uint64_t typeHash = detail::typeHash<T>();

		auto it = typeRegistry_.find(typeHash);
		if (it == typeRegistry_.end() || it->second.activeVariantNameId == 0) return;

		stagedMutations_.push_back(StagedThemeMutation{
			.typeHash = typeHash,
			.variantNameId = it->second.activeVariantNameId,
			.mutator = [userMutator = std::move(mutator)](void* rawPayload) {
				auto* typedPayload = reinterpret_cast<T*>(rawPayload);
				userMutator(*typedPayload);
			}
		});
	}

	void applyStagedMutations();
	storage::StringId internString(std::string_view str);

private:
	storage::ResourceKey makeThemeResourceKey(uint64_t typeHash, storage::StringId variantNameId) const noexcept;

private:
	storage::IStorageSystem* storage_ = nullptr;
	mutable std::mutex mutex_{};
	std::unordered_map<uint64_t, TypeRegistrationRecord> typeRegistry_{};
	std::vector<StagedThemeMutation> stagedMutations_{};
};

} // namespace FlowUi::detail::manager_storage
