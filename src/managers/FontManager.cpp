#include "managers/FontManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <artery-font/stdio-serialization.h>
#include <artery-font/std-artery-font.h>
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"

namespace Font = FlowUi::Font;

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void FontManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!controller_) return;
	try {
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.liveBytes += controller_->families.size() * sizeof(detail::manager_storage::FontFamilyRecord);
		memory.capacityBytes += memory.liveBytes;
		memory.objectCount += controller_->families.size();
		memory.capacityCount += controller_->families.size();
		memory.addNodeContainer(controller_->familyIdByName);
		memory.liveBytes += controller_->fonts.size() * sizeof(Font::FontFaceData);
		memory.capacityBytes += controller_->fonts.size() * sizeof(Font::FontFaceData);
		memory.objectCount += controller_->fonts.size();
		memory.capacityCount += controller_->fonts.size();
		memory.addNodeContainer(controller_->fontIndexById);
		memory.addNodeContainer(controller_->fontIdByName);
		memory.add(controller_->atlasLayerPixels);
		for (const auto& family : controller_->families) {
			memory.add(family.name);
			memory.add(family.faces);
		}
		for (const auto& layer : controller_->atlasLayerPixels) memory.add(layer);
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kFonts.id, memory);
		devSystems::DevContainerMemoryAccumulator atlasPixels{};
		atlasPixels.add(controller_->atlasLayerPixels);
		for (const auto& layer : controller_->atlasLayerPixels) atlasPixels.add(layer);
		atlasPixels.flags = devSystems::MemorySampleFlag::None;
		devSystems::appendManagerSample(
			sink, devSystems::memory_sources::kFontAtlasCpuPixels.id, atlasPixels);
	} catch (...) {}
}

void FontManager::setDevMemoryRecorder(devSystems::DevMemoryRecorder* recorder) noexcept {
	devMemoryRecorder_ = recorder;
	if (controller_) controller_->setDevMemoryRecorder(recorder);
}
#endif

namespace manager_storage = detail::manager_storage;
namespace key_storage = detail::managerStorage;
namespace storage = detail::storage;

