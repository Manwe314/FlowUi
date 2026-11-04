#pragma once

#include "FlowUi/BuildConfig.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <span>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "devSystems/devTooling/bake/DevBakedChanges.hpp"
#include "internal/ManagerStorage/ThemeStorageController.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#endif

namespace FlowUi {

class App;
class UiManager;

namespace detail::storage { class IStorageSystem; }
namespace devSystems { class MemorySampleSink; }
#if FLOW_UI_DEV_MODE
namespace devSystems::tooling { class DevOverrideCapture; class DevOverrideEngine; }
#endif

/**
 * @defgroup flowui_theme_manager Theme Manager
 * @brief Theme struct registration, variant management, and active theme dispatch.
 * @{
 */

/**
 * @brief Manages registrable themes, named theme variants, and active theme dispatch.
 *
 * ThemeManager owns registered user and library theme structs. Applications access
 * ThemeManager via App::themes() to register theme structs, select active variants,
 * query theme values, or queue frame-boundary theme updates.
 */
class ThemeManager {
public:
#if FLOW_UI_DEV_MODE
	/** A reproducible component example. Captures own example data and local state.
	 * Draw callbacks must not register previews or trigger application operations while building.
	 * Scenes declaring uses_local_theme resolve themes exclusively through UiManager::theme(). */
	struct DevThemePreview {
		std::function<void(App&, UiManager&)> draw{};
		std::string debug_name{};
		bool uses_local_theme = false;
	};
	/** Register or replace the named preview for every variant of T (development only).
	 * Capture example state by value, or retain dependencies for the manager lifetime.
	 * Set uses_local_theme only when all nested scene theme reads use UiManager's resolver;
	 * direct App::themes() reads bypass the local scope and restrict the scene to active variants.
	 */
	template <typename T>
	[[nodiscard]] Status registerDevPreview(std::string_view debug_name,
											std::function<void(App&, UiManager&)> draw,
											bool uses_local_theme = false) noexcept {
		if (debug_name.empty() || !draw)
			return unexpectedError(
				makeError(ErrorCode::ThemeMutationFailed, ErrorSite::ThemeRegisterVariant));
		try {
			for (auto& preview : dev_previews_)
				if (preview.theme_type == detail::typeHash<T>() &&
					preview.scene.debug_name == debug_name) {
					preview.scene.draw = std::move(draw);
					preview.scene.uses_local_theme = uses_local_theme;
					return {};
				}
			dev_previews_.emplace_back(DevPreviewEntry{
				DevThemePreview{std::move(draw), std::string(debug_name), uses_local_theme},
				detail::typeHash<T>()});
			return {};
		} catch (...) {
			return unexpectedError(
				makeError(ErrorCode::ThemeMutationFailed, ErrorSite::ThemeRegisterVariant));
		}
	}
	/** Visit registered component examples for a theme type, in registration order. */
	template <typename Visitor>
	void visitDevPreviews(devMode::DevTypeId theme_type, Visitor&& visitor) const {
		for (const auto& preview : dev_previews_)
			if (preview.theme_type == theme_type)
				visitor(preview.scene);
	}
	struct DevThemePayloadView {
		devMode::DevTypeId type = 0;
		std::string_view typeName{};
		std::string_view variant{};
		const void* payload = nullptr;
		const void* authored = nullptr;
		const void* startup = nullptr;
		std::size_t payloadSize = 0;
		std::uint64_t revision = 0;
		bool active = false;
	};
	using DevThemePayloadVisitor = bool (*)(void*, const DevThemePayloadView&) noexcept;
	/** Clone the live payload into a new named variant; existing names are rejected. */
	[[nodiscard]] Status clone_dev_variant(devMode::DevTypeId type, std::string_view source,
										   std::string_view destination) noexcept;
	/** Switch the active variant of a reflected theme type. */
	[[nodiscard]] Status set_dev_active_variant(devMode::DevTypeId type,
												std::string_view variant) noexcept;
	[[nodiscard]] std::uint64_t devRevision() const noexcept;
	[[nodiscard]] std::size_t devThemeCount() const noexcept;
	bool visitDevCatalogueThemes(void* userData, DevThemePayloadVisitor visitor) const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	ThemeManager();
	~ThemeManager();

