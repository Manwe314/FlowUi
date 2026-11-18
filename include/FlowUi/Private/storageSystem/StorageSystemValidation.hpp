#pragma once

#include "FlowUi/Private/storageSystem/StorageSystemConcept.hpp"

namespace FlowUi::detail::storage {

/** Validate the selected private storage implementation at compilation. */
template <typename Storage>
[[nodiscard]] consteval bool validateStorageSystem() noexcept {
	static_assert(std::is_constructible_v<Storage, VulkanContext&>,
				  "Storage must be constructible from the app VulkanContext&.");
	static_assert(std::is_nothrow_destructible_v<Storage>, "Storage destruction must not throw.");
	static_assert(!std::is_polymorphic_v<Storage>,
				  "Selected storage must use concrete nonvirtual dispatch.");
	if constexpr (requires {
					  typename std::integral_constant<uint32_t, Storage::contractRevision>;
					  typename std::integral_constant<uint32_t, Storage::interfaceVersionValue>;
					  typename std::integral_constant<uint64_t, Storage::supportedCapabilities>;
				  }) {
		static_assert(Storage::contractRevision == storageContractRevision,
					  "Selected storage has an incompatible private contract revision.");
		static_assert(Storage::interfaceVersionValue == currentStorageInterfaceVersion,
					  "Selected storage has an incompatible behavioral interface version.");
		static_assert((Storage::supportedCapabilities & requiredStorageCapabilities) ==
						  requiredStorageCapabilities,
					  "Selected storage lacks features required by the current Vulkan core.");
	} else {
		static_assert(StorageSystemConcept<Storage>,
					  "Storage must declare constant revision, version, and capability metadata.");
	}
	static_assert(StorageSystemConcept<Storage>,
				  "Selected storage must satisfy the complete Vulkan StorageSystemConcept.");
	return StorageSystemConcept<Storage> && !std::is_polymorphic_v<Storage>;
}

} // namespace FlowUi::detail::storage
