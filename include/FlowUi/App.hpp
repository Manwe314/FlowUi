#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "clay.h"

struct FontManager;

namespace FlowUi {

using FlowElementId = uint64_t;
using FlowDefinitionId = uint64_t;

namespace detail {

constexpr uint64_t kFlowFnvOffsetBasis = 14695981039346656037ull;
constexpr uint64_t kFlowFnvPrime = 1099511628211ull;

constexpr uint64_t flowHashBytes(std::string_view text) noexcept {
	uint64_t hash = kFlowFnvOffsetBasis;
	for (const char c : text) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
		hash *= kFlowFnvPrime;
	}
	return (hash == 0ull) ? 1ull : hash;
}

constexpr uint64_t flowMix64(uint64_t value) noexcept {
	value ^= value >> 30;
	value *= 0xbf58476d1ce4e5b9ull;
	value ^= value >> 27;
	value *= 0x94d049bb133111ebull;
	value ^= value >> 31;
	return (value == 0ull) ? 1ull : value;
}

} // namespace detail

constexpr FlowElementId toFlowId(std::string_view elementName) noexcept {
	return detail::flowHashBytes(elementName);
}

template <std::size_t N>
constexpr FlowElementId toFlowId(const char (&elementName)[N]) noexcept {
	return detail::flowHashBytes(std::string_view{elementName, N - 1});
}

constexpr FlowDefinitionId toFlowDefinitionId(std::string_view definitionName) noexcept {
	return detail::flowHashBytes(definitionName);
}

template <std::size_t N>
constexpr FlowDefinitionId toFlowDefinitionId(const char (&definitionName)[N]) noexcept {
	return detail::flowHashBytes(std::string_view{definitionName, N - 1});
}

constexpr FlowElementId createIndexedFlowId(FlowElementId rootId, uint64_t index) noexcept {
	const uint64_t mixedIndex = detail::flowMix64(index + 0x9e3779b97f4a7c15ull);
	return detail::flowMix64(rootId ^ mixedIndex);
}

constexpr FlowElementId createIndexedFlowId(std::string_view rootName, uint64_t index) noexcept {
	return createIndexedFlowId(toFlowId(rootName), index);
}

template <std::size_t N>
constexpr FlowElementId createIndexedFlowId(const char (&rootName)[N], uint64_t index) noexcept {
	return createIndexedFlowId(toFlowId(rootName), index);
}

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

#define FLOW_ID(label) (::FlowUi::toFlowId(label))
#define FLOW_DEF_ID(label) (::FlowUi::toFlowDefinitionId(label))
