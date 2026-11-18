#pragma once
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontMSDFProvider.hpp"
#include "FlowUi/Public/BuildConfig.hpp"
namespace FlowUi::detail::font {
/** Default CPU suite for Artery serialization and optional FreeType/MTSDF generation. */
class MsdfAtlasFontProvider {
public:
	static constexpr uint32_t contractRevision = fontMsdfContractRevision;
	static constexpr bool supportsBaking = FLOWUI_CONFIG_DEFAULT_FONT_BAKING != 0;
	static constexpr bool supportsEncoding = true;
	/** Decode owned CPU metrics and top-down MSDF pixels. */
	[[nodiscard]] Result<FontCpuAsset> loadBaked(std::span<const std::byte> bakedBytes) noexcept;
	/** Generate a fixed-page or automatically packed CPU MTSDF atlas. */
	[[nodiscard]] Result<FontBakeOutput> bake(const FontBakeRequest& request) noexcept;
	/** Encode normalized CPU data without assigning catalog identity. */
	[[nodiscard]] Result<std::vector<std::byte>> encodeBaked(const FontCpuAsset& asset) noexcept;
};
static_assert(FontMSDFProvider<MsdfAtlasFontProvider>);
} // namespace FlowUi::detail::font