	ThemeManager(const ThemeManager&) = delete;
	ThemeManager& operator=(const ThemeManager&) = delete;

	/**
	 * @brief Register a named theme variant for type T.
	 *
	 * @tparam T User-defined or library theme struct type.
	 * @param variantName Name identifying this theme variant.
	 * @param themeData Theme struct payload containing design tokens.
	 * @param makeActive If true, sets this variant as the active theme for type T.
	 */
	template <typename T>
	Status registerTheme(std::string_view variantName, T themeData, bool makeActive = true) {
		if (!controller_) {
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeRegisterVariant));
		}
#if FLOW_UI_DEV_MODE
		if (devSchemaRegistry_) devSchemaRegistry_->ensureTheme<T>();
#endif
		try {
#if FLOW_UI_DEV_MODE
			const T authored = themeData;
#endif
#if FLOW_UI_HAS_BAKED_CHANGES
			baked::applyBakedThemeDefaults<T>(variantName, themeData);
#endif
			const auto variantId = controller_->internString(variantName);
			controller_->registerThemeVariant<T>(variantId, std::move(themeData), makeActive
#if FLOW_UI_DEV_MODE
												 ,
												 &authored
#endif
			);
			storage_->noteManagerMutation(InvalidWindowId);
			return {};
		} catch (const FlowUiException& exception) {
			return unexpectedError(exception.error());
		}
	}

	/**
	 * @brief Register the default variant for theme type T under name "default".
	 *
	 * @tparam T User-defined or library theme struct type.
	 * @param themeData Theme struct payload containing design tokens.
	 * @param makeActive If true, sets this variant as the active theme for type T.
	 */
	template <typename T>
	Status registerTheme(T themeData, bool makeActive = true) {
		return registerTheme<T>("default", std::move(themeData), makeActive);
	}

	/**
	 * @brief Set the active variant for theme type T by variant name.
	 *
	 * @tparam T Theme struct type.
	 * @param variantName Name of the registered variant to set active.
	 * @return true if the variant exists and was set active, false otherwise.
	 */
	template <typename T>
	Status setActiveVariant(std::string_view variantName) {
		if (!controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeSetActiveVariant));
		const auto variantId = controller_->internString(variantName);
		if (!controller_->hasThemeType<T>()) {
			return unexpectedError(makeError(ErrorCode::ThemeTypeNotFound, ErrorSite::ThemeSetActiveVariant));
		}
		if (!controller_->setActiveVariant<T>(variantId)) {
			return unexpectedError(makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeSetActiveVariant));
		}
		storage_->noteManagerMutation(InvalidWindowId);
		return {};
	}

	/**
	 * @brief Access the active variant for theme type T.
	 *
	 * @tparam T Theme struct type.
	 * @return Const reference to the active theme payload of type T.
	 * @throws std::runtime_error if no theme of type T is registered.
	 */
	template <typename T>
	[[nodiscard]] const T& getActiveTheme() const {
		if (!controller_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeLookupActiveVariant));
		}
		const T* ptr = controller_->getActiveThemeVariant<T>();
		if (!ptr) {
			throw FlowUiException(makeError(ErrorCode::ThemeActiveVariantMissing, ErrorSite::ThemeLookupActiveVariant));
		}
		return *ptr;
	}

	/**
	 * @brief Access a specific named variant for theme type T.
	 *
	 * @tparam T Theme struct type.
	 * @param variantName Name of the registered variant.
	 * @return Const reference to the named theme payload of type T.
	 * @throws std::runtime_error if variantName of type T is not registered.
	 */
	template <typename T>
	[[nodiscard]] const T& getTheme(std::string_view variantName) const {
		if (!controller_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeLookupActiveVariant));
		}
		const auto variantId = controller_->internString(variantName);
		const T* ptr = controller_->getThemeVariant<T>(variantId);
		if (!ptr) {
			throw FlowUiException(makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeLookupActiveVariant));
		}
		return *ptr;
	}

	/**
	 * @brief Queue a mutation function to update a theme variant at the next frame boundary.
	 *
	 * @tparam T Theme struct type.
	 * @param variantName Name of the registered variant to mutate.
	 * @param mutator Callback receiving reference to the theme struct.
	 */
	template <typename T>
	Status updateTheme(std::string_view variantName, std::function<void(T&)> mutator) {
		if (!controller_) {
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeStageMutation));
		}
		const auto variantId = controller_->internString(variantName);
		if (!controller_->hasThemeType<T>()) {
			return unexpectedError(makeError(ErrorCode::ThemeTypeNotFound, ErrorSite::ThemeStageMutation));
		}
		if (!controller_->queueThemeMutation<T>(variantId, std::move(mutator))) {
			return unexpectedError(makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeStageMutation));
		}
		return {};
	}

	/**
	 * @brief Queue a mutation function for the active variant of type T.
	 *
	 * @tparam T Theme struct type.
	 * @param mutator Callback receiving reference to the active theme struct.
	 */
	template <typename T>
	Status updateActiveTheme(std::function<void(T&)> mutator) {
		if (!controller_) {
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeStageMutation));
		}
		if (!controller_->hasThemeType<T>()) {
			return unexpectedError(makeError(ErrorCode::ThemeTypeNotFound, ErrorSite::ThemeStageMutation));
		}
		if (!controller_->queueActiveThemeMutation<T>(std::move(mutator))) {
			return unexpectedError(makeError(ErrorCode::ThemeActiveVariantMissing, ErrorSite::ThemeStageMutation));
		}
		return {};
	}

	/**
	 * @brief Initialize ThemeManager with a storage system instance.
	 * 
	 * @param storage Storage system instance.
	 */
	void init(detail::storage::IStorageSystem& storage);

	/**
	 * @brief Destroy ThemeManager resources and unregister storage records.
	 */
	void destroy() noexcept;

	/**
	 * @brief Apply all queued staged theme mutations.
	 */
	void applyStagedMutations();

