#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <cstdint>
#include <utility>

#include "flowui/PublicStructs.hpp"
#include "Ui/FlowUiElementSystem.hpp"
#include "Ui/UiContext.hpp"

namespace FlowUi {

class App {
public:
	App();
	App(App&&) noexcept;
	App& operator=(App&&) noexcept;
	App(const App&) = delete;
	App& operator=(const App&) = delete;
	~App();

	// Main loop helpers
	void pollEvents();
	bool shouldClose() const;

	// Frame lifecycle
	void beginFrame();   // consumes input gathered since last pollEvents
	void endFrame();     // finalize draw list
	void drawFrame();    // record + submit + present

	// Resource loading
	// Return small handles/IDs (int or strong typedef)
	int loadFont(std::string_view fontPath, float pxSize);
	int loadSvgIcon(std::string_view svgPath, int pxSize);

	// Access to UI builder (your Clay C++ wrapper)
	// either return a reference, or App forwards widget calls
	UiContext& ui();
	ElementRegistry& elementRegistry();
	const ElementRegistry& elementRegistry() const;
	void registerElement(ElementDefinition definition);

	// Optional utilities (v1 nice-to-have)
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
