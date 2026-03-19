#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <cstdint>
#include <utility>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"

struct FontManager;

namespace FlowUi {

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

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;

	friend App makeApplication(const AppConfig& cfg);
};

App makeApplication(const AppConfig& cfg);

} // namespace FlowUi
