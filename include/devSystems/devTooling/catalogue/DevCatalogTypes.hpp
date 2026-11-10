#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/Public/identificationTypes/AppActionID.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Observed lifecycle state of a catalogue resource. */
enum class DevResourceStatus : std::uint8_t {
	/** @brief No loaded backing resource is available. */
	Unloaded,
	/** @brief Resource creation/upload is in progress. */
	Loading,
	/** @brief Resource is available for inspection. */
	Ready,
	/** @brief Resource creation or observation failed. */
	Failed,
	/** @brief Removal has been requested while outstanding uses finish. */
	Retiring
};
/** @brief Atlas encoding used by a catalogue preview. */
enum class DevAtlasKind : std::uint8_t {
	/** @brief Font multi-channel signed-distance-field array atlas. */
	FontMsdfAtlas,
	/** @brief Rasterized SVG icon atlas page. */
	IconVectorAtlas
};
/** @brief Callable-free action catalogue category. */
enum class DevActionKind : std::uint8_t {
	/** @brief Retained semantic app action binding. */
	AppActionBinding,
	/** @brief Typed transient UI recipe. */
	UiActionRecipe
};
/** Independent cache observations, matching the query failure-bit positions. */
enum class DevCatalogueQuery : std::uint8_t {
	/** @brief Observation cache for images. */
	Images,
	/** @brief Observation cache for icons. */
	Icons,
	/** @brief Observation cache for atlases. */
	Atlases,
	/** @brief Observation cache for fonts. */
	Fonts,
	/** @brief Observation cache for actions. */
	Actions,
	/** @brief Observation cache for themes. */
	Themes,
	/** @brief Observation cache for elements. */
	Elements
};

/** @brief Normalized texture UV rectangle, expressed as origin plus extent. */
struct DevUvRect {
	/** @brief Left UV coordinate. */
	float x = 0.0f;
	/** @brief Top UV coordinate. */
	float y = 0.0f;
	/** @brief UV width. */
	float width = 1.0f;
	/** @brief UV height. */
	float height = 1.0f;
};

/** @brief Borrowed image metadata; retain an inspection lease for textures actually displayed. */
struct DevImageCatalogEntry {
	/** @brief Image resource key; its name borrows catalogue/storage string memory. */
	ResourceKey key{};
	/** @brief Borrowed source path or diagnostic image label. */
	std::string_view sourcePathOrDebugName{};
	/** @brief Source image width in pixels. */
	std::uint32_t width = 0;
	/** @brief Source image height in pixels. */
	std::uint32_t height = 0;
	/** @brief Numeric Vulkan image format. */
	std::uint32_t formatVulkan = 0;
	/** @brief Observed backing GPU allocation in bytes. */
	std::uint64_t gpuMemoryBytes = 0;
	/** @brief Generation-checked logical texture binding. */
	TextureHandle textureHandle{};
	/** @brief Observed resource lifecycle state. */
	DevResourceStatus status = DevResourceStatus::Ready;
	/** @brief Whether the image lacks an application registration name. */
	bool isAnonymous = false;
};

/** @brief Borrowed SVG raster observation with a cached atlas location. */
struct DevIconCatalogEntry {
	/** @brief Registered SVG resource identity. */
	ResourceKey iconKey{};
	/** @brief Borrowed SVG display/registration name. */
	std::string_view iconName{};
	/** @brief Raster width in pixels. */
	std::uint32_t targetWidth = 0;
	/** @brief Raster height in pixels. */
	std::uint32_t targetHeight = 0;
	/** @brief Generation-checked atlas texture binding. */
	TextureHandle atlasTexture{};
	/** @brief Normalized raster rectangle within the atlas texture. */
	DevUvRect atlasUv{};
	/** @brief Whether this entry originates from a registered SVG. */
	bool isVectorSvg = true;
	/** @brief Atlas layer/page containing the raster. */
	std::uint32_t atlasLayer = 0;
};

/** @brief Borrowed font/icon atlas allocation and occupancy observation. */
struct DevAtlasCatalogEntry {
	/** @brief Atlas resource identity with borrowed name storage. */
	ResourceKey atlasKey{};
	/** @brief Borrowed diagnostic atlas label. */
	std::string_view debugName{};
	/** @brief Font MSDF array or vector-icon atlas. */
	DevAtlasKind kind = DevAtlasKind::FontMsdfAtlas;
	/** @brief Generation-checked atlas preview texture. */
	TextureHandle atlasTexture{};
	/** @brief Opaque manager-owned image view; never destroy from tooling. */
	::FlowUi::detail::storage::ImageViewHandle atlasImageView{};
	/** @brief Atlas width in pixels. */
	std::uint32_t width = 0;
	/** @brief Atlas height in pixels. */
	std::uint32_t height = 0;
	/** @brief Number of exposed array layers. */
	std::uint32_t layerCount = 1;
	/** @brief Page/layer index represented by this entry. */
	std::uint32_t pageIndex = 0;
	/** @brief Signed-distance range in atlas pixels for MSDF rendering. */
	float msdfPixelRange = 4.0f;
	/** @brief Observed fraction of allocated atlas area. */
	float occupancyRatio = 0.0f;
	/** @brief Number of occupied allocation regions. */
	std::uint32_t allocatedRegions = 0;
	/** @brief Observed GPU backing allocation in bytes. */
	std::uint64_t gpuMemoryBytes = 0;
};

