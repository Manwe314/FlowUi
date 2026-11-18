#pragma once

#include "FlowUi/Private/storageSystem/StorageSystemSelection.hpp"

namespace FlowUi::detail::storage {

/** Borrowed concrete storage access; its owner outlives attached managers. */
class StorageBinding {
public:
	/** Attach the selected storage after successful initialization. */
	void bind(ActiveStorageSystem& storageSystem) noexcept { storageSystem_ = &storageSystem; }
	/** Detach after all borrowers have released their resources. */
	void detach() noexcept { storageSystem_ = nullptr; }
	/** Return whether an initialized storage object is attached. */
	[[nodiscard]] bool bound() const noexcept { return storageSystem_ != nullptr; }
	/** Mutable access requires bound(). */
	[[nodiscard]] ActiveStorageSystem& system() noexcept { return *storageSystem_; }
	/** Const observation requires bound(). */
	[[nodiscard]] const ActiveStorageSystem& system() const noexcept { return *storageSystem_; }

private:
	ActiveStorageSystem* storageSystem_ = nullptr;
};

} // namespace FlowUi::detail::storage
