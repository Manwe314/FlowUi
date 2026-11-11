#pragma once

#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include <clay.h>

namespace FlowUi {

/**
 * @brief Standard design system tokens for FlowUi built-in elements.
 *
 * Default-constructed tokens form the dark palette. Color constants in this
 * legacy theme use normalized 0..1 components; layout spacing and radii are
 * logical pixels. The theme value owns its tokens and may be copied or
 * registered with ThemeManager like any other application theme type.
 */
struct FlowUiTheme {
	/** @brief Primary brand accent color. */
	Clay_Color primary = {0.0f, 122.0f / 255.0f, 204.0f / 255.0f, 1.0f};
	/** @brief Hover state color for primary elements. */
	Clay_Color primaryHover = {28.0f / 255.0f, 151.0f / 255.0f, 234.0f / 255.0f, 1.0f};
	/** @brief Active/pressed state color for primary elements. */
	Clay_Color primaryActive = {0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
	/** @brief Content/text color rendered on top of primary color background. */
	Clay_Color onPrimary = {1.0f, 1.0f, 1.0f, 1.0f};

	/** @brief Main background surface color. */
	Clay_Color background = {30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f};
	/** @brief Standard component surface color. */
	Clay_Color surface = {37.0f / 255.0f, 37.0f / 255.0f, 38.0f / 255.0f, 1.0f};
	/** @brief Header surface color. */
	Clay_Color surfaceHeader = {45.0f / 255.0f, 45.0f / 255.0f, 46.0f / 255.0f, 1.0f};
	/** @brief Hover state color for standard surface elements. */
	Clay_Color surfaceHover = {62.0f / 255.0f, 62.0f / 255.0f, 66.0f / 255.0f, 1.0f};
	/** @brief Selected state color for list items and cards. */
	Clay_Color surfaceSelected = {55.0f / 255.0f, 55.0f / 255.0f, 61.0f / 255.0f, 1.0f};

	/** @brief High-contrast primary text color. */
	Clay_Color textPrimary = {1.0f, 1.0f, 1.0f, 1.0f};
	/** @brief Muted secondary text color. */
	Clay_Color textSecondary = {204.0f / 255.0f, 204.0f / 255.0f, 204.0f / 255.0f, 1.0f};
	/** @brief Disabled text color. */
	Clay_Color textDisabled = {108.0f / 255.0f, 108.0f / 255.0f, 108.0f / 255.0f, 1.0f};
	/** @brief Hyperlink text color. */
	Clay_Color textLink = {79.0f / 255.0f, 193.0f / 255.0f, 255.0f / 255.0f, 1.0f};

	/** @brief Standard border color. */
	Clay_Color border = {69.0f / 255.0f, 69.0f / 255.0f, 69.0f / 255.0f, 1.0f};
	/** @brief Focused border color. */
	Clay_Color borderFocused = {0.0f, 122.0f / 255.0f, 204.0f / 255.0f, 1.0f};
	/** @brief Separator and divider line color. */
	Clay_Color divider = {45.0f / 255.0f, 45.0f / 255.0f, 46.0f / 255.0f, 1.0f};

	/** @brief Success status indicator color. */
	Clay_Color success = {78.0f / 255.0f, 201.0f / 255.0f, 176.0f / 255.0f, 1.0f};
	/** @brief Warning status indicator color. */
	Clay_Color warning = {206.0f / 255.0f, 145.0f / 255.0f, 120.0f / 255.0f, 1.0f};
	/** @brief Error/danger status indicator color. */
	Clay_Color danger = {241.0f / 255.0f, 76.0f / 255.0f, 76.0f / 255.0f, 1.0f};

	/** @brief Small corner radius token. */
	Clay_CornerRadius radiusSmall = {2.0f, 2.0f, 2.0f, 2.0f};
	/** @brief Medium corner radius token. */
	Clay_CornerRadius radiusMedium = {4.0f, 4.0f, 4.0f, 4.0f};
	/** @brief Large corner radius token. */
	Clay_CornerRadius radiusLarge = {8.0f, 8.0f, 8.0f, 8.0f};
	/** @brief Pill / circular corner radius token. */
	Clay_CornerRadius radiusPill = {99.0f, 99.0f, 99.0f, 99.0f};

	/** @brief Default font family ID. */
	FontFamilyId defaultFontFamily = 0;
	/** @brief Small font size in pixels. */
	float fontSizeSmall = 12.0f;
	/** @brief Medium font size in pixels. */
	float fontSizeMedium = 14.0f;
	/** @brief Large font size in pixels. */
	float fontSizeLarge = 18.0f;
	/** @brief Header font size in pixels. */
	float fontSizeHeader = 24.0f;

	/** @brief Extra-small layout spacing in pixels. */
	uint16_t spacingXs = 4;
	/** @brief Small layout spacing in pixels. */
	uint16_t spacingSm = 8;
	/** @brief Medium layout spacing in pixels. */
	uint16_t spacingMd = 16;
	/** @brief Large layout spacing in pixels. */
	uint16_t spacingLg = 24;

	/** @brief Factory returning default dark theme instance. */
	[[nodiscard]] static FlowUiTheme dark() noexcept;
	/** @brief Factory returning default light theme instance. */
	[[nodiscard]] static FlowUiTheme light() noexcept;
};

} // namespace FlowUi
