#pragma once

#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <span>
#include <unordered_map>
#include <vector>

#include "FlowUi/FontResources.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"

namespace FlowUi::detail::manager_storage {

struct FontFamilyFaceRecord {
	FontId fontId = 0;
	uint32_t weight = 400;
	FontStyle style = FontStyle::Normal;
};

struct FontFamilyRecord {
	FontFamilyId id = 0;
	std::string name{};
	std::vector<FontFamilyFaceRecord> faces{};
};

struct FontFrameView {
	const std::deque<FontFamilyRecord>* families = nullptr;
	const std::deque<Font::FontFaceData>* fonts = nullptr;
	size_t familyCount = 0;
	size_t fontCount = 0;
	Font::AtlasArrayResource atlas{};
	storage::ImageHandle atlasImage{};
	storage::ImageViewHandle atlasView{};
	storage::SamplerHandle atlasSampler{};
	uint64_t publicationRevision = 0;

	[[nodiscard]] const Font::FontFaceData* font(FontId id) const noexcept {
		if (!fonts) return nullptr;
		if (id < fontCount && (*fonts)[id].id == id) return &(*fonts)[id];
		for (size_t index = 0; index < fontCount; ++index) {
			if ((*fonts)[index].id == id) return &(*fonts)[index];
		}
		return nullptr;
	}
	[[nodiscard]] FontId resolve(FontFamilyId familyId, uint32_t weight, FontStyle style) const noexcept {
		if (!families || familyId >= familyCount) return 0;
		const FontFamilyRecord& family = (*families)[familyId];
		const FontFamilyFaceRecord* best = nullptr;
		uint32_t distance = std::numeric_limits<uint32_t>::max();
		for (const FontFamilyFaceRecord& face : family.faces) {
			if (face.style != style) continue;
			const uint32_t candidate = face.weight > weight ? face.weight - weight : weight - face.weight;
			if (!best || candidate < distance) { best = &face; distance = candidate; }
		}
		if (best) return best->fontId;
		return family.faces.empty() ? 0 : family.faces.front().fontId;
	}
	[[nodiscard]] FontId resolve(std::string_view name, uint32_t weight, FontStyle style) const noexcept {
		if (!families) return 0;
		for (size_t index = 0; index < familyCount; ++index) {
			const FontFamilyRecord& family = (*families)[index];
			if (family.name == name) return resolve(family.id, weight, style);
		}
		return 0;
	}
};

class FontCatalogController {
public:
	FontCatalogController(storage::IStorageSystem& storageSystem, uint32_t atlasSize);
	~FontCatalogController() noexcept;

	FontCatalogController(const FontCatalogController&) = delete;
	FontCatalogController& operator=(const FontCatalogController&) = delete;

	void uploadLayerTransactional(uint32_t layer, const std::vector<uint8_t>& rgbaPixels);
	void refreshBorrowedAtlas();

	storage::IStorageSystem* storage = nullptr;
	uint32_t atlasSizeHint = 0;
	FontId nextFontId = 0;
	bool familyTransaction = false;
	std::deque<FontFamilyRecord> families{};
	std::unordered_map<std::string, FontFamilyId> familyIdByName{};
	std::deque<Font::FontFaceData> fonts{};
	std::unordered_map<FontId, size_t> fontIndexById{};
	std::unordered_map<std::string, FontId> fontIdByName{};
	std::vector<std::vector<uint8_t>> atlasLayerPixels{};
	storage::ImageHandle atlasImage{};
	storage::ImageViewHandle atlasView{};
	storage::SamplerHandle atlasSampler{};
	Font::AtlasArrayResource borrowedAtlas{};
};

} // namespace FlowUi::detail::manager_storage
