#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <vector>

#include "FlowUi/TextureHandle.hpp"

namespace FlowUi::detail::storage { class IStorageSystem; }

namespace FlowUi::devMode {

class DevCatalogLease {
public:
	DevCatalogLease() = default;
	~DevCatalogLease();
	DevCatalogLease(const DevCatalogLease&) = delete;
	DevCatalogLease& operator=(const DevCatalogLease&) = delete;
	DevCatalogLease(DevCatalogLease&& other) noexcept;
	DevCatalogLease& operator=(DevCatalogLease&& other) noexcept;

	[[nodiscard]] bool isValid() const noexcept { return storage_ && !textures_.empty(); }
	[[nodiscard]] std::size_t resourceCount() const noexcept { return textures_.size(); }

private:
	friend class DevCatalogues;
	DevCatalogLease(
		detail::storage::IStorageSystem* storage,
		std::vector<TextureHandle> textures) noexcept;
	void release() noexcept;

	detail::storage::IStorageSystem* storage_ = nullptr;
	std::vector<TextureHandle> textures_{};
};

} // namespace FlowUi::devMode

#endif
