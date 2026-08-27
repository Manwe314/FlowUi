#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/Error.hpp"
#include "FlowUi/FontResources.hpp"
#include "FlowUi/ResourceKey.hpp"
#include "managers/structs/FontManagerStructs.hpp"
#if FLOW_UI_DEV_MODE
#include "internal/StorageSystem/StorageTypes.hpp"
#endif

namespace FlowUi {
class App;
namespace detail::storage { class IStorageSystem; struct FrameToken; }
namespace devSystems { class MemorySampleSink; class DevMemoryRecorder; }
namespace detail::manager_storage {
class FontCatalogController;
struct FontFrameView;
}

/** @addtogroup flowui_font_manager
 * @{
 */

/**
 * @brief Loads font faces and owns the atlas used by FlowUi text rendering.
 *
 * FlowUi::FontManager registers concrete font faces, groups them into logical font
 * families, and resolves a requested family/weight/style into the concrete
 * FontId consumed by Clay text configuration. Applications normally access it
 * through App::fonts() after App initialization.
 *
 * Loaded faces are uploaded into a Vulkan 2D array atlas. Each face occupies one
 * array layer, so the atlas can grow by allocating more layers without changing
 * the page width or height configured by UiConfig::fontAtlasSize.
 *
 * @code{.cpp}
 * FlowUi::App app(config);
 *
 * const FlowUi::FontFamilyId bodyFamily = app.fonts().createFamily({
 *     .name = "Body",
 *     .faces = {
 *         FlowUi::FontFaceCreateInfo{
 *             .path = "assets/fonts/Inter-Regular.arfont",
 *             .pixelSize = 18.0f,
 *             .weight = 400,
 *             .style = FlowUi::FontStyle::Normal,
 *         },
 *     },
 * });
 *
 * const FlowUi::FontId bodyFont =
 *     app.fonts().resolveFont(bodyFamily, 400, FlowUi::FontStyle::Normal);
 * @endcode
 *
 * @see @ref md_docs_2tutorials_2fonts__and__text "Fonts and Text"
 * @see @ref md_docs_2concepts_2managers "Managers"
 */
struct FontManager {
#if FLOW_UI_DEV_MODE
	struct DevFontView {
		FontId fontId = 0;
		std::string_view familyName{};
		std::uint32_t weight = 400;
		FontStyle style = FontStyle::Normal;
		const Font::FontFaceData* face = nullptr;
	};
	struct DevAtlasView {
		TextureHandle texture{};
		detail::storage::ImageViewHandle imageView{};
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint32_t layersUsed = 0;
		std::uint32_t layersCapacity = 0;
	};
	using DevFontVisitor = bool (*)(void*, const DevFontView&);
	[[nodiscard]] std::uint64_t devRevision() const noexcept;
	[[nodiscard]] std::size_t devFontCount() const noexcept;
	bool visitDevFonts(void* userData, DevFontVisitor visitor) const;
	[[nodiscard]] DevAtlasView devAtlas() const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
	void setDevMemoryRecorder(devSystems::DevMemoryRecorder* recorder) noexcept;
#endif
	/** @brief Convenience alias for FlowUi::FontId. */
	using FontId = FlowUi::FontId;

	/** @brief Convenience alias for FlowUi::FontFamilyId. */
	using FontFamilyId = FlowUi::FontFamilyId;

	/** @brief Convenience alias for FlowUi::FontStyle. */
	using FontStyle = FlowUi::FontStyle;

	/** @brief Convenience alias for FlowUi::FontFaceCreateInfo. */
	using FontFaceCreateInfo = FlowUi::FontFaceCreateInfo;

	/** @brief Convenience alias for FlowUi::FontFamilyCreateInfo. */
	using FontFamilyCreateInfo = FlowUi::FontFamilyCreateInfo;

	/**
	 * @brief Number of font atlas array layers allocated for the first atlas image.
	 *
	 * A layer is one square atlas page of UiConfig::fontAtlasSize by
	 * UiConfig::fontAtlasSize pixels. Because each loaded face is uploaded into
	 * one layer, this value is the number of faces the initial atlas allocation
	 * can hold before storage has to grow.
	 */
	static constexpr uint32_t kInitialAtlasLayerCapacity = 4;

	/**
	 * @brief Number of atlas array layers added when the font atlas grows.
	 *
	 * When registered faces exceed the current capacity, FlowUi::FontManager creates a
	 * larger atlas image and copies the already-uploaded layers into it. Capacity
	 * grows in fixed steps of this many layers to avoid reallocating for every
	 * additional face.
	 */
	static constexpr uint32_t kAtlasLayerGrowthStep = 4;

