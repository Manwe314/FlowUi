#pragma once

#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "internal/StorageSystem/AlignedRecord.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"

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

	[[nodiscard]] explicit operator bool() const noexcept {
		return header != nullptr && payload != nullptr;
	}
};

template <typename Header, typename Payload>
struct ConstTypedPersistentRecordView {
	const Header* header = nullptr;
	const Payload* payload = nullptr;

	[[nodiscard]] explicit operator bool() const noexcept {
		return header != nullptr && payload != nullptr;
	}
};

template <typename Header, typename Payload, typename HeaderInitializer, typename... PayloadArgs>
[[nodiscard]] PersistentRecordHandle createTypedPersistentRecord(
	IStorageSystem& storage,
	PersistentRecordCreateInfo info,
	HeaderInitializer&& initializeHeader,
	PayloadArgs&&... payloadArgs) {
	static_assert(std::is_nothrow_destructible_v<Header>);
	static_assert(std::is_nothrow_destructible_v<Payload>);
	using Initializer = std::decay_t<HeaderInitializer>;
	using Arguments = std::tuple<std::decay_t<PayloadArgs>...>;
	struct ConstructionContext {
		Initializer initializeHeader;
		Arguments payloadArguments;
		AlignedRecordLayout layout;
	};

	ConstructionContext context{
		.initializeHeader = std::forward<HeaderInitializer>(initializeHeader),
		.payloadArguments = Arguments(std::forward<PayloadArgs>(payloadArgs)...),
		.layout = makeAlignedRecordLayout<Header, Payload>(),
	};

	return storage.createPersistentRecord(PersistentRecordDesc{
		.kind = info.kind,
		.window = info.window,
		.headerBytes = sizeof(Header),
		.headerAlignment = alignof(Header),
		.payloadBytes = sizeof(Payload),
		.payloadAlignment = alignof(Payload),
		.debugName = info.debugName,
		.construct = +[](void* headerMemory, void*, void* userData) {
			auto& values = *static_cast<ConstructionContext*>(userData);
			Header* header = std::apply(
				[&](auto&&... unpacked) {
					return constructAlignedRecord<Header, Payload>(
						headerMemory,
						values.layout,
						std::forward<decltype(unpacked)>(unpacked)...);
				},
				std::move(values.payloadArguments));
			try {
				std::invoke(values.initializeHeader, *header, values.layout);
			} catch (...) {
				destroyAlignedRecord<Header, Payload>(headerMemory, values.layout);
				throw;
			}
		},
		.destroy = +[](void* headerMemory, void*) noexcept {
			destroyAlignedRecord<Header, Payload>(
				headerMemory, makeAlignedRecordLayout<Header, Payload>());
		},
		.userData = &context,
	});
}

template <typename Header, typename Payload>
[[nodiscard]] TypedPersistentRecordView<Header, Payload> typedPersistentRecord(
	IStorageSystem& storage,
	PersistentRecordHandle handle,
	ResourceKind kind) noexcept {
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

template <typename Header, typename Payload>
[[nodiscard]] ConstTypedPersistentRecordView<Header, Payload> typedPersistentRecord(
	const IStorageSystem& storage,
	PersistentRecordHandle handle,
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