namespace {

#if defined(FLOWUI_RUNTIME_FONT_BAKING)
constexpr double kDefaultRuntimeFontPxRange = 6.0;
constexpr double kDefaultRuntimeFontAngleThreshold = 3.0;
constexpr double kDefaultRuntimeFontMiterLimit = 1.0;
constexpr unsigned long long kRuntimeFontLcgMultiplier = 6364136223846793005ull;
constexpr unsigned long long kRuntimeFontLcgIncrement = 1442695040888963407ull;
#endif

struct DecodedAtlasImage {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> rgbaPixels;
};

template <typename T>
const T* listData(const artery_font::StdList<T>& list) {
	return static_cast<const T*>(list);
}

std::string toStdString(const artery_font::StdString& input) {
	const char* value = static_cast<const char*>(input);
	return value ? std::string(value) : std::string{};
}

std::string toLowerAscii(std::string text) {
	for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return text;
}

bool isArfontPath(const std::filesystem::path& path) {
	return toLowerAscii(path.extension().string()) == ".arfont";
}

bool supportsImageEncoding(artery_font::ImageEncoding encoding) {
	return encoding == artery_font::IMAGE_PNG || encoding == artery_font::IMAGE_RAW_BINARY;
}

int pickAtlasImageIndex(const artery_font::StdArteryFont<float>& font) {
	if (font.images.length() <= 0) {
		return -1;
	}
	const auto* images = listData(font.images);
	const auto* variants = listData(font.variants);

	if (font.variants.length() > 0) {
		const artery_font::ImageType preferredType = variants[0].imageType;
		for (int i = 0; i < font.images.length(); ++i) {
			if (images[i].imageType == preferredType && supportsImageEncoding(images[i].encoding)) {
				return i;
			}
		}
	}

	for (int i = 0; i < font.images.length(); ++i) {
		if (supportsImageEncoding(images[i].encoding)) {
			return i;
		}
	}

	return -1;
}

DecodedAtlasImage decodeImageToRgba8(
	const artery_font::StdArteryFont<float>::Image& image
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	, devSystems::DevMemoryRecorder* memoryRecorder
#endif
	) {
	DecodedAtlasImage decoded{};
	decoded.width = image.width;
	decoded.height = image.height;

	if (decoded.width == 0 || decoded.height == 0) {
		throw std::runtime_error(".arfont image has zero dimensions.");
	}

	const auto* sourceDataBytes = static_cast<const unsigned char*>(image.data);
	const auto* sourceData = reinterpret_cast<const uint8_t*>(sourceDataBytes);
	const size_t sourceDataSize = static_cast<size_t>(image.data.length());

	switch (image.encoding) {
		case artery_font::IMAGE_PNG: {
			if (!sourceData || sourceDataSize == 0) {
				throw std::runtime_error(".arfont PNG image payload is empty.");
			}
			if (sourceDataSize > static_cast<size_t>(std::numeric_limits<int>::max())) {
				throw std::runtime_error(".arfont PNG payload is too large for stb_image.");
			}

			int decodedWidth = 0;
			int decodedHeight = 0;
			int decodedChannels = 0;
			stbi_uc* rgbaPixels = stbi_load_from_memory(
				sourceData,
				static_cast<int>(sourceDataSize),
				&decodedWidth,
				&decodedHeight,
				&decodedChannels,
				4);
			if (!rgbaPixels) {
				const char* reason = stbi_failure_reason();
				throw std::runtime_error(
					std::string("Failed to decode .arfont PNG image: ") + (reason ? reason : "unknown error"));
			}

			const size_t decodedBytes = static_cast<size_t>(decodedWidth) * static_cast<size_t>(decodedHeight) * 4u;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
			devSystems::DevExternalMemoryScope decodedMemory(
				memoryRecorder, devSystems::memory_sources::kFontDecode.id, decodedBytes);
#endif
			decoded.rgbaPixels.resize(decodedBytes);
			std::memcpy(decoded.rgbaPixels.data(), rgbaPixels, decodedBytes);
			stbi_image_free(rgbaPixels);

			decoded.width = static_cast<uint32_t>(decodedWidth);
			decoded.height = static_cast<uint32_t>(decodedHeight);

			if (decoded.width != image.width || decoded.height != image.height) {
				throw std::runtime_error(".arfont PNG decoded dimensions do not match image metadata.");
			}
			break;
		}

		case artery_font::IMAGE_RAW_BINARY: {
			if (image.pixelFormat != artery_font::PIXEL_UNSIGNED8) {
				throw std::runtime_error("Only unsigned 8-bit raw .arfont atlas images are supported.");
			}
			if (image.channels != 1 && image.channels != 3 && image.channels != 4) {
				throw std::runtime_error("Only 1/3/4 channel raw .arfont atlas images are supported.");
			}

			const size_t pixelRowBytes = static_cast<size_t>(image.width) * static_cast<size_t>(image.channels);
			const size_t sourceRowBytes = (image.rawBinaryFormat.rowLength != 0)
				? static_cast<size_t>(image.rawBinaryFormat.rowLength)
				: pixelRowBytes;

			if (sourceRowBytes < pixelRowBytes) {
				throw std::runtime_error(".arfont raw image row length is smaller than required.");
			}

			const size_t requiredBytes = sourceRowBytes * static_cast<size_t>(image.height);
			if (!sourceData || sourceDataSize < requiredBytes) {
				throw std::runtime_error(".arfont raw image payload is too small.");
			}

			decoded.rgbaPixels.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4u);
			const bool topDown = image.rawBinaryFormat.orientation != artery_font::ORIENTATION_BOTTOM_UP;

			for (uint32_t y = 0; y < image.height; ++y) {
				const uint32_t sourceY = topDown ? y : (image.height - 1u - y);
				const uint8_t* srcRow = sourceData + static_cast<size_t>(sourceY) * sourceRowBytes;
				uint8_t* dstRow = decoded.rgbaPixels.data() + static_cast<size_t>(y) * static_cast<size_t>(image.width) * 4u;

				for (uint32_t x = 0; x < image.width; ++x) {
					const uint8_t* src = srcRow + static_cast<size_t>(x) * image.channels;
					uint8_t* dst = dstRow + static_cast<size_t>(x) * 4u;
					if (image.channels == 1) {
						dst[0] = src[0];
						dst[1] = src[0];
						dst[2] = src[0];
						dst[3] = 255;
					} else if (image.channels == 3) {
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = 255;
					} else {
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = src[3];
					}
				}
			}
			break;
		}

		default:
			throw std::runtime_error("Unsupported .arfont image encoding.");
	}

	return decoded;
}

std::vector<uint8_t> copyAtlasIntoPage(
	const DecodedAtlasImage& source,
	uint32_t pageWidth,
	uint32_t pageHeight,
	const std::filesystem::path& sourcePath) {
	if (pageWidth == 0 || pageHeight == 0) {
		throw std::runtime_error("Font atlas page size must be greater than zero.");
	}
	if (source.width > pageWidth || source.height > pageHeight) {
		throw std::runtime_error(
			".arfont atlas " + std::to_string(source.width) + "x" + std::to_string(source.height) +
			" is larger than configured ui.fontAtlasSize=" + std::to_string(pageWidth) +
			" for " + sourcePath.string());
	}

	const size_t pageRowBytes = static_cast<size_t>(pageWidth) * 4u;
	const size_t sourceRowBytes = static_cast<size_t>(source.width) * 4u;
	std::vector<uint8_t> pagePixels(static_cast<size_t>(pageWidth) * static_cast<size_t>(pageHeight) * 4u, 0u);

	for (uint32_t y = 0; y < source.height; ++y) {
		const size_t sourceOffset = static_cast<size_t>(y) * sourceRowBytes;
		const size_t pageOffset = static_cast<size_t>(y) * pageRowBytes;
		std::memcpy(pagePixels.data() + pageOffset, source.rgbaPixels.data() + sourceOffset, sourceRowBytes);
	}

	return pagePixels;
}