	/**
	 * @brief Create a logical font family and register its initial faces.
	 *
	 * The family name is used for string-based lookup and each face is loaded
	 * immediately. If createInfo.name is empty, the family is named "Default".
	 * Family ids are stable until FlowUi::FontManager is destroyed.
	 *
	 * Face paths ending in .arfont load baked font assets. Paths to .ttf files
	 * only work when FlowUi is built with runtime font baking enabled.
	 *
	 * @param createInfo Family name and initial concrete faces to load.
	 * @return Id of the newly created font family.
	 *
	 * @throws std::runtime_error if the family name already exists or any face
	 * cannot be loaded.
	 * 
	 * @note For now Font Family with the name "Default" will be created at init time so its best not to keep createInfo.name empty
	 *
	 * @code{.cpp}
	 * const FlowUi::FontFamilyId titleFamily = app.fonts().createFamily({
	 *     .name = "Title",
	 *     .faces = {
	 *         FlowUi::FontFaceCreateInfo{
	 *             .path = "assets/fonts/Inter-Bold.arfont",
	 *             .pixelSize = 24.0f,
	 *             .weight = 700,
	 *             .style = FlowUi::FontStyle::Normal,
	 *         },
	 *     },
	 * });
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2fonts__and__text "Fonts and Text"
	 */
	Result<FontFamilyId> createFamily(const FontFamilyCreateInfo& createInfo);
	Result<FontFamilyId> createFamily(ResourceKey key, const FontFamilyCreateInfo& createInfo);

	/**
	 * @brief Return a family id by name.
	 *
	 * This is a non-throwing lookup for code that wants to cache the id returned
	 * by a previously registered family. Missing families return
	 * std::numeric_limits<FontFamilyId>::max().
	 *
	 * @param familyName Logical family name passed to createFamily().
	 * @return Matching family id, or UINT32_MAX when no family has that name.
	 *
	 * @code{.cpp}
	 * const FlowUi::FontFamilyId bodyFamily = app.fonts().getFamilyId("Body");
	 * if (bodyFamily != std::numeric_limits<FlowUi::FontFamilyId>::max()) {
	 *     const FlowUi::FontId bodyFont = app.fonts().resolveFont(bodyFamily);
	 *     (void)bodyFont;
	 * }
	 * @endcode
	 */
	FontFamilyId getFamilyId(std::string_view familyName) const;
	FontFamilyId getFamilyId(ResourceKey key) const;

	/**
	 * @brief Add a concrete face to an existing family.
	 *
	 * The face is loaded immediately and becomes available to resolveFont() for
	 * the provided weight and style. Use this overload when the caller already
	 * cached a FontFamilyId.
	 *
	 * Paths ending in .arfont load baked font assets. Paths to .ttf files only
	 * work when FlowUi is built with runtime font baking enabled.
	 *
	 * @param familyId Existing logical family id.
	 * @param createInfo Concrete face source path, pixel size, weight, style,
	 * and optional name.
	 * @return FontId of the newly loaded concrete face.
	 *
	 * @throws std::runtime_error if familyId does not exist or the face cannot be
	 * loaded.
	 *
	 * @code{.cpp}
	 * const FlowUi::FontFamilyId bodyFamily = app.fonts().getFamilyId("Body");
	 * const FlowUi::FontId italicFont = app.fonts().addFamilyFace(
	 *     bodyFamily,
	 *     FlowUi::FontFaceCreateInfo{
	 *         .path = "assets/fonts/Inter-Italic.arfont",
	 *         .pixelSize = 18.0f,
	 *         .weight = 400,
	 *         .style = FlowUi::FontStyle::Italic,
	 *     });
	 * @endcode
	 */
	Result<FontId> addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo);

