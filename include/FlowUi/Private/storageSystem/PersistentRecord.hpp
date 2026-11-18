#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "FlowUi/Private/storageSystem/AlignedRecord.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"

namespace FlowUi::detail::storage {

struct PersistentRecordCreateInfo {
	ResourceKind kind = ResourceKind::Invalid;
	WindowId window = InvalidWindowId;
	StringId debugName = 0;
};

template <typename Header, typename Payload>
struct TypedPersistentRecordView {
	Header* header = nullptr;
	Payload* payload = nullptr;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return header != nullptr && payload != nullptr;
	}
};

template <typename Header, typename Payload>
struct ConstTypedPersistentRecordView {
	const Header* header = nullptr;
	const Payload* payload = nullptr;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return header != nullptr && payload != nullptr;
	}
};

template <typename Header, typename Payload, typename Storage, typename HeaderInitializer,
		  typename... PayloadArgs>
	requires requires(Storage& storage, const PersistentRecordDesc& desc) {
		{ storage.createPersistentRecord(desc) } -> std::same_as<PersistentRecordHandle>;
	}
[[nodiscard]] PersistentRecordHandle
createTypedPersistentRecord(Storage& storage, PersistentRecordCreateInfo info,
							HeaderInitializer&& initializeHeader, PayloadArgs&&... payloadArgs) {
	static_assert(std::is_nothrow_destructible_v<Header>);
	static_assert(std::is_nothrow_destructible_v<Payload>);
	using Initializer = std::decay_t<HeaderInitializer>;
	using Arguments = std::tuple<std::decay_t<PayloadArgs>...>;
	struct ConstructionContext {
		Initializer initializeHeader;
		Arguments payloadArguments;
	};

	ConstructionContext context{
		.initializeHeader = std::forward<HeaderInitializer>(initializeHeader),
		.payloadArguments = Arguments(std::forward<PayloadArgs>(payloadArgs)...),
	};

	return storage.createPersistentRecord(PersistentRecordDesc{
		.kind = info.kind,
		.window = info.window,
		.headerBytes = typedRecordLayout<Header, Payload>.headerBytes,
		.headerAlignment = alignof(Header),
		.payloadBytes = typedRecordLayout<Header, Payload>.payloadBytes,
		.payloadAlignment = typedRecordLayout<Header, Payload>.payloadAlignment,
		.debugName = info.debugName,
		.construct =
			+[](void* headerMemory, void*, void* userData) {
				auto& values = *static_cast<ConstructionContext*>(userData);
				Header* header = std::apply(
					[&](auto&&... unpacked) {
						return constructAlignedRecord<Header, Payload>(
							headerMemory, typedRecordLayout<Header, Payload>,
							std::forward<decltype(unpacked)>(unpacked)...);
					},
					std::move(values.payloadArguments));
				try {
					std::invoke(values.initializeHeader, *header,
								typedRecordLayout<Header, Payload>);
				} catch (...) {
					destroyAlignedRecord<Header, Payload>(headerMemory,
														  typedRecordLayout<Header, Payload>);
					throw;
				}
			},
		.destroy =
			+[](void* headerMemory, void*) noexcept {
				destroyAlignedRecord<Header, Payload>(headerMemory,
													  typedRecordLayout<Header, Payload>);
			},
		.userData = &context,
	});
}

template <typename Header, typename Payload, typename Storage>
	requires requires(Storage& storage, PersistentRecordHandle handle, ResourceKind kind) {
		{ storage.persistentRecord(handle, kind) } noexcept -> std::same_as<PersistentRecordView>;
	}
[[nodiscard]] TypedPersistentRecordView<Header, Payload>
typedPersistentRecord(Storage& storage, PersistentRecordHandle handle, ResourceKind kind) noexcept {
	const PersistentRecordView view = storage.persistentRecord(handle, kind);
	if (!view || view.headerBytes < sizeof(Header) || view.payloadBytes < sizeof(Payload) ||
		view.payloadAlignment < alignof(Payload) ||
		reinterpret_cast<uintptr_t>(view.header) % alignof(Header) != 0 ||
		reinterpret_cast<uintptr_t>(view.payload) % alignof(Payload) != 0) {
		return {};
	}
	return {
		.header = static_cast<Header*>(view.header),
		.payload = static_cast<Payload*>(view.payload),
	};
}

template <typename Header, typename Payload, typename Storage>
	requires requires(const Storage& storage, PersistentRecordHandle handle, ResourceKind kind) {
		{
			storage.persistentRecord(handle, kind)
		} noexcept -> std::same_as<ConstPersistentRecordView>;
	}
[[nodiscard]] ConstTypedPersistentRecordView<Header, Payload>
typedPersistentRecord(const Storage& storage, PersistentRecordHandle handle,
					  ResourceKind kind) noexcept {
	const ConstPersistentRecordView view = storage.persistentRecord(handle, kind);
	if (!view || view.headerBytes < sizeof(Header) || view.payloadBytes < sizeof(Payload) ||
		view.payloadAlignment < alignof(Payload) ||
		reinterpret_cast<uintptr_t>(view.header) % alignof(Header) != 0 ||
		reinterpret_cast<uintptr_t>(view.payload) % alignof(Payload) != 0) {
		return {};
	}
	return {
		.header = static_cast<const Header*>(view.header),
		.payload = static_cast<const Payload*>(view.payload),
	};
}

} // namespace FlowUi::detail::storage
