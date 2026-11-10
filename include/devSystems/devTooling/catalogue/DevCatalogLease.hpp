#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <vector>

#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"

namespace FlowUi::detail::storage { class IStorageSystem; }

namespace FlowUi::devSystems::tooling {

/** @brief Move-only retention of generation-checked inspection textures. Destruction releases the retained bindings; the backing storage owner must outlive the lease. */
class DevCatalogLease {
public:
	/** @brief Construct an empty lease that retains no textures. */
	DevCatalogLease() = default;
	/** @brief Release every retained texture binding; never destroy application-owned texture sources. */
	~DevCatalogLease();
	DevCatalogLease(const DevCatalogLease&) = delete;
	DevCatalogLease& operator=(const DevCatalogLease&) = delete;
	/** @brief Transfer retained texture bindings and leave other empty. */
	DevCatalogLease(DevCatalogLease&& other) noexcept;
	/** @brief Release existing bindings, transfer those from other, and leave other empty. */
	DevCatalogLease& operator=(DevCatalogLease&& other) noexcept;

	/** @brief Return whether this lease has backing storage and at least one retained binding. */
	[[nodiscard]] bool isValid() const noexcept { return storage_ && !textures_.empty(); }
	/** @brief Return the number of texture bindings retained by this lease. */
	[[nodiscard]] std::size_t resourceCount() const noexcept { return textures_.size(); }

private:
	friend class DevCatalogues;
	DevCatalogLease(::FlowUi::detail::storage::IStorageSystem* storage,
					std::vector<TextureHandle> textures) noexcept;
	void release() noexcept;

	::FlowUi::detail::storage::IStorageSystem* storage_ = nullptr;
	std::vector<TextureHandle> textures_{};
};

} // namespace FlowUi::devSystems::tooling

#endif