std::string makeUniqueFontName(
	std::string baseName,
	const std::unordered_map<std::string, FontManager::FontId>& existingNames) {
	if (baseName.empty()) {
		baseName = "font";
	}

	if (existingNames.find(baseName) == existingNames.end()) {
		return baseName;
	}

	for (uint32_t suffix = 1; suffix < std::numeric_limits<uint32_t>::max(); ++suffix) {
		const std::string candidate = baseName + "_" + std::to_string(suffix);
		if (existingNames.find(candidate) == existingNames.end()) {
			return candidate;
		}
	}
	throw std::runtime_error("Could not generate a unique font name.");
}

} // namespace

void FontManager::init(storage::IStorageSystem& storageSystem, uint32_t atlasSize) {
	destroy();
	const storage::StringId name = storageSystem.intern("flowui.font.catalog");
	const storage::ResourceKey key{storage::ResourceDomain::Font, name, InvalidWindowId};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::FontCatalogController>(
		storageSystem, key, storage::ResourceKind::FontFamily, name,
		std::ref(storageSystem), atlasSize);
	storage_ = &storageSystem;
	controllerHandle_ = handle.packed();
	controller_ = manager_storage::state<manager_storage::FontCatalogController>(
		storage_, handle, storage::ResourceKind::FontFamily);
	if (!controller_) {
		destroy();
		throw std::runtime_error("Font catalog storage publication failed.");
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	controller_->setDevMemoryRecorder(devMemoryRecorder_);
#endif
}

FontManager::FontFamilyId FontManager::createFamily(const FontFamilyCreateInfo& createInfo) {
	if (!controller_ || !storage_) throw std::runtime_error("FontManager is not initialized.");
	std::string familyName = createInfo.name.empty() ? std::string("Default") : createInfo.name;
	if (controller_->familyIdByName.find(familyName) != controller_->familyIdByName.end()) {
		throw std::runtime_error("Font family already exists: " + familyName);
	}

	const size_t oldFontCount = controller_->fonts.size();
	const size_t oldLayerCount = controller_->atlasLayerPixels.size();
	const FontId oldNextFontId = controller_->nextFontId;
	const FontFamilyId familyId = static_cast<FontFamilyId>(controller_->families.size());
	controller_->familyIdByName.reserve(controller_->familyIdByName.size() + 1u);
	manager_storage::FontFamilyRecord family{};
	family.id = familyId;
	family.name = std::move(familyName);
	family.faces.reserve(createInfo.faces.size());
	controller_->families.push_back(std::move(family));
	try {
		controller_->familyIdByName.emplace(controller_->families.back().name, familyId);
		controller_->familyTransaction = true;
		for (const FontFaceCreateInfo& faceInfo : createInfo.faces) addFamilyFace(familyId, faceInfo);
		controller_->familyTransaction = false;
	} catch (...) {
		controller_->familyTransaction = false;
		for (size_t index = oldFontCount; index < controller_->fonts.size(); ++index) {
			controller_->fontIndexById.erase(controller_->fonts[index].id);
			controller_->fontIdByName.erase(controller_->fonts[index].name);
		}
		controller_->fonts.resize(oldFontCount);
		controller_->nextFontId = oldNextFontId;
		controller_->atlasLayerPixels.resize(oldLayerCount);
		controller_->borrowedAtlas.layersUsed = static_cast<uint32_t>(oldLayerCount);
		controller_->familyIdByName.erase(controller_->families.back().name);
		controller_->families.pop_back();
		throw;
	}
	++controller_->catalogRevision;
	storage_->noteManagerMutation(InvalidWindowId);
	return familyId;
}

FontManager::FontFamilyId FontManager::createFamily(ResourceKey key, const FontFamilyCreateInfo& createInfo) {
	const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
		*storage_, key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	FontFamilyCreateInfo normalizedInfo = createInfo;
	normalizedInfo.name = std::string(storage_->string(normalized.name));
	return createFamily(normalizedInfo);
}

FontManager::FontFamilyId FontManager::getFamilyId(std::string_view familyName) const {
	const auto it = controller_->familyIdByName.find(std::string(familyName));
	return (it != controller_->familyIdByName.end()) ? it->second : std::numeric_limits<FontFamilyId>::max();
}

FontManager::FontFamilyId FontManager::getFamilyId(ResourceKey key) const {
	const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
		*storage_, key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	return getFamilyId(storage_->string(normalized.name));
}

FontManager::FontId FontManager::addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo) {
	if (familyId >= controller_->families.size()) {
		throw std::runtime_error("Font family id does not exist.");
	}

	controller_->families[familyId].faces.reserve(controller_->families[familyId].faces.size() + 1u);
	const FontId fontId = loadFontFace(createInfo);
	controller_->families[familyId].faces.push_back(manager_storage::FontFamilyFaceRecord{
		.fontId = fontId,
		.weight = createInfo.weight,
		.style = createInfo.style,
	});
	if (!controller_->familyTransaction) {
		++controller_->catalogRevision;
		storage_->noteManagerMutation(InvalidWindowId);
	}
	return fontId;
}

