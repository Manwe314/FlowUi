#pragma once

#include "FlowUi/Private/storageSystem/StorageSystemValidation.hpp"
#include "FlowUi/Public/BuildConfig.hpp"

#if FLOWUI_CONFIG_CUSTOM_STORAGE_SYSTEM
#include FLOWUI_CONFIG_STORAGE_SYSTEM_HEADER
#else
#include "FlowUi/Private/storageSystem/FlowStorageSystem.hpp"
#endif

namespace FlowUi::detail::storage {

#if FLOWUI_CONFIG_CUSTOM_STORAGE_SYSTEM
using ActiveStorageSystem = FLOWUI_CONFIG_STORAGE_SYSTEM_TYPE;
#else
using ActiveStorageSystem = FlowStorageSystem;
#endif

static_assert(validateStorageSystem<ActiveStorageSystem>());

} // namespace FlowUi::detail::storage
