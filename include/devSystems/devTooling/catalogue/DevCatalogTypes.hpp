#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/AppActionID.hpp"
#include "FlowUi/ElementID.hpp"
#include "FlowUi/FontResources.hpp"
#include "FlowUi/ResourceKey.hpp"
#include "FlowUi/TextureHandle.hpp"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::devMode {

enum class DevResourceStatus : std::uint8_t { Unloaded, Loading, Ready, Failed, Retiring };
enum class DevAtlasKind : std::uint8_t { FontMsdfAtlas, IconVectorAtlas };
enum class DevActionKind : std::uint8_t { AppActionBinding, UiActionRecipe };

struct DevUvRect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 1.0f;
	float height = 1.0f;
};

struct DevImageCatalogEntry {
	ResourceKey key{};
	TextureHandle textureHandle{};
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t formatVulkan = 0;
	std::uint64_t gpuMemoryBytes = 0;
	std::string_view sourcePathOrDebugName{};
	DevResourceStatus status = DevResourceStatus::Ready;
	bool isAnonymous = false;
};

struct DevIconCatalogEntry {
	ResourceKey iconKey{};
	std::string_view iconName{};
	std::uint32_t targetWidth = 0;
	std::uint32_t targetHeight = 0;
	TextureHandle atlasTexture{};
	DevUvRect atlasUv{};
	bool isVectorSvg = true;
	std::uint32_t atlasLayer = 0;
};

struct DevAtlasCatalogEntry {
	ResourceKey atlasKey{};
	std::string_view debugName{};
	DevAtlasKind kind = DevAtlasKind::FontMsdfAtlas;
	TextureHandle atlasTexture{};
	detail::storage::ImageViewHandle atlasImageView{};
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t layerCount = 1;
	float msdfPixelRange = 4.0f;
	float occupancyRatio = 0.0f;
	std::uint32_t allocatedRegions = 0;
	std::uint64_t gpuMemoryBytes = 0;
};

struct DevFontCatalogEntry {
	FontId fontHandle = 0;
	FontFamilyId familyHandle = 0;
	std::string_view familyName{};
	std::string_view faceName{};
	std::string_view sourcePath{};
	std::uint16_t weight = 400;
	bool isItalic = false;
	float emSizePoints = 14.0f;
	float ascent = 0.0f;
	float descent = 0.0f;
	float lineGap = 0.0f;
	std::uint32_t loadedGlyphCount = 0;
	std::uint32_t atlasLayer = 0;
};

struct DevActionCatalogEntry {
	AppActionID actionId{};
	std::uint64_t stableId = 0;
	std::string_view debugName{};
	DevActionKind kind = DevActionKind::AppActionBinding;
	bool isBound = false;
	bool isReconstructable = false;
	std::uint8_t availabilityFlags = 0;
	std::uint64_t callableTypeHash = 0;
	std::uint64_t resultTypeHash = 0;
	std::uint64_t lifetimeInvocations = 0;
	std::string_view definitionSource{};
};

struct DevThemeCatalogEntry {
	DevTypeId themeTypeId = 0;
	std::string_view typeName{};
	std::string_view variantName{};
	bool isActive = false;
	std::uint64_t revision = 0;
	const void* payloadPointer = nullptr;
	std::size_t payloadSizeBytes = 0;
	std::size_t reflectedFieldCount = 0;
};

struct DevElementCatalogEntry {
	FlowDefinitionID definition{};
	std::string_view definitionName{};
	DevTypeId parametersTypeId = 0;
	DevTypeId stateTypeId = 0;
	std::size_t reflectedFieldCount = 0;
	std::uint32_t activeInstanceCount = 0;
	bool isBakeable = true;
	std::string_view headerSourceFile{};
};

struct DevCatalogueRevisions {
	std::uint64_t storageRevision = 0;
	std::uint64_t fontRevision = 0;
	std::uint64_t schemaRevision = 0;
	std::uint64_t treeRevision = 0;
};

} // namespace FlowUi::devMode

#endif