	/**
	 * @brief Add a concrete face to a named family.
	 *
	 * The face is loaded immediately and becomes available to resolveFont() for
	 * the provided weight and style. Use this overload when the caller wants the
	 * manager to look up the family by name.
	 *
	 * Paths ending in .arfont load baked font assets. Paths to .ttf files only
	 * work when FlowUi is built with runtime font baking enabled.
	 *
	 * @param familyName Existing logical family name.
	 * @param createInfo Concrete face source path, pixel size, weight, style,
	 * and optional name.
	 * @return FontId of the newly loaded concrete face.
	 *
	 * @throws std::runtime_error if familyName does not exist or the face cannot
	 * be loaded.
	 *
	 * @code{.cpp}
	 * const FlowUi::FontId boldFont = app.fonts().addFamilyFace(
	 *     "Body",
	 *     FlowUi::FontFaceCreateInfo{
	 *         .path = "assets/fonts/Inter-Bold.arfont",
	 *         .pixelSize = 18.0f,
	 *         .weight = 700,
	 *         .style = FlowUi::FontStyle::Normal,
	 *     });
	 * @endcode
	 */
	Result<FontId> addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo);
	Result<FontId> addFamilyFace(ResourceKey key, const FontFaceCreateInfo& createInfo);

	/**
	 * @brief Resolve a concrete Clay font id for a family, weight, and style.
	 *
	 * Resolution first considers faces with the requested style, then chooses the
	 * face whose CSS-style weight is closest to weight. If the family has no
	 * matching style, the first registered face in the family is returned as a
	 * fallback.
	 *
	 * @see docs/font-resolution.md for the planned detailed fallthrough
	 * resolution rules.
	 *
	 * @param familyId Existing logical family id.
	 * @param weight Requested CSS-style font weight, such as 400 or 700.
	 * @param style Requested normal or italic style.
	 * @return Concrete FontId for Clay text, or 0 if the family is invalid or
	 * empty.
	 *
	 * @code{.cpp}
	 * Clay_TextElementConfig titleText{};
	 * titleText.fontId = app.fonts().resolveFont(
	 *     titleFamily,
	 *     700,
	 *     FlowUi::FontStyle::Normal);
	 * @endcode
	 */
	FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const;

	/**
	 * @brief Resolve a concrete Clay font id for a named family, weight, and style.
	 *
	 * Resolution first looks up familyName, then considers faces with the
	 * requested style and chooses the face whose CSS-style weight is closest to
	 * weight. If the family has no matching style, the first registered face in
	 * the family is returned as a fallback.
	 *
	 * @see docs/font-resolution.md for the planned detailed fallthrough
	 * resolution rules.
	 *
	 * @param familyName Existing logical family name.
	 * @param weight Requested CSS-style font weight, such as 400 or 700.
	 * @param style Requested normal or italic style.
	 * @return Concrete FontId for Clay text, or 0 if the family is missing or
	 * empty.
	 *
	 * @code{.cpp}
	 * Clay_TextElementConfig labelText{};
	 * labelText.fontId = app.fonts().resolveFont(
	 *     "Body",
	 *     400,
	 *     FlowUi::FontStyle::Normal);
	 * @endcode
	 */
	FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const;
	FontId resolveFont(ResourceKey key, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const;

	/**
	 * @brief Return loaded font data by concrete font id.
	 *
	 * This exposes the baked metrics, glyph table, kerning table, and atlas
	 * placement for renderer or layout integrations that need direct access to
	 * a registered face. Normal UI code usually only needs resolveFont().
	 *
	 * @param fontId Concrete id returned by createFamily(), addFamilyFace(), or
	 * resolveFont().
	 * @return Pointer to loaded face data, or nullptr if fontId is unknown.
	 *
	 * @code{.cpp}
	 * const FlowUi::FontId bodyFont = app.fonts().resolveFont("Body");
	 * if (const FlowUi::Font::FontFaceData* face = app.fonts().getFontById(bodyFont)) {
	 *     const FlowUi::Font::FontVariantData* variant = face->defaultVariant();
	 *     const uint32_t atlasLayer = face->atlasLayer;
	 *     (void)variant;
	 *     (void)atlasLayer;
	 * }
	 * @endcode
	 */
	const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const;

	/**
	 * @brief Return the active Vulkan font atlas array resource.
	 *
	 * Render integrations use this to bind the atlas image view and sampler. The
	 * returned reference is owned by FlowUi::FontManager and remains valid until the font
	 * manager is destroyed or reinitialized. Check bindingRevision to know when a
	 * descriptor using the atlas should be refreshed.
	 *
	 * @return Font atlas image, allocation, image view, sampler, dimensions,
	 * layer counts, and binding revision.
	 *
	 * @code{.cpp}
	 * const FlowUi::Font::AtlasArrayResource& atlas = app.fonts().getAtlasResource();
	 * if (atlas.view != VK_NULL_HANDLE && atlas.sampler != VK_NULL_HANDLE) {
	 *     VkDescriptorImageInfo imageInfo{};
	 *     imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	 *     imageInfo.imageView = atlas.view;
	 *     imageInfo.sampler = atlas.sampler;
	 * }
	 * @endcode
	 */
	const FlowUi::Font::AtlasArrayResource& getAtlasResource() const;

private:
	friend class App;

	void init(detail::storage::IStorageSystem& storage, uint32_t atlasSize);
	void destroy() noexcept;
	[[nodiscard]] detail::manager_storage::FontFrameView frameView(
		const detail::storage::FrameToken& frame) const;

	FontId loadFontFace(const FontFaceCreateInfo& createInfo);
	FontId loadFont(std::string_view path, float px);
	FontId registerBakedFont(std::string_view arfontPath, std::string_view requestedName = {});
	FontId registerRuntimeFont(const FontFaceCreateInfo& createInfo);

	detail::storage::IStorageSystem* storage_ = nullptr;
	uint64_t controllerHandle_ = 0;
	detail::manager_storage::FontCatalogController* controller_ = nullptr;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevMemoryRecorder* devMemoryRecorder_ = nullptr;
#endif
};

/** @} */

} // namespace FlowUi
