#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

#include "FlowUi/Public/BuildConfig.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <span>
#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "devSystems/devTooling/bake/DevBakedChanges.hpp"
#include "FlowUi/Private/resourceService/ThemeStorageController.hpp"
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
		/** @brief Retained example drawing callback invoked synchronously by the preview host. */
		std::function<void(App&, UiManager&)> draw{};
		/** @brief Owned name identifying this example within its theme type. */
		std::string debug_name{};
		/** @brief Whether all nested theme reads support UiManager's local theme resolver. */
		bool uses_local_theme = false;
	};
	/**
	 * @brief Available only with FLOW_UI_DEV_MODE. Register or replace a named retained example for every variant of T. Name/callback must be nonempty. Callback/name storage failures inside this call become Status errors; constructing the std::function argument happens before the call and may itself throw. Captures must remain valid for the manager lifetime. Set uses_local_theme only when nested theme reads all use UiManager's scoped resolver. Preview drawing must not register previews or trigger application operations.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const std::function<void(FlowUi::App&, FlowUi::UiManager&)> draw =
	 *     [](FlowUi::App&, FlowUi::UiManager& ui) {
	 *         auto themeResult = ui.flowTheme();
	 *         if (!themeResult) throw FlowUi::FlowUiException(themeResult.error());
	 *         const FlowUi::FlowUiTheme& theme = themeResult->get();
	 *         ui.createElement(counter, "theme-preview")
	 *             .setParameters({.width = theme.fontSizeMedium * 8.0f})
	 *             .draw();
	 *     };
	 * auto status = themes.registerDevPreview<FlowUi::FlowUiTheme>("Counter", draw, true);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T>
	[[nodiscard]] Status registerDevPreview(std::string_view debug_name,
											const std::function<void(App&, UiManager&)>& draw,
											bool uses_local_theme = false) noexcept {
		if (debug_name.empty() || !draw)
			return unexpectedError(
				makeError(ErrorCode::ThemeMutationFailed, ErrorSite::ThemeRegisterVariant));
		try {
			for (auto& preview : dev_previews_)
				if (preview.theme_type == detail::typeHash<T>() &&
					preview.scene.debug_name == debug_name) {
					preview.scene.draw = draw;
					preview.scene.uses_local_theme = uses_local_theme;
					return {};
				}
			dev_previews_.emplace_back(DevPreviewEntry{
				DevThemePreview{draw, std::string(debug_name), uses_local_theme},
				detail::typeHash<T>()});
			return {};
		} catch (...) {
			return unexpectedError(detail::currentExceptionError(ErrorSite::ThemeRegisterVariant));
		}
	}
	/**
	 * @brief Available only with FLOW_UI_DEV_MODE. Construct the retained std::function inside the recoverable error boundary, then register it using the same name/ownership/scoped-theme contract as the std::function overload. Callback construction and registration failures become Status errors. The drawing callback runs later; its own drawing exceptions follow that enclosing frame boundary.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto status = themes.registerDevPreview<FlowUi::FlowUiTheme>("Counter",
	 *     [](FlowUi::App&, FlowUi::UiManager& ui) {
	 *         auto themeResult = ui.flowTheme();
	 *         if (!themeResult) throw FlowUi::FlowUiException(themeResult.error());
	 *         const FlowUi::FlowUiTheme& theme = themeResult->get();
	 *         ui.createElement(counter, "theme-preview")
	 *             .setParameters({.width = theme.fontSizeMedium * 8.0f})
	 *             .draw();
	 *     }, true);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T, typename Draw>
		requires (!std::is_same_v<std::remove_cvref_t<Draw>, std::function<void(App&, UiManager&)>>)
	[[nodiscard]] Status registerDevPreview(std::string_view debugName, Draw&& draw,
										 bool usesLocalTheme = false) noexcept {
		return detail::catchResult<Status>(ErrorSite::ThemeRegisterVariant, [&] {
			return registerDevPreview<T>(debugName,
				std::function<void(App&, UiManager&)>(std::forward<Draw>(draw)), usesLocalTheme);
		});
	}

	/**
	 * @brief Available only with FLOW_UI_DEV_MODE. Synchronously visit examples of one reflected theme type in registration order. The preview and its fields are borrowed for the callback. A completed visit returns successful Status; arbitrary visitor exceptions propagate. This function has no noexcept contract.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t previewCount = 0;
	 * auto status = themes.visitDevPreviews(inspectedTheme.type,
	 *     [&previewCount](const FlowUi::ThemeManager::DevThemePreview& preview) {
	 *         if (preview.draw && !preview.debug_name.empty()) ++previewCount;
	 *     });
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename Visitor>
	[[nodiscard]] Status visitDevPreviews(devSystems::tooling::DevTypeId theme_type, Visitor&& visitor) const {
		for (const auto& preview : dev_previews_)
			if (preview.theme_type == theme_type)
				visitor(preview.scene);
		return {};
	}
	/** @brief Borrowed reflected theme payloads; valid during visitation and until registry mutation. */
	struct DevThemePayloadView {
		/** @brief Reflected theme type identity. */
		devSystems::tooling::DevTypeId type = 0;
		/** @brief Borrowed theme type display name. */
		std::string_view typeName{};
		/** @brief Borrowed registered variant name. */
		std::string_view variant{};
		/** @brief Live payload; cast only through the matching reflected type. */
		const void* payload = nullptr;
		/** @brief Authored baseline payload before baked/runtime changes, when available. */
		const void* authored = nullptr;
		/** @brief Startup baseline payload for change comparison, when available. */
		const void* startup = nullptr;
		/** @brief Payload size in bytes for type validation. */
		std::size_t payloadSize = 0;
		/** @brief Theme revision observed by this snapshot. */
		std::uint64_t revision = 0;
		/** @brief Whether this is the active variant of its type. */
		bool active = false;
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevThemePayloadVisitor = bool (*)(void*, const DevThemePayloadView&) noexcept;
	/**
	 * @brief Available only with FLOW_UI_DEV_MODE. Clone the live reflected payload into a new named variant, rejecting existing destination names. Check initialization/type/source/destination failures through Status. Obtain the reflected identity from a real catalogue payload view; cloning does not change its source payload. Reacquire catalogue borrows after registry mutation.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const std::string_view destination = "custom";
	 * auto status = themes.clone_dev_variant(
	 *     inspectedTheme.type, inspectedTheme.variant, destination);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	[[nodiscard]] Status clone_dev_variant(devSystems::tooling::DevTypeId type, std::string_view source,
										   std::string_view destination) noexcept;
	/**
	 * @brief Available only with FLOW_UI_DEV_MODE. Select the named variant of a reflected theme type. Return initialization/missing-variant errors through Status. Obtain the type and variant from a real catalogue payload view and reacquire theme borrows after changing the registry selection.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto status = themes.set_dev_active_variant(
	 *     inspectedTheme.type, inspectedTheme.variant);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	[[nodiscard]] Status set_dev_active_variant(devSystems::tooling::DevTypeId type,
												std::string_view variant) noexcept;
	/** @brief Return the current development-observation revision; zero when support is disabled. */
	[[nodiscard]] std::uint64_t devRevision() const noexcept;
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devThemeCount() const noexcept;
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevCatalogueThemes(void* userData, DevThemePayloadVisitor visitor) const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Construct a detached manager; application code normally obtains the initialized App-owned manager.
	 *
	 * @see docs/api/theme_manager.md
	 */
	ThemeManager() noexcept;
	/**
	 * @brief Release owned service data. The App controls this manager lifetime.
	 *
	 * @see docs/api/theme_manager.md
	 */
	~ThemeManager() noexcept;

	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/theme_manager.md
	 */
	ThemeManager(const ThemeManager&) = delete;
	/**
	 * @brief Copying and moving are disabled as declared; use the App-owned instance by reference.
	 *
	 * @see docs/api/theme_manager.md
	 */
	ThemeManager& operator=(const ThemeManager&) = delete;

	/**
	 * @brief Register an owned copy of a named theme payload; the shorter overload uses variant "default". makeActive selects the registered variant immediately.
	 *
	 * Hybrid contract: known validation failures are returned; arbitrary payload copy/assignment and allocation exceptions propagate. Failed replacement can partially affect a payload; abandon it instead of assuming rollback.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = themes.registerTheme<FlowUi::FlowUiTheme>("light", FlowUi::FlowUiTheme::light(), false);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T>
	[[nodiscard]] Status registerTheme(std::string_view variantName, const T& themeData,
									   bool makeActive = true) {
		if (!controller_ || !storage_) {
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeRegisterVariant));
		}
		const auto writable = detail::catchResult<Status>(ErrorSite::ThemeRegisterVariant, [&] {
			storage_->noteManagerMutation(InvalidWindowId);
			return Status{};
		});
		if (!writable) return writable;
#if FLOW_UI_DEV_MODE
		if (devSchemaRegistry_) devSchemaRegistry_->ensureTheme<T>();
#endif
		T ownedThemeData = themeData;
#if FLOW_UI_DEV_MODE
		const T authored = themeData;
#endif
#if FLOW_UI_HAS_BAKED_CHANGES
		baked::applyBakedThemeDefaults<T>(variantName, ownedThemeData);
#endif
		const auto variantId = controller_->internString(variantName);
		controller_->registerThemeVariant<T>(variantId, std::move(ownedThemeData), makeActive
#if FLOW_UI_DEV_MODE
											 ,
											 &authored
#endif
		);
		return {};
	}

	/**
	 * @brief Register an owned copy of a named theme payload; the shorter overload uses variant "default". makeActive selects the registered variant immediately.
	 *
	 * Hybrid contract: known validation failures are returned; arbitrary payload copy/assignment and allocation exceptions propagate. Failed replacement can partially affect a payload; abandon it instead of assuming rollback.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = themes.registerTheme<FlowUi::FlowUiTheme>(FlowUi::FlowUiTheme::dark());
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T>
	[[nodiscard]] Status registerTheme(const T& themeData, bool makeActive = true) {
		return registerTheme<T>("default", themeData, makeActive);
	}

	/**
	 * @brief Select a previously registered variant for this theme type. Missing initialization/type/variant failures are returned in Status.
	 *
	 * Hybrid contract: known FlowUi validation failures are returned; non-FlowUi name preparation/allocation exceptions propagate.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = themes.setActiveVariant<FlowUi::FlowUiTheme>("light");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T>
	[[nodiscard]] Status setActiveVariant(std::string_view variantName) try {
		if (!controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeSetActiveVariant));
		const auto variantId = controller_->internString(variantName);
		if (!controller_->hasThemeType<T>()) {
			return unexpectedError(makeError(ErrorCode::ThemeTypeNotFound, ErrorSite::ThemeSetActiveVariant));
		}
		if (!controller_->setActiveVariant<T>(variantId)) {
			return unexpectedError(makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeSetActiveVariant));
		}
		return {};
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}

	/**
	 * @brief Borrow the active registered payload through Result<reference_wrapper<const T>>; missing initialization or active variant is an error.
	 *
	 * Check the Result, then use const T& value = result->get(). This is a borrow: reacquire after replacing variants, registry mutations or manager destruction/reinitialization.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = themes.getActiveTheme<FlowUi::FlowUiTheme>();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowUiTheme& theme = result->get();
	 * const float fontSize = theme.fontSizeMedium;
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T>
	[[nodiscard]] Result<std::reference_wrapper<const T>> getActiveTheme() const noexcept try {
		if (!controller_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeLookupActiveVariant));
		}
		const T* ptr = controller_->getActiveThemeVariant<T>();
		if (!ptr) {
			throw FlowUiException(makeError(ErrorCode::ThemeActiveVariantMissing, ErrorSite::ThemeLookupActiveVariant));
		}
		return std::cref(*ptr);
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ThemeLookupActiveVariant));
	}

	/**
	 * @brief Borrow one named registered payload through Result<reference_wrapper<const T>>; missing initialization or variant is an error.
	 *
	 * Check the Result, then use const T& value = result->get(). This is a borrow: reacquire after replacing variants, registry mutations or manager destruction/reinitialization.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = themes.getTheme<FlowUi::FlowUiTheme>("light");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowUiTheme& theme = result->get();
	 * const float fontSize = theme.fontSizeMedium;
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T>
	[[nodiscard]] Result<std::reference_wrapper<const T>> getTheme(std::string_view variantName) const noexcept try {
		if (!controller_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeLookupActiveVariant));
		}
		const auto variantId = controller_->internString(variantName);
		const T* ptr = controller_->getThemeVariant<T>(variantId);
		if (!ptr) {
			throw FlowUiException(makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeLookupActiveVariant));
		}
		return std::cref(*ptr);
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ThemeLookupActiveVariant));
	}

	/**
	 * @brief Queue a retained mutator for the named variant. Success means queued, not applied; captured references must survive application of queued work.
	 *
	 * Hybrid contract: initialization/type/variant failures are returned; callback construction, name preparation and allocation exceptions propagate. Mutators run later and may throw when applied.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = themes.updateTheme<FlowUi::FlowUiTheme>("light", [](FlowUi::FlowUiTheme& theme) { theme.fontSizeMedium = 16.0f; });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T, typename Mutator>
	[[nodiscard]] Status updateTheme(std::string_view variantName,
									 Mutator&& mutator) {
		if (!controller_) {
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeStageMutation));
		}
		const auto variantId = controller_->internString(variantName);
		if (!controller_->hasThemeType<T>()) {
			return unexpectedError(makeError(ErrorCode::ThemeTypeNotFound, ErrorSite::ThemeStageMutation));
		}
		if (!controller_->queueThemeMutation<T>(variantId, std::function<void(T&)>(std::forward<Mutator>(mutator)))) {
			return unexpectedError(makeError(ErrorCode::ThemeVariantNotFound, ErrorSite::ThemeStageMutation));
		}
		return {};
	}

	/**
	 * @brief Queue a retained mutator for the active variant of this type. Success means queued, not applied; borrowed captures must survive application.
	 *
	 * Hybrid contract: initialization/type/variant failures are returned; callback construction, name preparation and allocation exceptions propagate. Mutators run later and may throw when applied.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = themes.updateActiveTheme<FlowUi::FlowUiTheme>([](FlowUi::FlowUiTheme& theme) { theme.fontSizeMedium = 16.0f; });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/theme_manager.md
	 */
	template <typename T, typename Mutator>
	[[nodiscard]] Status updateActiveTheme(Mutator&& mutator) {
		if (!controller_) {
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ThemeStageMutation));
		}
		if (!controller_->hasThemeType<T>()) {
			return unexpectedError(makeError(ErrorCode::ThemeTypeNotFound, ErrorSite::ThemeStageMutation));
		}
		if (!controller_->queueActiveThemeMutation<T>(std::function<void(T&)>(std::forward<Mutator>(mutator)))) {
			return unexpectedError(makeError(ErrorCode::ThemeActiveVariantMissing, ErrorSite::ThemeStageMutation));
		}
		return {};
	}

	/**
	 * @brief Framework lifecycle wiring to the App-owned storage system. Application code obtains this manager through App::themes().
	 *
	 * @see docs/api/theme_manager.md
	 */
	void init(detail::storage::IStorageSystem& storage);

	/**
	 * @brief Release this manager storage records and pending mutations. Framework lifecycle service; application code should retain the App-owned manager.
	 *
	 * @see docs/api/theme_manager.md
	 */
	void destroy() noexcept;

	/**
	 * @brief Apply queued mutations at an application safe point. App normally calls this service; avoid changing themes while UI output is being built.
	 *
	 * Arbitrary mutator exceptions propagate; a mutator may already have changed its target. Remaining detached callbacks are abandoned after failure, with no automatic retry. Rejection before detaching retains the queued batch.
	 *
	 * @see docs/api/theme_manager.md
	 */
	void applyStagedMutations();

