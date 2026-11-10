#pragma once

#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"
#include <cstdint>
#include <string_view>

namespace FlowUi {

/** @brief Texture layout mode inside a target rectangle. */
enum class TextureFitMode : uint8_t {
	/** @brief Scale independently on each axis to fill the target rectangle. */
	Stretch = 0,
	/** @brief Fit the whole texture inside the target rectangle without cropping. */
	Contain = 1,
	/** @brief Fill the target rectangle while preserving aspect ratio; may crop. */
	Cover = 2,
	/** @brief Draw at source size without fit scaling. */
	None = 3,
};

/**
 * @brief Renderer texture reference returned by image, icon, and viewport managers.
 *
 * Treat this as a manager-owned handle with optional render settings. After
 * obtaining a TextureRef, application code may adjust fitMode and tintEnabled.
 * The handle, UV coordinates, and source dimensions are populated by
 * FlowUi managers and should normally be left unchanged. TextureRef owns no
 * GPU resource and does not extend manager or sourceKey storage lifetime.
 */
struct TextureRef {
	/**
	 * Borrowed logical source key used for stable reconstruction; backing bytes must outlive retained
	 * references.
	 */
	std::string_view sourceKey{};

	/** Create an unresolved texture reference from a borrowed manager key. The receiving renderer/manager resolves it later; this does not load or validate a resource.
	 * @param domain Manager namespace containing key.
	 * @param key Borrowed logical lookup key.
	 * @param fit Texture sizing policy.
	 * @param tint Whether to multiply by the image command color.
	 * @return Reference with no resolved texture handle. */
	[[nodiscard]] static constexpr TextureRef
	fromStable(ResourceDomain domain, std::string_view key,
			   TextureFitMode fit = TextureFitMode::Contain, bool tint = false) noexcept {
		TextureRef result{};
		result.sourceDomain = domain;
		result.sourceKey = key;
		result.fitMode = fit;
		result.tintEnabled = tint;
		return result;
	}

	/** @brief Manager-owned logical texture handle; do not edit manually. */
	TextureHandle handle{};
	/** Stable manager lookup retained for overrides and baked changes. */
	ResourceDomain sourceDomain = ResourceDomain::Auto;

	/** @brief Manager-owned left U coordinate; do not edit manually. */
	float uv0x = 0.0f;
	/** @brief Manager-owned top V coordinate; do not edit manually. */
	float uv0y = 0.0f;
	/** @brief Manager-owned right U coordinate; do not edit manually. */
	float uv1x = 1.0f;
	/** @brief Manager-owned bottom V coordinate; do not edit manually. */
	float uv1y = 1.0f;

	/**
	 * @brief Texture fit behavior inside the target rectangle.
	 *
	 * Application code may change this after obtaining the TextureRef.
	 *
	 * @see FlowUi::TextureFitMode
	 */
	TextureFitMode fitMode = TextureFitMode::Contain;

	/** @brief Manager-owned source texture width in pixels; do not edit manually. */
	int32_t sourceWidth = 0;
	/** @brief Manager-owned source texture height in pixels; do not edit manually. */
	int32_t sourceHeight = 0;

	/**
	 * @brief Enable multiplication by the image command color while rendering.
	 *
	 * Application code may change this after obtaining the TextureRef.
	 */
	bool tintEnabled = false;

	/** Manager-owned signal that an unavailable optional visual emits no draw. */
	bool skipIfUnavailable = false;
};

} // namespace FlowUi