FontManager::FontId FontManager::addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo) {
	const FontFamilyId familyId = getFamilyId(familyName);
	if (familyId == std::numeric_limits<FontFamilyId>::max()) {
		throw std::runtime_error("Font family does not exist: " + std::string(familyName));
	}
	return addFamilyFace(familyId, createInfo);
}

FontManager::FontId FontManager::addFamilyFace(ResourceKey key, const FontFaceCreateInfo& createInfo) {
	const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
		*storage_, key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	return addFamilyFace(storage_->string(normalized.name), createInfo);
}

FontManager::FontId FontManager::resolveFont(FontFamilyId familyId, uint32_t weight, FontStyle style) const {
	if (familyId >= controller_->families.size()) {
		return 0;
	}

	const manager_storage::FontFamilyRecord& family = controller_->families[familyId];
	const manager_storage::FontFamilyFaceRecord* bestFace = nullptr;
	uint32_t bestDistance = std::numeric_limits<uint32_t>::max();

	for (const manager_storage::FontFamilyFaceRecord& face : family.faces) {
		if (face.style != style) {
			continue;
		}
		const uint32_t distance = (face.weight > weight) ? (face.weight - weight) : (weight - face.weight);
		if (!bestFace || distance < bestDistance) {
			bestFace = &face;
			bestDistance = distance;
		}
	}

	if (bestFace) {
		return bestFace->fontId;
	}
	if (!family.faces.empty()) {
		return family.faces.front().fontId;
	}
	return 0;
}

FontManager::FontId FontManager::resolveFont(std::string_view familyName, uint32_t weight, FontStyle style) const {
	const FontFamilyId familyId = getFamilyId(familyName);
	return (familyId != std::numeric_limits<FontFamilyId>::max()) ? resolveFont(familyId, weight, style) : 0;
}

FontManager::FontId FontManager::resolveFont(ResourceKey key, uint32_t weight, FontStyle style) const {
	const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
		*storage_, key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	return resolveFont(storage_->string(normalized.name), weight, style);
}

FontManager::FontId FontManager::loadFontFace(const FontFaceCreateInfo& createInfo) {
	if (createInfo.path.empty()) {
		throw std::runtime_error("Font face path must not be empty.");
	}
	if (isArfontPath(createInfo.path)) {
		return registerBakedFont(createInfo.path.string(), createInfo.name);
	}
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	return registerRuntimeFont(createInfo);
#else
	return loadFont(createInfo.path.string(), createInfo.pixelSize);
#endif
}

FontManager::FontId FontManager::loadFont(std::string_view path, float px) {
	(void)px;
	if (!controller_) {
		throw std::runtime_error("FontManager is not initialized.");
	}
	const std::filesystem::path fontPath(path);
	if (fontPath.empty()) {
		throw std::runtime_error("Font path must not be empty.");
	}

	if (isArfontPath(fontPath)) {
		return registerBakedFont(path);
	}

#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	FontFaceCreateInfo createInfo{};
	createInfo.path = fontPath;
	createInfo.pixelSize = px;
	return registerRuntimeFont(createInfo);
#else
	throw std::runtime_error("Unsupported font file type: " + fontPath.string());
#endif
}