/** @brief Borrowed face metadata and glyph metrics from the font catalogue. */
struct DevFontCatalogEntry {
	/** @brief Concrete Clay font identity. */
	FontId fontHandle = 0;
	/** @brief Owning logical font family identity. */
	FontFamilyId familyHandle = 0;
	/** @brief Borrowed logical family name. */
	std::string_view familyName{};
	/** @brief Borrowed concrete face name. */
	std::string_view faceName{};
	/** @brief Borrowed original face asset path. */
	std::string_view sourcePath{};
	/** @brief Registered CSS-style font weight. */
	std::uint16_t weight = 400;
	/** @brief Whether this face uses the italic style. */
	bool isItalic = false;
	/** @brief Registered em size in points. */
	float emSizePoints = 14.0f;
	/** @brief Registered ascent metric. */
	float ascent = 0.0f;
	/** @brief Registered descent metric. */
	float descent = 0.0f;
	/** @brief Registered additional line gap. */
	float lineGap = 0.0f;
	/** @brief Number of registered glyphs for this face. */
	std::uint32_t loadedGlyphCount = 0;
	/** @brief Atlas array layer containing this face. */
	std::uint32_t atlasLayer = 0;
};

/** @brief Callable-free action observation safe for catalogue browsing. */
struct DevActionCatalogEntry {
	/** @brief App action identity; zero for a UI recipe. */
	AppActionID actionId{};
	/** @brief App identity or UI recipe identity, according to kind. */
	std::uint64_t stableId = 0;
	/** @brief Borrowed diagnostic action/recipe name. */
	std::string_view debugName{};
	/** @brief Retained app binding or UI recipe. */
	DevActionKind kind = DevActionKind::AppActionBinding;
	/** @brief Whether the app binding or recipe is currently registered. */
	bool isBound = false;
	/** @brief Whether tooling may create a call without borrowing runtime resources. */
	bool isReconstructable = false;
	/** @brief Observed compact action availability flags. */
	std::uint8_t availabilityFlags = 0;
	/** @brief Process-local callable type fingerprint. */
	std::uint64_t callableTypeHash = 0;
	/** @brief Process-local result type fingerprint; zero denotes void. */
	std::uint64_t resultTypeHash = 0;
	/** @brief Observed invocation count for this binding. */
	std::uint64_t lifetimeInvocations = 0;
	/** @brief Borrowed source location text, when available. */
	std::string_view definitionSource{};
};

/** @brief Borrowed named theme variant and reflected payload observation. */
struct DevThemeCatalogEntry {
	/** @brief Reflected theme type identity. */
	DevTypeId themeTypeId = 0;
	/** @brief Borrowed reflected type name. */
	std::string_view typeName{};
	/** @brief Borrowed registered variant name. */
	std::string_view variantName{};
	/** @brief Whether this variant is active for its theme type. */
	bool isActive = false;
	/** @brief Theme publication revision. */
	std::uint64_t revision = 0;
	/** @brief Borrowed payload; cast only through its matching reflected schema. */
	const void* payloadPointer = nullptr;
	/** @brief Payload storage size in bytes. */
	std::size_t payloadSizeBytes = 0;
	/** @brief Number of declared reflected root fields. */
	std::size_t reflectedFieldCount = 0;
};

/** @brief Borrowed definition metadata; querying never invokes element construction. */
struct DevElementCatalogEntry {
	/** @brief Stable Flow element definition identity. */
	FlowDefinitionID definition{};
	/** @brief Borrowed definition display name. */
	std::string_view definitionName{};
	/** @brief Reflected parameter type identity. */
	DevTypeId parametersTypeId = 0;
	/** @brief Reflected state type identity, or zero when absent. */
	DevTypeId stateTypeId = 0;
	/** @brief Number of declared parameter fields. */
	std::size_t reflectedFieldCount = 0;
	/** @brief Observed instance count in the supplied completed tree. */
	std::uint32_t activeInstanceCount = 0;
	/** @brief Whether the definition has metadata needed for baked assignments. */
	bool isBakeable = true;
	/** @brief Borrowed source header used for generated assignments. */
	std::string_view headerSourceFile{};
};

/** @brief Source revisions used to determine whether borrowed catalogue caches need refresh. */
struct DevCatalogueRevisions {
	/** @brief Last observed resource-storage publication revision. */
	std::uint64_t storageRevision = 0;
	/** @brief Last observed font catalogue revision. */
	std::uint64_t fontRevision = 0;
	/** @brief Last observed reflected-schema generation. */
	std::uint64_t schemaRevision = 0;
	/** @brief Last observed completed UI tree generation. */
	std::uint64_t treeRevision = 0;
};

} // namespace FlowUi::devSystems::tooling

#endif
