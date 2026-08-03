#pragma once

#include <string_view>

#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::detail {

//Transitional: this bridge disappears when manager Vulkan resources are owned
// and published directly by the storage system.
struct IUiTexturePublisher {
	virtual ~IUiTexturePublisher() = default;

	virtual TextureHandle publishExternal(
		storage::ResourceDomain domain,
		std::string_view namespacedKey,
		const storage::ExternalTextureDesc& desc,
		bool& inserted) = 0;
	virtual TextureHandle publishFallbackAlias(
		storage::ResourceDomain domain,
		std::string_view namespacedKey,
		bool& inserted) = 0;
	virtual bool remove(
		storage::ResourceDomain domain,
		std::string_view namespacedKey,
		WindowId window = InvalidWindowId) = 0;
	[[nodiscard]] virtual bool retired(TextureHandle handle) const noexcept = 0;
};

} // namespace FlowUi::detail
