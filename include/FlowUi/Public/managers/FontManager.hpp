#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
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
namespace detail::font {
struct FontCpuAsset;
struct FontManagerAccess;
} // namespace detail::font
namespace detail::storage {
class StorageBinding;
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
 * @brief App-shared font-family registration and face resolution.
 *
 * Recoverable operations return checked Result/Status values. A successful bool
 * payload may be false; inspect the payload only after checking the Result.
 * Obtain App-owned managers through the checked App getter and unwrap its
 * reference_wrapper with result->get().
 * Returned text, metadata, resource views, and pointers remain borrowed under
 * each operation's documented invalidation rules.
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
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts loaded face rows; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t count = 0;
	 * auto result = manager.visitDevFonts(&count,
	 *     [](void* target, const FlowUi::FontManager::DevFontView& face) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (face.face != nullptr) {
	 *             ++observedCount;
	 *         }
	 *         return true;
	 *     });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool completed = *result;
	 * #endif
	 * @endcode
	 */
	Result<bool> visitDevFonts(void* userData, DevFontVisitor visitor) const noexcept;
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
	 * @brief Create a font family and synchronously load its initial faces.
	 *
	 * Faces accept baked .arfont assets or borrowed baked_data; source-font baking depends on FLOWUI_RUNTIME_FONT_BAKING. Borrowed bytes are consumed during this call. Family and face IDs remain valid for this manager's lifetime.
	 *
	 * @return The value is the stable family ID. Duplicate names are FontFamilyAlreadyExists errors. An empty createInfo name uses Default, which startup normally already registered. The ResourceKey overload uses its normalized key name instead of createInfo.name. A failed family registration rolls back the candidate family and faces.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * FlowUi::FontFamilyCreateInfo createInfo{};
	 * createInfo.name = "Body";
	 * auto result = manager.createFamily(createInfo);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto familyId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontFamilyId> createFamily(const FontFamilyCreateInfo& createInfo) noexcept;
	/**
	 * @brief Create a font family and synchronously load its initial faces.
	 *
	 * Faces accept baked .arfont assets or borrowed baked_data; source-font baking depends on FLOWUI_RUNTIME_FONT_BAKING. Borrowed bytes are consumed during this call. Family and face IDs remain valid for this manager's lifetime. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return The value is the stable family ID. Duplicate names are FontFamilyAlreadyExists errors. An empty createInfo name uses Default, which startup normally already registered. The ResourceKey overload uses its normalized key name instead of createInfo.name. A failed family registration rolls back the candidate family and faces.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "Body"};
	 * FlowUi::FontFamilyCreateInfo createInfo{};
	 * createInfo.name = "Body";
	 * auto result = manager.createFamily(key, createInfo);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto familyId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontFamilyId> createFamily(ResourceKey key,
													const FontFamilyCreateInfo& createInfo) noexcept;

	/**
	 * @brief Look up a family ID without loading a font.
	 *
	 * String family lookup is exact; ResourceKey lookup additionally validates domain and scope.
	 *
	 * @return A missing family is a successful std::numeric_limits<FontFamilyId>::max() value, not an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.getFamilyId("Body");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto familyId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontFamilyId> getFamilyId(std::string_view familyName) const noexcept;
	/**
	 * @brief Look up a family ID without loading a font.
	 *
	 * String family lookup is exact; ResourceKey lookup additionally validates domain and scope. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return A missing family is a successful std::numeric_limits<FontFamilyId>::max() value, not an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "Body"};
	 * auto result = manager.getFamilyId(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto familyId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontFamilyId> getFamilyId(ResourceKey key) const noexcept;

	/**
	 * @brief Synchronously load and append a face to an existing family.
	 *
	 * The manager owns loaded bytes. FontFaceCreateInfo::baked_data is borrowed only for this call; adding faces can invalidate previously borrowed FontFaceData pointers.
	 *
	 * @return The value is a concrete FontId; a missing family is FontFamilyNotFound.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * FlowUi::FontFaceCreateInfo face{};
	 * face.path = "assets/fonts/Inter.arfont";
	 * auto result = manager.addFamilyFace(familyId, face);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto fontId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> addFamilyFace(FontFamilyId familyId,
											   const FontFaceCreateInfo& createInfo) noexcept;

	/**
	 * @brief Synchronously load and append a face to an existing family.
	 *
	 * The manager owns loaded bytes. FontFaceCreateInfo::baked_data is borrowed only for this call; adding faces can invalidate previously borrowed FontFaceData pointers.
	 *
	 * @return The value is a concrete FontId; a missing family is FontFamilyNotFound.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * FlowUi::FontFaceCreateInfo face{};
	 * face.path = "assets/fonts/Inter.arfont";
	 * auto result = manager.addFamilyFace("Body", face);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto fontId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> addFamilyFace(std::string_view familyName,
											   const FontFaceCreateInfo& createInfo) noexcept;
	/**
	 * @brief Synchronously load and append a face to an existing family.
	 *
	 * The manager owns loaded bytes. FontFaceCreateInfo::baked_data is borrowed only for this call; adding faces can invalidate previously borrowed FontFaceData pointers. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return The value is a concrete FontId; a missing family is FontFamilyNotFound.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "Body"};
	 * FlowUi::FontFaceCreateInfo face{};
	 * face.path = "assets/fonts/Inter.arfont";
	 * auto result = manager.addFamilyFace(key, face);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto fontId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> addFamilyFace(ResourceKey key,
											   const FontFaceCreateInfo& createInfo) noexcept;

	/**
	 * @brief Select a registered face for a family, weight, and style.
	 *
	 * Defaults are weight 400 and FontStyle::Normal. This query does not load or bake fonts.
	 *
	 * @return Success contains the nearest weight within the requested style (first registered face wins ties); if that style is absent, the first family face is used. A missing or empty family successfully resolves to fallback FontId 0.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.resolveFont(familyId, 400, FlowUi::FontStyle::Normal);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto fontId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> resolveFont(FontFamilyId familyId, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const noexcept;

	/**
	 * @brief Select a registered face for a family, weight, and style.
	 *
	 * Defaults are weight 400 and FontStyle::Normal. This query does not load or bake fonts.
	 *
	 * @return Success contains the nearest weight within the requested style (first registered face wins ties); if that style is absent, the first family face is used. A missing or empty family successfully resolves to fallback FontId 0.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.resolveFont("Body", 400, FlowUi::FontStyle::Normal);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto fontId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> resolveFont(std::string_view familyName, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const noexcept;
	/**
	 * @brief Select a registered face for a family, weight, and style.
	 *
	 * Defaults are weight 400 and FontStyle::Normal. This query does not load or bake fonts. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains the nearest weight within the requested style (first registered face wins ties); if that style is absent, the first family face is used. A missing or empty family successfully resolves to fallback FontId 0.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "Body"};
	 * auto result = manager.resolveFont(key, 400, FlowUi::FontStyle::Normal);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto fontId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<FontId> resolveFont(ResourceKey key, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const noexcept;

	/**
	 * @brief Borrow a loaded face and its metrics.
	 *
	 * Additional font registrations can reallocate face storage and invalidate the pointer. Reacquire it after loading fonts; do not retain it beyond the manager.
	 *
	 * @return Returns nullptr when the concrete ID is unknown, including on a detached manager.
	 */
	[[nodiscard]] const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const noexcept;

	/**
	 * @brief Borrow the current Vulkan font atlas resource.
	 *
	 * Unwrap with result->get() after checking the Result. The manager owns the atlas and Vulkan handles; never destroy them. The resource reference lasts until manager destruction/reinitialization, while bindingRevision changes when descriptor bindings need refreshing.
	 *
	 * @return Success contains `std::reference_wrapper<const Font::AtlasArrayResource>`; a detached manager returns ObjectNotInitialized.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.getAtlasResource();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::Font::AtlasArrayResource& borrowed = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const FlowUi::Font::AtlasArrayResource>> getAtlasResource() const noexcept;

private:
	friend class App;
	friend struct detail::font::FontManagerAccess;

	void init(detail::storage::StorageBinding& storage, uint32_t atlasSize);
	void destroy() noexcept;
	[[nodiscard]] detail::manager_storage::FontFrameView frameView(
		const detail::storage::FrameToken& frame) const;

	FontId loadFontFace(const FontFaceCreateInfo& createInfo);
	FontId loadFont(const std::filesystem::path& path, float px);
	FontId registerBakedFont(const std::filesystem::path& arfontPath,
							std::string_view requestedName = {},
							std::span<const std::byte> baked_data = {});
	FontId registerRuntimeFont(const FontFaceCreateInfo& createInfo);
	FontId registerCpuFont(detail::font::FontCpuAsset asset,
						   const std::filesystem::path& sourcePath, std::string_view requestedName,
						   ErrorSite operationSite);

	detail::storage::StorageBinding* storage_ = nullptr;
	uint64_t controllerHandle_ = 0;
	detail::manager_storage::FontCatalogController* controller_ = nullptr;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevMemoryRecorder* devMemoryRecorder_ = nullptr;
#endif
};

/** @} */

} // namespace FlowUi