private:
	friend class App;
	friend class UiManager;
#if FLOW_UI_DEV_MODE
	friend class devSystems::tooling::DevOverrideEngine;
	friend class devSystems::tooling::DevOverrideCapture;
	void setDevSchemaRegistry(devMode::DevSchemaRegistry* registry) noexcept {
		devSchemaRegistry_ = registry;
	}
	bool visitDevThemePayloads(
		void* userData,
		DevThemePayloadVisitor visitor) const noexcept;
	devMode::DevValueOperationStatus assignDevThemeField(
		devMode::DevTypeId type,
		std::string_view variant,
		std::span<const devMode::DevFieldOps* const> ownerPath,
		const devMode::DevFieldOps& field,
		const void* source) noexcept;
	/** Keep resource keys alive for assigned payloads and cloned startup baselines. */
	[[nodiscard]] Result<std::string_view>
	retain_dev_resource_key(std::string_view key) const noexcept;
#endif

private:
	detail::storage::IStorageSystem* storage_ = nullptr;
	std::unique_ptr<detail::manager_storage::ThemeStorageController> controller_{};
#if FLOW_UI_DEV_MODE
	struct DevPreviewEntry {
		DevThemePreview scene{};
		devMode::DevTypeId theme_type = 0;
	};
	std::vector<DevPreviewEntry> dev_previews_{};
	devMode::DevSchemaRegistry* devSchemaRegistry_ = nullptr;
#endif
};

/** @} */

} // namespace FlowUi