FontManager::FontId FontManager::registerRuntimeFont(const FontFaceCreateInfo& createInfo) {
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	if (!controller_) {
		throw std::runtime_error("[Flow Ui]: FontManager is not initialized.");
	}
	if (createInfo.path.empty()) {
		throw std::runtime_error("[Flow Ui]: Runtime font path must not be empty.");
	}
	if (createInfo.pixelSize <= 0.0f) {
		throw std::runtime_error("[Flow Ui]: Runtime font pixel size must be greater than zero.");
	}
	if (!std::filesystem::is_regular_file(createInfo.path)) {
		throw std::runtime_error("[Flow Ui]: Font file does not exist: " + createInfo.path.string());
	}
	if (controller_->atlasSizeHint == 0 || controller_->atlasSizeHint > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
		throw std::runtime_error("[Flow Ui]: Runtime font atlas page size is invalid.");
	}

	struct FreetypeGuard {
		msdfgen::FreetypeHandle* handle = nullptr;
		~FreetypeGuard() {
			if (handle) {
				msdfgen::deinitializeFreetype(handle);
			}
		}
	};
	struct FontGuard {
		msdfgen::FontHandle* handle = nullptr;
		~FontGuard() {
			if (handle) {
				msdfgen::destroyFont(handle);
			}
		}
	};

	FreetypeGuard freetype{};
	freetype.handle = msdfgen::initializeFreetype();
	if (!freetype.handle) {
		throw std::runtime_error("Failed to initialize FreeType for runtime font baking.");
	}

	FontGuard font{};
	const std::string pathString = createInfo.path.string();
	font.handle = msdfgen::loadFont(freetype.handle, pathString.c_str());
	if (!font.handle) {
		throw std::runtime_error("Failed to load runtime font file: " + pathString);
	}

	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry fontGeometry(&glyphs);
	const int glyphsLoaded = fontGeometry.loadCharset(
		font.handle,
		1.0,
		msdf_atlas::Charset::ASCII,
		false,
		true);

	if (glyphsLoaded < 0) {
		throw std::runtime_error("Failed to load glyph geometry from runtime font: " + pathString);
	}
	if (glyphsLoaded == 0 || glyphs.empty()) {
		throw std::runtime_error("No glyphs were loaded from runtime font: " + pathString);
	}

	unsigned long long glyphSeed = 0;
	for (msdf_atlas::GlyphGeometry& glyph : glyphs) {
		glyphSeed = glyphSeed * kRuntimeFontLcgMultiplier + kRuntimeFontLcgIncrement;
		glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, kDefaultRuntimeFontAngleThreshold, glyphSeed);
	}

	const uint32_t pageWidth = controller_->atlasSizeHint;
	const uint32_t pageHeight = controller_->atlasSizeHint;
	msdf_atlas::TightAtlasPacker packer;
	packer.setDimensions(static_cast<int>(pageWidth), static_cast<int>(pageHeight));
	packer.setSpacing(2);
	packer.setScale(static_cast<double>(createInfo.pixelSize));
	packer.setPixelRange(kDefaultRuntimeFontPxRange);
	packer.setMiterLimit(kDefaultRuntimeFontMiterLimit);
	packer.setOriginPixelAlignment(false, true);

	const int remaining = packer.pack(glyphs.data(), static_cast<int>(glyphs.size()));
	if (remaining < 0) {
		throw std::runtime_error("Failed to pack runtime font glyphs into atlas: " + pathString);
	}
	if (remaining > 0) {
		throw std::runtime_error(
			"Could not fit " + std::to_string(remaining) +
			" runtime font glyphs into configured ui.fontAtlasSize=" + std::to_string(pageWidth) +
			" for " + pathString);
	}

	msdf_atlas::GeneratorAttributes attributes;
	attributes.config.overlapSupport = true;
	attributes.scanlinePass = true;

	using MtsdfGenerator = msdf_atlas::ImmediateAtlasGenerator<
		float,
		4,
		msdf_atlas::mtsdfGenerator,
		msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;

	MtsdfGenerator generator(static_cast<int>(pageWidth), static_cast<int>(pageHeight));
	generator.setAttributes(attributes);
	generator.setThreadCount(static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
	generator.generate(glyphs.data(), glyphs.size());

	msdfgen::BitmapConstSection<msdf_atlas::byte, 4> atlasBitmap =
		static_cast<msdfgen::BitmapConstSection<msdf_atlas::byte, 4>>(generator.atlasStorage());
	atlasBitmap.reorient(msdfgen::Y_DOWNWARD);

	std::vector<uint8_t> pagePixels(static_cast<size_t>(pageWidth) * static_cast<size_t>(pageHeight) * 4u);
	const size_t rowBytes = static_cast<size_t>(pageWidth) * 4u;
	for (uint32_t y = 0; y < pageHeight; ++y) {
		std::memcpy(
			pagePixels.data() + static_cast<size_t>(y) * rowBytes,
			atlasBitmap(0, static_cast<int>(y)),
			rowBytes);
	}

	const uint32_t assignedLayer = static_cast<uint32_t>(controller_->atlasLayerPixels.size());

	Font::FontFaceData fontFace{};
	if (controller_->nextFontId == std::numeric_limits<FontId>::max()) {
		throw std::runtime_error("FlowUi font id limit exceeded.");
	}
	fontFace.id = controller_->nextFontId;
	fontFace.sourcePath = createInfo.path;
	fontFace.atlasLayer = assignedLayer;
	fontFace.atlasWidth = pageWidth;
	fontFace.atlasHeight = pageHeight;
	fontFace.sourceAtlasX = 0;
	fontFace.sourceAtlasY = 0;
	fontFace.sourceAtlasWidth = pageWidth;
	fontFace.sourceAtlasHeight = pageHeight;
	fontFace.imageType = static_cast<uint32_t>(artery_font::IMAGE_MTSDF);
	fontFace.metadata = "runtime-msdf";
	fontFace.defaultVariantIndex = 0;

	Font::FontVariantData variant{};
	variant.weight = createInfo.weight;
	variant.fontSizePx = static_cast<float>(packer.getScale());
	const msdfgen::Range finalPxRange = packer.getPixelRange();
	variant.distanceRange = static_cast<float>(finalPxRange.upper - finalPxRange.lower);
	variant.distanceRangeMiddle = static_cast<float>(0.5 * (finalPxRange.lower + finalPxRange.upper));

	const msdfgen::FontMetrics& metrics = fontGeometry.getMetrics();
	variant.emSize = static_cast<float>(metrics.emSize);
	variant.ascender = static_cast<float>(metrics.ascenderY);
	variant.descender = static_cast<float>(metrics.descenderY);
	variant.lineHeight = static_cast<float>(metrics.lineHeight);
	variant.underlineY = static_cast<float>(metrics.underlineY);
	variant.underlineThickness = static_cast<float>(metrics.underlineThickness);
	variant.name = createInfo.name.empty() ? createInfo.path.stem().string() : createInfo.name;
	variant.metadata = "runtime-msdf";
	variant.glyphs.reserve(glyphs.size());

	for (const msdf_atlas::GlyphGeometry& glyphGeometry : fontGeometry.getGlyphs()) {
		double planeLeft = 0.0;
		double planeBottom = 0.0;
		double planeRight = 0.0;
		double planeTop = 0.0;
		glyphGeometry.getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);

		double imageLeft = 0.0;
		double imageBottom = 0.0;
		double imageRight = 0.0;
		double imageTop = 0.0;
		glyphGeometry.getQuadAtlasBounds(imageLeft, imageBottom, imageRight, imageTop);

		Font::GlyphData glyph{};
		glyph.codepoint = glyphGeometry.getCodepoint();
		glyph.sourceImageIndex = 0;
		glyph.planeLeft = static_cast<float>(planeLeft);
		glyph.planeBottom = static_cast<float>(planeBottom);
		glyph.planeRight = static_cast<float>(planeRight);
		glyph.planeTop = static_cast<float>(planeTop);
		glyph.imageLeft = static_cast<float>(imageLeft);
		glyph.imageBottom = static_cast<float>(imageBottom);
		glyph.imageRight = static_cast<float>(imageRight);
		glyph.imageTop = static_cast<float>(imageTop);
		glyph.advanceX = static_cast<float>(glyphGeometry.getAdvance());
		glyph.advanceY = 0.0f;

		const uint32_t newGlyphIndex = static_cast<uint32_t>(variant.glyphs.size());
		variant.glyphs.push_back(glyph);
		if (glyph.codepoint != 0) {
			variant.unicodeToGlyphIndex.emplace(glyph.codepoint, newGlyphIndex);
		}
	}

	for (const auto& pair : fontGeometry.getKerning()) {
		const msdf_atlas::GlyphGeometry* leftGlyph = fontGeometry.getGlyph(msdfgen::GlyphIndex(pair.first.first));
		const msdf_atlas::GlyphGeometry* rightGlyph = fontGeometry.getGlyph(msdfgen::GlyphIndex(pair.first.second));
		if (!leftGlyph || !rightGlyph || leftGlyph->getCodepoint() == 0 || rightGlyph->getCodepoint() == 0) {
			continue;
		}
		const uint64_t key = Font::FontVariantData::kerningKey(leftGlyph->getCodepoint(), rightGlyph->getCodepoint());
		variant.kerningPairs[key] = static_cast<float>(pair.second);
	}

	if (const auto questionIt = variant.unicodeToGlyphIndex.find('?'); questionIt != variant.unicodeToGlyphIndex.end()) {
		variant.fallbackGlyphIndex = questionIt->second;
	} else if (variant.fallbackGlyphIndex >= variant.glyphs.size()) {
		variant.fallbackGlyphIndex = 0;
	}

	fontFace.name = makeUniqueFontName(variant.name, controller_->fontIdByName);
	fontFace.variants.push_back(std::move(variant));

	const size_t newIndex = controller_->fonts.size();
	const FontId committedId = fontFace.id;
	const std::string committedName = fontFace.name;
	controller_->fontIndexById.reserve(controller_->fontIndexById.size() + 1u);
	controller_->fontIdByName.reserve(controller_->fontIdByName.size() + 1u);
	controller_->fontIndexById.emplace(committedId, newIndex);
	try {
		controller_->fontIdByName.emplace(committedName, committedId);
		controller_->uploadLayerTransactional(assignedLayer, pagePixels);
		controller_->fonts.push_back(std::move(fontFace));
		++controller_->nextFontId;
	} catch (...) {
		controller_->fontIdByName.erase(committedName);
		controller_->fontIndexById.erase(committedId);
		if (controller_->atlasLayerPixels.size() > assignedLayer) {
			controller_->atlasLayerPixels.resize(assignedLayer);
			controller_->borrowedAtlas.layersUsed = assignedLayer;
		}
		throw;
	}
	return controller_->fonts.back().id;
