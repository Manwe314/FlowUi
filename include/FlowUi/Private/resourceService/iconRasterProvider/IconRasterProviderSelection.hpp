#pragma once

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/IconRasterProvider.hpp"

#if FLOWUI_CONFIG_CUSTOM_ICON_RASTER_PROVIDER
#include FLOWUI_CONFIG_ICON_RASTER_PROVIDER_HEADER
#else
#include "FlowUi/Private/resourceService/iconRasterProvider/providers/plutoSvg/PlutoSvgIconRasterProvider.hpp"
#endif

namespace FlowUi::detail::icon {

#if FLOWUI_CONFIG_CUSTOM_ICON_RASTER_PROVIDER
using ActiveIconRasterProvider = FLOWUI_CONFIG_ICON_RASTER_PROVIDER_TYPE;
#else
using ActiveIconRasterProvider = PlutoSvgIconRasterProvider;
#endif

static_assert(IconRasterProvider<ActiveIconRasterProvider>,
	"Selected icon provider must satisfy IconRasterProvider revision 1.");

} // namespace FlowUi::detail::icon
