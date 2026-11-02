#include "managers/ThemeManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#endif
#include "internal/ManagerStorage/ThemeStorageController.hpp"

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ThemeManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (controller_) controller_->appendDevMemorySamples(sink);
}
#endif

ThemeManager::ThemeManager()
	: controller_(std::make_unique<detail::manager_storage::ThemeStorageController>()) {}

ThemeManager::~ThemeManager() {
	destroy();
}

void ThemeManager::init(detail::storage::IStorageSystem& storage) {
	storage_ = &storage;
	if (controller_) {
		controller_->init(storage);
	}
}

void ThemeManager::destroy() noexcept {
#if FLOW_UI_DEV_MODE
	dev_previews_.clear();
#endif
	if (controller_) {
		controller_->shutdown();
	}
	storage_ = nullptr;
}

void ThemeManager::applyStagedMutations() {
	if (controller_) {
		controller_->applyStagedMutations();
	}
}

#if FLOW_UI_DEV_MODE
namespace {
struct DevThemeVisitBridge {
	void* userData = nullptr;
	ThemeManager::DevThemePayloadVisitor visitor = nullptr;
};

bool visitDevThemePayload(
	void* userData,
	const detail::manager_storage::ThemeStorageController::DevPayloadView& view) noexcept {
	auto& bridge = *static_cast<DevThemeVisitBridge*>(userData);
	return bridge.visitor(bridge.userData, ThemeManager::DevThemePayloadView{
											   .type = view.type,
											   .typeName = view.typeName,
											   .variant = view.variant,
											   .payload = view.payload,
											   .authored = view.authored,
											   .startup = view.startup,
											   .payloadSize = view.payloadSize,
											   .revision = view.revision,
											   .active = view.active,
										   });
}
} // namespace

bool ThemeManager::visitDevThemePayloads(
	void* userData,
	DevThemePayloadVisitor visitor) const noexcept {
	if (!controller_ || !visitor) return false;
	DevThemeVisitBridge bridge{.userData = userData, .visitor = visitor};
	return controller_->visitDevPayloads(&bridge, &visitDevThemePayload);
}

Status ThemeManager::clone_dev_variant(devMode::DevTypeId type, std::string_view source,
									   std::string_view destination) noexcept {
	if (!controller_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeRegisterVariant));
	try {
		if (controller_->clone_dev_variant(type, source, destination))
			return {};
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(
			makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeRegisterVariant));
	}
	return unexpectedError(
		makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeRegisterVariant));
}

Status ThemeManager::set_dev_active_variant(devMode::DevTypeId type,
											std::string_view variant) noexcept {
	if (!controller_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeSetActiveVariant));
	if (!controller_->set_dev_active_variant(type, variant))
		return unexpectedError(
			makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeSetActiveVariant));
	return {};
}

std::uint64_t ThemeManager::devRevision() const noexcept {
	return storage_ ? storage_->managerSharedRevision() : 0;
}

std::size_t ThemeManager::devThemeCount() const noexcept {
	return controller_ ? controller_->devPayloadCount() : 0;
}

bool ThemeManager::visitDevCatalogueThemes(
	void* userData,
	DevThemePayloadVisitor visitor) const noexcept {
	return visitDevThemePayloads(userData, visitor);
}

devMode::DevValueOperationStatus ThemeManager::assignDevThemeField(
	devMode::DevTypeId type,
	std::string_view variant,
	std::span<const devMode::DevFieldOps* const> ownerPath,
	const devMode::DevFieldOps& field,
	const void* source) noexcept {
	if (!controller_) return devMode::DevValueOperationStatus::NullDestination;
	return controller_->assignDevField(type, variant, ownerPath, field, source);
}

Result<std::string_view> ThemeManager::retain_dev_resource_key(std::string_view key) const noexcept {
	if (!storage_)
		return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeRegisterVariant));
	try {
		const auto identity = storage_->intern(key);
		const auto retained = storage_->string(identity);
		if (retained != key)
			return unexpectedError(makeError(ErrorCode::ThemeMutationFailed, ErrorSite::ThemeRegisterVariant));
		return retained;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::ThemeRegisterVariant));
	}
}
#endif

} // namespace FlowUi