#else
	(void)createInfo;
	throw std::runtime_error("Runtime font baking is not enabled.");
#endif
}

FontManager::FontId FontManager::registerBakedFont(std::string_view arfontPath, std::string_view requestedName) {
	if (!controller_) {
		throw std::runtime_error("FontManager is not initialized.");
	}

	const std::filesystem::path path(arfontPath);
	if (!isArfontPath(path)) {
		throw std::runtime_error("Unsupported baked font file type: " + path.string());
	}
	if (!std::filesystem::is_regular_file(path)) {
		throw std::runtime_error("Font file does not exist: " + path.string());
	}

	artery_font::StdArteryFont<float> arteryFont{};
	const std::string pathString = path.string();
	if (!artery_font::readFile(arteryFont, pathString.c_str())) {
		throw std::runtime_error("Failed to read .arfont file: " + path.string());
	}
	if (arteryFont.variants.length() <= 0) {
		throw std::runtime_error(".arfont file has no font variants: " + path.string());
	}
	if (arteryFont.images.length() <= 0) {
		throw std::runtime_error(".arfont file has no atlas images: " + path.string());
	}

	const int imageIndex = pickAtlasImageIndex(arteryFont);
	if (imageIndex < 0) {
		throw std::runtime_error(".arfont file has no supported atlas image encoding.");
	}

	const auto* images = listData(arteryFont.images);
	const DecodedAtlasImage decodedImage = decodeImageToRgba8(
		images[imageIndex]
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		, devMemoryRecorder_
#endif
		);
	const uint32_t pageWidth = controller_->atlasSizeHint;
	const uint32_t pageHeight = controller_->atlasSizeHint;
	std::vector<uint8_t> pagePixels = copyAtlasIntoPage(decodedImage, pageWidth, pageHeight, path);

	if (decodedImage.width != pageWidth || decodedImage.height != pageHeight) {
		std::fprintf(
			stderr,
			"[FlowUi] Warning: .arfont atlas %ux%u was copied into configured font atlas page %ux%u.\n",
			decodedImage.width,
			decodedImage.height,
			pageWidth,
			pageHeight);
	}

	const uint32_t assignedLayer = static_cast<uint32_t>(controller_->atlasLayerPixels.size());

	const auto* variants = listData(arteryFont.variants);
	Font::FontFaceData fontFace{};
	if (controller_->nextFontId == std::numeric_limits<FontId>::max()) {
		throw std::runtime_error("FlowUi font id limit exceeded.");
	}
	fontFace.id = controller_->nextFontId;
	fontFace.sourcePath = path;
	fontFace.atlasLayer = assignedLayer;
	fontFace.atlasWidth = pageWidth;
	fontFace.atlasHeight = pageHeight;
	fontFace.sourceAtlasX = 0;
	fontFace.sourceAtlasY = 0;
	fontFace.sourceAtlasWidth = decodedImage.width;
	fontFace.sourceAtlasHeight = decodedImage.height;
	fontFace.imageType = static_cast<uint32_t>(images[imageIndex].imageType);
	fontFace.metadata = toStdString(arteryFont.metadata);
	fontFace.defaultVariantIndex = 0;
	fontFace.variants.reserve(static_cast<size_t>(arteryFont.variants.length()));

	for (int variantIndex = 0; variantIndex < arteryFont.variants.length(); ++variantIndex) {
		const auto& sourceVariant = variants[variantIndex];
		Font::FontVariantData variant{};
		variant.flags = sourceVariant.flags;
		variant.weight = sourceVariant.weight;
		variant.fallbackGlyphIndex = sourceVariant.fallbackGlyph;
		variant.fontSizePx = sourceVariant.metrics.fontSize;
		variant.distanceRange = sourceVariant.metrics.distanceRange;
		variant.emSize = sourceVariant.metrics.emSize;
		variant.ascender = sourceVariant.metrics.ascender;
		variant.descender = sourceVariant.metrics.descender;
		variant.lineHeight = sourceVariant.metrics.lineHeight;
		variant.underlineY = sourceVariant.metrics.underlineY;
		variant.underlineThickness = sourceVariant.metrics.underlineThickness;
		variant.distanceRangeMiddle = sourceVariant.metrics.distanceRangeMiddle;
		variant.name = toStdString(sourceVariant.name);
		variant.metadata = toStdString(sourceVariant.metadata);

		const auto* sourceGlyphs = listData(sourceVariant.glyphs);
		variant.glyphs.reserve(static_cast<size_t>(sourceVariant.glyphs.length()));
		for (int glyphIndex = 0; glyphIndex < sourceVariant.glyphs.length(); ++glyphIndex) {
			const auto& sourceGlyph = sourceGlyphs[glyphIndex];
			if (sourceGlyph.image != static_cast<uint32_t>(imageIndex)) {
				throw std::runtime_error(
					".arfont references multiple atlas images per variant, which is not supported yet.");
			}

			Font::GlyphData glyph{};
			glyph.codepoint = sourceGlyph.codepoint;
			glyph.sourceImageIndex = sourceGlyph.image;
			glyph.planeLeft = sourceGlyph.planeBounds.l;
			glyph.planeBottom = sourceGlyph.planeBounds.b;
			glyph.planeRight = sourceGlyph.planeBounds.r;
			glyph.planeTop = sourceGlyph.planeBounds.t;
			glyph.imageLeft = sourceGlyph.imageBounds.l;
			glyph.imageBottom = sourceGlyph.imageBounds.b;
			glyph.imageRight = sourceGlyph.imageBounds.r;
			glyph.imageTop = sourceGlyph.imageBounds.t;
			glyph.advanceX = sourceGlyph.advance.h;
			glyph.advanceY = sourceGlyph.advance.v;

			const uint32_t newGlyphIndex = static_cast<uint32_t>(variant.glyphs.size());
			variant.glyphs.push_back(glyph);

			if (sourceVariant.codepointType == artery_font::CP_UNICODE && glyph.codepoint != 0) {
				variant.unicodeToGlyphIndex.emplace(glyph.codepoint, newGlyphIndex);
			}
		}

		const auto* sourceKernPairs = listData(sourceVariant.kernPairs);
		for (int kernIndex = 0; kernIndex < sourceVariant.kernPairs.length(); ++kernIndex) {
			const auto& pair = sourceKernPairs[kernIndex];
			const uint64_t key = Font::FontVariantData::kerningKey(pair.codepoint1, pair.codepoint2);
			variant.kerningPairs[key] = pair.advance.h;
		}

		if (variant.fallbackGlyphIndex >= variant.glyphs.size()) {
			variant.fallbackGlyphIndex = 0;
		}

		fontFace.variants.push_back(std::move(variant));
	}

	std::string preferredName;
	if (!requestedName.empty()) {
		preferredName = std::string(requestedName);
	} else if (!fontFace.variants.empty() && !fontFace.variants[0].name.empty()) {
		preferredName = fontFace.variants[0].name;
	} else {
		preferredName = path.stem().string();
	}
	fontFace.name = makeUniqueFontName(preferredName, controller_->fontIdByName);

	const size_t newIndex = controller_->fonts.size();
	const FontId committedId = fontFace.id;
	const std::string committedName = fontFace.name;
	controller_->fontIndexById.reserve(controller_->fontIndexById.size() + 1u);
	controller_->fontIdByName.reserve(controller_->fontIdByName.size() + 1u);
	controller_->fontIndexById.emplace(committedId, newIndex);
	try {
		controller_->fontIdByName.emplace(committedName, committedId);
		controller_->uploadLayerTransactional(assignedLayer, pagePixels);
		controller_->fonts.push_back(std::move(fontFace));
		++controller_->nextFontId;
	} catch (...) {
		controller_->fontIdByName.erase(committedName);
		controller_->fontIndexById.erase(committedId);
		if (controller_->atlasLayerPixels.size() > assignedLayer) {
			controller_->atlasLayerPixels.resize(assignedLayer);
			controller_->borrowedAtlas.layersUsed = assignedLayer;
		}
		throw;
	}
	return controller_->fonts.back().id;
}

