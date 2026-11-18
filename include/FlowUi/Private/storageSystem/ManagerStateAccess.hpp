#pragma once

#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "FlowUi/Private/storageSystem/StorageBinding.hpp"

namespace FlowUi::detail::manager_storage {

/** Resolve core records directly, including the fixed manager binding. */
template <typename Storage>
[[nodiscard]] Storage& recordSystem(Storage& storageSystem) noexcept {
	return storageSystem;
}
[[nodiscard]] inline auto& recordSystem(storage::StorageBinding& binding) noexcept {
	return binding.system();
}
[[nodiscard]] inline const auto& recordSystem(const storage::StorageBinding& binding) noexcept {
	return binding.system();
}

template <typename State, typename Storage, typename... Args>
[[nodiscard]] storage::ManagerRecordHandle createState(Storage& storageSystem, storage::ResourceKey key,
										 storage::ResourceKind kind, storage::StringId debugName,
										 Args&&... args) {
	using Arguments = std::tuple<std::decay_t<Args>...>;
	Arguments arguments(std::forward<Args>(args)...);
	return storageSystem.createManagerRecord(storage::ManagerRecordDesc{
		.key = key,
		.bytes = sizeof(State),
		.alignment = alignof(State),
		.debugName = debugName,
		.construct = +[](void* destination, void* userData) {
			auto& values = *static_cast<Arguments*>(userData);
			std::apply([destination](auto&&... unpacked) {
				::new (destination) State(std::forward<decltype(unpacked)>(unpacked)...);
			}, std::move(values));
		},
		.destroy = +[](void* object) noexcept { static_cast<State*>(object)->~State(); },
		.userData = &arguments,
		.kind = kind,
	});
}

template <typename State, typename Storage>
[[nodiscard]] State* state(Storage* storageSystem, storage::ManagerRecordHandle handle,
			 storage::ResourceKind kind) noexcept {
	return storageSystem
			   ? static_cast<State*>(recordSystem(*storageSystem).managerRecordData(handle, kind))
			   : nullptr;
}

template <typename State, typename Storage>
[[nodiscard]] const State* state(const Storage* storageSystem, storage::ManagerRecordHandle handle,
				   storage::ResourceKind kind) noexcept {
	return storageSystem ? static_cast<const State*>(
							   recordSystem(*storageSystem).managerRecordData(handle, kind))
						 : nullptr;
}

} // namespace FlowUi::detail::manager_storage
