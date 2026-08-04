#pragma once

#include <stdexcept>
#include <string>

#include "FlowUi/ResourceKey.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"

namespace FlowUi::detail::managerStorage {

enum class ResourceScope : unsigned char {
	AppShared = 0,
	WindowLocal,
};

[[nodiscard]] inline storage::ResourceDomain internalDomain(ResourceDomain domain) {
	switch (domain) {
	case ResourceDomain::Image: return storage::ResourceDomain::Image;
	case ResourceDomain::Icon: return storage::ResourceDomain::Icon;
	case ResourceDomain::Font: return storage::ResourceDomain::Font;
	case ResourceDomain::Viewport: return storage::ResourceDomain::Viewport;
	case ResourceDomain::Ui: return storage::ResourceDomain::Layout;
	case ResourceDomain::InputField: return storage::ResourceDomain::Input;
	case ResourceDomain::Development: return storage::ResourceDomain::Development;
	case ResourceDomain::Internal: return storage::ResourceDomain::Internal;
	case ResourceDomain::Auto: break;
	}
	throw std::invalid_argument("ResourceDomain::Auto must be resolved by a manager.");
}

[[nodiscard]] inline storage::ResourceKey normalizeResourceKey(
	storage::IStorageSystem& storageSystem,
	ResourceKey key,
	ResourceDomain managerDomain,
	ResourceScope scope,
	WindowId owningWindow = InvalidWindowId) {
	if (key.name.empty()) {
		throw std::invalid_argument("FlowUi resource key name must not be empty.");
	}
	const ResourceDomain resolvedDomain = key.domain == ResourceDomain::Auto ? managerDomain : key.domain;
	if (resolvedDomain != managerDomain) {
		throw std::invalid_argument("FlowUi resource key domain does not match the receiving manager.");
	}

	WindowId resolvedWindow = InvalidWindowId;
	if (scope == ResourceScope::WindowLocal) {
		if (owningWindow == InvalidWindowId) {
			throw std::logic_error("Window-local FlowUi manager has no owning WindowId.");
		}
		resolvedWindow = key.window == InvalidWindowId ? owningWindow : key.window;
		if (resolvedWindow != owningWindow) {
			throw std::invalid_argument("FlowUi resource key references a different manager window.");
		}
	}

	return storage::ResourceKey{
		.domain = internalDomain(resolvedDomain),
		.name = storageSystem.intern(key.name),
		.window = resolvedWindow,
	};
}

} // namespace FlowUi::detail::managerStorage