private:
	friend class App;
	friend class UiManager;
#if FLOW_UI_DEV_MODE
	friend class devSystems::tooling::DevOverrideEngine;
	friend class devSystems::tooling::DevOverrideCapture;
	void setDevSchemaRegistry(devSystems::tooling::DevSchemaRegistry* registry) noexcept {
		devSchemaRegistry_ = registry;
	}
	bool visitDevThemePayloads(
		void* userData,
		DevThemePayloadVisitor visitor) const noexcept;
	devSystems::tooling::DevValueOperationStatus assignDevThemeField(
		devSystems::tooling::DevTypeId type,
		std::string_view variant,
		std::span<const devSystems::tooling::DevFieldOps* const> ownerPath,
		const devSystems::tooling::DevFieldOps& field,
		const void* source) noexcept;
	// Keep resource keys alive for assigned payloads and cloned startup baselines.
	[[nodiscard]] Result<std::string_view>
	retain_dev_resource_key(std::string_view key) const noexcept;
#endif

private:
	detail::storage::IStorageSystem* storage_ = nullptr;
	std::unique_ptr<detail::manager_storage::ThemeStorageController> controller_{};
#if FLOW_UI_DEV_MODE
	struct DevPreviewEntry {
		DevThemePreview scene{};
		devSystems::tooling::DevTypeId theme_type = 0;
	};
	std::vector<DevPreviewEntry> dev_previews_{};
	devSystems::tooling::DevSchemaRegistry* devSchemaRegistry_ = nullptr;
#endif
};

/** @} */

} // namespace FlowUi
