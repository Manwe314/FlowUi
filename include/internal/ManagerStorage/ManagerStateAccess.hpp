#pragma once

#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "internal/StorageSystem/IStorageSystem.hpp"

namespace FlowUi::detail::manager_storage {

template <typename State, typename... Args>
storage::ManagerRecordHandle createState(
	storage::IStorageSystem& storageSystem,
	storage::ResourceKey key,
	storage::ResourceKind kind,
	storage::StringId debugName,
	Args&&... args) {
	using Arguments = std::tuple<std::decay_t<Args>...>;
	Arguments arguments(std::forward<Args>(args)...);
	return storageSystem.createManagerRecord(storage::ManagerRecordDesc{
		.key = key,
		.kind = kind,
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
	});
}

template <typename State>
State* state(
	storage::IStorageSystem* storageSystem,
	storage::ManagerRecordHandle handle,
	storage::ResourceKind kind) noexcept {
	return storageSystem
		? static_cast<State*>(storageSystem->managerRecordData(handle, kind))
		: nullptr;
}

template <typename State>
const State* state(
	const storage::IStorageSystem* storageSystem,
	storage::ManagerRecordHandle handle,
	storage::ResourceKind kind) noexcept {
	return storageSystem
		? static_cast<const State*>(storageSystem->managerRecordData(handle, kind))
		: nullptr;
}

} // namespace FlowUi::detail::manager_storage