const Font::FontFaceData* FontManager::getFontById(FontId fontId) const {
	const auto it = controller_->fontIndexById.find(fontId);
	if (it == controller_->fontIndexById.end()) {
		return nullptr;
	}
	const size_t index = it->second;
	return (index < controller_->fonts.size()) ? &controller_->fonts[index] : nullptr;
}

const Font::AtlasArrayResource& FontManager::getAtlasResource() const {
	if (!controller_) throw std::logic_error("FontManager is not initialized.");
	return controller_->borrowedAtlas;
}

manager_storage::FontFrameView FontManager::frameView(const storage::FrameToken& frame) const {
	if (!controller_ || !storage_) throw std::logic_error("FontManager is not initialized.");
	std::array<storage::ResourceUse, 3> uses{};
	size_t count = 0;
	if (controller_->atlasImage) uses[count++] = storage::useOf(controller_->atlasImage);
	if (controller_->atlasView) uses[count++] = storage::useOf(controller_->atlasView);
	if (controller_->atlasSampler) uses[count++] = storage::useOf(controller_->atlasSampler);
	if (count > 0) storage_->trackUses(frame, std::span(uses).first(count));
	return manager_storage::FontFrameView{
		.families = &controller_->families,
		.fonts = &controller_->fonts,
		.familyCount = controller_->families.size(),
		.fontCount = controller_->fonts.size(),
		.atlas = controller_->borrowedAtlas,
		.atlasImage = controller_->atlasImage,
		.atlasView = controller_->atlasView,
		.atlasSampler = controller_->atlasSampler,
		.publicationRevision = controller_->catalogRevision,
	};
}

void FontManager::destroy() noexcept {
	if (storage_) {
		try {
			const storage::StringId name = storage_->intern("flowui.font.catalog");
			(void)storage_->removeManagerRecord(
				storage::ResourceKey{storage::ResourceDomain::Font, name, InvalidWindowId},
				storage::ResourceKind::FontFamily);
		} catch (...) {
		}
	}
	controller_ = nullptr;
	controllerHandle_ = 0;
	storage_ = nullptr;
}

} // namespace FlowUi
