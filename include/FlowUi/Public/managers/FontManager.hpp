#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#endif

namespace FlowUi {
class App;
namespace Font {
struct AtlasArrayResource;
}
namespace detail::storage {
class IStorageSystem;
struct FrameToken;
} // namespace detail::storage
namespace devSystems {
class MemorySampleSink;
class DevMemoryRecorder;
} // namespace devSystems
namespace detail::manager_storage {
class FontCatalogController;
struct FontFrameView;
} // namespace detail::manager_storage

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
 * FlowUi::App app = FlowUi::makeApplication(config);
 *
 * const auto bodyFamily = app.fonts().createFamily({
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
 * if (!bodyFamily) {
 *     return; // Report bodyFamily.error() to the application.
 * }
 * const FlowUi::FontId bodyFont =
 *     app.fonts().resolveFont(*bodyFamily, 400, FlowUi::FontStyle::Normal);
 * @endcode
 *
 * @see @ref md_docs_2tutorials_2fonts__and__text "Fonts and Text"
 * @see @ref md_docs_2concepts_2managers "Managers"
 */
struct FontManager {
#if FLOW_UI_DEV_MODE
	/** @brief Borrowed registered-face metadata; reacquire after font catalog mutation. */
	struct DevFontView {
		/** @brief Concrete face identity used by Clay text. */
		FontId fontId = 0;
		/** @brief Owning logical family identity. */
		FontFamilyId familyId = 0;
		/** @brief Borrowed registered family name. */
		std::string_view familyName{};
		/** @brief Registered CSS-style weight. */
		std::uint32_t weight = 400;
		/** @brief Registered normal or italic style. */
		FontStyle style = FontStyle::Normal;
		/** @brief Borrowed loaded-face payload; never destroy or retain across catalog mutation. */
		const Font::FontFaceData* face = nullptr;
	};
	/** Borrowed raw RGB preview of one font atlas layer, valid until atlas publication changes. */
	struct DevAtlasLayerView {
		/** @brief Borrowed preview binding for this atlas layer. */
		TextureHandle texture{};
		/** @brief Opaque image view owned by storage; observation only. */
		detail::storage::ImageViewHandle image_view{};
		/** @brief Zero-based atlas array layer index. */
		std::uint32_t layer_index = 0;
	};
	/** Atlas allocation metrics and the first layer's opaque RGB preview. */
	struct DevAtlasView {
		/** @brief Borrowed preview binding for the first atlas layer. */
		TextureHandle texture{};
		/** @brief Opaque atlas image view owned by storage. */
		detail::storage::ImageViewHandle imageView{};
		/** @brief Atlas page width in pixels. */
		std::uint32_t width = 0;
		/** @brief Atlas page height in pixels. */
		std::uint32_t height = 0;
		/** @brief Number of populated face layers. */
		std::uint32_t layersUsed = 0;
		/** @brief Number of allocated atlas array layers. */
		std::uint32_t layersCapacity = 0;
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevFontVisitor = bool (*)(void*, const DevFontView&);
	/** @brief Return the current development-observation revision; zero when support is disabled. */
	[[nodiscard]] std::uint64_t devRevision() const noexcept;
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devFontCount() const noexcept;
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevFonts(void* userData, DevFontVisitor visitor) const;
	/** @brief Return current atlas metrics and borrowed first-layer preview; does not transfer ownership. */
	[[nodiscard]] DevAtlasView devAtlas() const noexcept;
	/** Borrow contiguous per-layer previews without copying pixels or modifying text rendering. */
	[[nodiscard]] std::span<const DevAtlasLayerView> dev_atlas_layers() const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
	/** @brief Attach a borrowed development memory recorder; null disables recording. */
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
	 * Nonempty FontFaceCreateInfo::baked_data loads borrowed .arfont bytes synchronously.
	 * Face paths ending in .arfont load baked font assets. Paths to .ttf files
	 * only work when FlowUi is built with runtime font baking enabled.
	 *
	 * @param createInfo Family name and initial concrete faces to load.
	 * @return The new family id, or a structured error when registration fails.
	 * @throws std::bad_alloc if allocating family or font data fails.
	 *
	 * @note Initialization already creates "Default"; use a distinct nonempty name
	 * for additional families. Duplicate names return FontFamilyAlreadyExists.
	 *
	 * @code{.cpp}
	 * const auto titleFamily = app.fonts().createFamily({
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
	[[nodiscard]] Result<FontFamilyId> createFamily(const FontFamilyCreateInfo& createInfo);
	/** @brief Register a family using the normalized app-shared key name instead of createInfo.name.
	 * ResourceDomain::Font is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<FontFamilyId> createFamily(ResourceKey key,
													const FontFamilyCreateInfo& createInfo);

	/**
	 * @brief Return a family id by name.
	 *
	 * This lookup lets callers cache a previously registered family id. Name
	 * normalization and lookup metadata may allocate. Missing families return
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
	[[nodiscard]] FontFamilyId getFamilyId(std::string_view familyName) const;
	/** @brief Look up an app-shared font-family key; return UINT32_MAX when absent.
	 * ResourceDomain::Font is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] FontFamilyId getFamilyId(ResourceKey key) const;

	/**
	 * @brief Add a concrete face to an existing family.
	 *
	 * The face is loaded immediately and becomes available to resolveFont() for
	 * the provided weight and style. Use this overload when the caller already
	 * cached a FontFamilyId.
	 *
	 * Nonempty FontFaceCreateInfo::baked_data loads borrowed .arfont bytes synchronously.
	 * Paths ending in .arfont load baked font assets. Paths to .ttf files only
	 * work when FlowUi is built with runtime font baking enabled.
	 *
	 * @param familyId Existing logical family id.
	 * @param createInfo Concrete face source path, pixel size, weight, style,
	 * and optional name.
	 * @return The loaded font id, or a structured error if the family is absent
	 * or the face cannot be loaded.
	 * @throws std::bad_alloc if allocating font data fails.
	 *
	 * @code{.cpp}
	 * const FlowUi::FontFamilyId bodyFamily = app.fonts().getFamilyId("Body");
	 * const auto italicFont = app.fonts().addFamilyFace(
	 *     bodyFamily,
	 *     FlowUi::FontFaceCreateInfo{
	 *         .path = "assets/fonts/Inter-Italic.arfont",
	 *         .pixelSize = 18.0f,
	 *         .weight = 400,
	 *         .style = FlowUi::FontStyle::Italic,
	 *     });
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> addFamilyFace(FontFamilyId familyId,
											   const FontFaceCreateInfo& createInfo);

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
	 * @return The loaded font id, or a structured error if the family is absent
	 * or the face cannot be loaded.
	 * @throws std::bad_alloc if allocating font data fails.
	 *
	 * @code{.cpp}
	 * const auto boldFont = app.fonts().addFamilyFace(
	 *     "Body",
	 *     FlowUi::FontFaceCreateInfo{
	 *         .path = "assets/fonts/Inter-Bold.arfont",
	 *         .pixelSize = 18.0f,
	 *         .weight = 700,
	 *         .style = FlowUi::FontStyle::Normal,
	 *     });
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> addFamilyFace(std::string_view familyName,
											   const FontFaceCreateInfo& createInfo);
	/** @brief Load a face into the app-shared family named by key; return a structured failure if absent.
	 * ResourceDomain::Font is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<FontId> addFamilyFace(ResourceKey key,
											   const FontFaceCreateInfo& createInfo);

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
	 *     *titleFamily,
	 *     700,
	 *     FlowUi::FontStyle::Normal);
	 * @endcode
	 */
	[[nodiscard]] FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const;

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
	[[nodiscard]] FontId resolveFont(std::string_view familyName, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const;
	/** @brief Resolve the app-shared family key using the weight/style fallback rules; return zero when absent.
	 * ResourceDomain::Font is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] FontId resolveFont(ResourceKey key, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const;

	/**
	 * @brief Return loaded font data by concrete font id.
	 *
	 * This exposes the baked metrics, glyph table, kerning table, and atlas
	 * placement for renderer or layout integrations that need direct access to
	 * a registered face. Normal UI code usually only needs resolveFont().
	 *
	 * @param fontId Concrete id returned by addFamilyFace() or
	 * resolveFont().
	 * @return Borrowed face pointer, or nullptr if fontId is unknown. Additional
	 * face registrations may invalidate the pointer; reacquire it after loading fonts.
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
	[[nodiscard]] const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const;

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
	[[nodiscard]] const FlowUi::Font::AtlasArrayResource& getAtlasResource() const;

private:
	friend class App;

	void init(detail::storage::IStorageSystem& storage, uint32_t atlasSize);
	void destroy() noexcept;
	[[nodiscard]] detail::manager_storage::FontFrameView frameView(
		const detail::storage::FrameToken& frame) const;

	FontId loadFontFace(const FontFaceCreateInfo& createInfo);
	FontId loadFont(const std::filesystem::path& path, float px);
	FontId registerBakedFont(const std::filesystem::path& arfontPath,
							std::string_view requestedName = {},
							std::span<const std::byte> baked_data = {});
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
