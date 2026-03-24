#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "clay.h"

struct FontManager;

namespace FlowUi {

inline Clay_Color Flow_Color(std::string_view hexRgba)
{
	if (hexRgba.size() != 9 || hexRgba[0] != '#') {
		throw std::invalid_argument("Flow_Color expects #RRGGBBAA.");
	}

	const auto decodeHexNibble = [](char c) -> uint8_t {
		if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
		if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
		if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
		throw std::invalid_argument("Flow_Color received invalid hex digit.");
	};

	const auto decodeHexByte = [&](std::size_t index) -> float {
		const uint8_t hi = decodeHexNibble(hexRgba[index]);
		const uint8_t lo = decodeHexNibble(hexRgba[index + 1]);
		return static_cast<float>((hi << 4) | lo);
	};

	return Clay_Color{
		decodeHexByte(1),
		decodeHexByte(3),
		decodeHexByte(5),
		decodeHexByte(7),
	};
}

class UiManager;
class ImageManager;
#if FLOWUI_INCLUDE_SVG_MANAGER
class IconManager;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
class ViewPortManager;
#endif
class ElementRegistry;
struct ElementDefinition;

class App {
public:
	App();
	App(App&&) noexcept;
	App& operator=(App&&) noexcept;
	App(const App&) = delete;
	App& operator=(const App&) = delete;
	~App();

	bool shouldClose() const;

	// Frame lifecycle
	void beginFrame();
	void endFrame();
	void drawFrame();

	FontManager& fonts();
	const FontManager& fonts() const;
	ImageManager& images();
	const ImageManager& images() const;
#if FLOWUI_INCLUDE_SVG_MANAGER
	IconManager& icons();
	const IconManager& icons() const;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	ViewPortManager& viewPorts();
	const ViewPortManager& viewPorts() const;
#endif

	UiManager& ui();
	const UiManager& ui() const;
	ElementRegistry& elementRegistry();
	const ElementRegistry& elementRegistry() const;
	void registerElement(ElementDefinition definition);

	void setWindowTitle(std::string_view title);
	std::pair<int,int> windowSize() const;
	std::pair<int,int> framebufferSize() const;
	void setWindowInputConfig(const WindowInputConfig& config);
	WindowInputConfig windowInputConfig() const;
	bool supportsRawMouseMotion() const;
	void setClipboardText(std::string_view text);
	std::string clipboardText() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;

	friend App makeApplication(const AppConfig& cfg);
};

App makeApplication(const AppConfig& cfg);

} // namespace FlowUi
