#pragma once


#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <string>
#include <cstring>
#include <stdexcept>
#include <clay.h>
#include <vector>
#include <iostream>

#include "flowui/PublicStructs.hpp"
#include "Ui/FlowUiElementSystem.hpp"
#include "window/Inputs.hpp"

namespace FlowUi {

class UiContext {
public:
	UiContext(ElementRegistry& elementRegistry, const AppConfig& appConfig);
	void initStringArenas(const AppConfig& cfg);
	void beginFrame(uint32_t frameIndex, const FrameInput& frameInput, float screenWidth, float screenHeight);
	Clay_RenderCommandArray endFrame();

	Clay_String str(std::string_view s);
	Clay_ElementId sid(std::string_view s);
	Clay_ElementId eid(std::string_view s);
	
	ElementBuilder createElement(std::string_view elementTypeName, std::string_view instanceIdPath);

    const InteractionSnapshot& previousFrameInteraction() const { return previousInteractionSnapshot_; }

    void setCurrentInteractionSnapshot(InteractionSnapshot snapshot);
    void advanceFrameInteractionSnapshots();
private:
	struct Arena {
		std::unique_ptr<char[]> mem;
		size_t capacity = 0;
		size_t offset = 0;
	};
	
	char* allocBytes(size_t nBytes, size_t align = alignof(std::max_align_t));

private:
	
	std::vector<Arena> arenas_;
	uint32_t arenasCount_ = 0;
	uint32_t curArena_ = 0;

	std::unique_ptr<char[]> clayArenaMemory_;
	Clay_Arena clayArena_{};
	Clay_Context* clayContext_ = nullptr;
	FrameInput frameInputForCurrentLayout_{};
	bool wasPrimaryPointerDownLastFrame_ = false;

	ElementRegistry& elementRegistry_;
	InteractionSnapshot previousInteractionSnapshot_;
    InteractionSnapshot currentInteractionSnapshot_;

};

} //namespace FlowUi
