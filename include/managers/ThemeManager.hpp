#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "internal/ManagerStorage/ThemeStorageController.hpp"

namespace FlowUi {

class App;
class UiManager;

namespace detail::storage { class IStorageSystem; }

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
	void registerTheme(std::string_view variantName, T themeData, bool makeActive = true) {
		if (!controller_) {
			throw std::runtime_error("ThemeManager is not initialized.");
		}
		const auto variantId = controller_->internString(variantName);
		controller_->registerThemeVariant<T>(variantId, std::move(themeData), makeActive);
	}

	/**
	 * @brief Register the default variant for theme type T under name "default".
	 *
	 * @tparam T User-defined or library theme struct type.
	 * @param themeData Theme struct payload containing design tokens.
	 * @param makeActive If true, sets this variant as the active theme for type T.
	 */
	template <typename T>
	void registerTheme(T themeData, bool makeActive = true) {
		registerTheme<T>("default", std::move(themeData), makeActive);
	}

	/**
	 * @brief Set the active variant for theme type T by variant name.
	 *
	 * @tparam T Theme struct type.
	 * @param variantName Name of the registered variant to set active.
	 * @return true if the variant exists and was set active, false otherwise.
	 */
	template <typename T>
	bool setActiveVariant(std::string_view variantName) {
		if (!controller_) return false;
		const auto variantId = controller_->internString(variantName);
		return controller_->setActiveVariant<T>(variantId);
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
			throw std::runtime_error("ThemeManager is not initialized.");
		}
		const T* ptr = controller_->getActiveThemeVariant<T>();
		if (!ptr) {
			throw std::runtime_error("No active theme variant registered for requested type.");
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
			throw std::runtime_error("ThemeManager is not initialized.");
		}
		const auto variantId = controller_->internString(variantName);
		const T* ptr = controller_->getThemeVariant<T>(variantId);
		if (!ptr) {
			throw std::runtime_error("Requested theme variant is not registered.");
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
	void updateTheme(std::string_view variantName, std::function<void(T&)> mutator) {
		if (!controller_) {
			throw std::runtime_error("ThemeManager is not initialized.");
		}
		const auto variantId = controller_->internString(variantName);
		controller_->queueThemeMutation<T>(variantId, std::move(mutator));
	}

	/**
	 * @brief Queue a mutation function for the active variant of type T.
	 *
	 * @tparam T Theme struct type.
	 * @param mutator Callback receiving reference to the active theme struct.
	 */
	template <typename T>
	void updateActiveTheme(std::function<void(T&)> mutator) {
		if (!controller_) {
			throw std::runtime_error("ThemeManager is not initialized.");
		}
		controller_->queueActiveThemeMutation<T>(std::move(mutator));
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

private:
	detail::storage::IStorageSystem* storage_ = nullptr;
	std::unique_ptr<detail::manager_storage::ThemeStorageController> controller_{};
};

/** @} */

} // namespace FlowUi
