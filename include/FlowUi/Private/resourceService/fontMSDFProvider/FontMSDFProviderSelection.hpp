#pragma once
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontMSDFProvider.hpp"
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOWUI_CONFIG_CUSTOM_FONT_MSDF_PROVIDER
#include FLOWUI_CONFIG_FONT_MSDF_PROVIDER_HEADER
#else
#include "FlowUi/Private/resourceService/fontMSDFProvider/providers/msdfAtlas/MsdfAtlasFontProvider.hpp"
#endif
namespace FlowUi::detail::font {
#if FLOWUI_CONFIG_CUSTOM_FONT_MSDF_PROVIDER
using ActiveFontMSDFProvider = FLOWUI_CONFIG_FONT_MSDF_PROVIDER_TYPE;
#else
using ActiveFontMSDFProvider = MsdfAtlasFontProvider;
#endif
static_assert(FontMSDFProvider<ActiveFontMSDFProvider>,
			  "Selected font provider must satisfy FontMSDFProvider revision 1.");
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
static_assert(ActiveFontMSDFProvider::supportsBaking,
			  "Runtime font baking requires a baking-capable FontMSDFProvider.");
#endif
} // namespace FlowUi::detail::font
