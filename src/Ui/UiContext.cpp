#include "Ui/UiContext.hpp"
#include <algorithm>

namespace FlowUi
{
	
	UiContext::UiContext(ElementRegistry& elementRegistry, const FlowUi::AppConfig& appConfig)
		: elementRegistry_(elementRegistry)
	{
		initStringArenas(appConfig);

		const size_t minimumClayArenaCapacityBytes = static_cast<size_t>(Clay_MinMemorySize());
		const size_t configuredClayArenaCapacityBytes = appConfig.ui.clayArenaCapacityBytes;
		const size_t clayArenaCapacityBytes = (configuredClayArenaCapacityBytes == 0)
			? minimumClayArenaCapacityBytes
			: std::max(configuredClayArenaCapacityBytes, minimumClayArenaCapacityBytes);

		clayArenaMemory_ = std::make_unique<char[]>(clayArenaCapacityBytes);
		clayArena_ = Clay_CreateArenaWithCapacityAndMemory(clayArenaCapacityBytes, clayArenaMemory_.get());

		const float initialScreenWidth = static_cast<float>(std::max(1, appConfig.window.width));
		const float initialScreenHeight = static_cast<float>(std::max(1, appConfig.window.height));
		const Clay_Dimensions initialLayoutDimensions{initialScreenWidth, initialScreenHeight};

		clayContext_ = Clay_Initialize(clayArena_, initialLayoutDimensions, Clay_ErrorHandler{});
		if (!clayContext_)
		{
			throw std::runtime_error("FlowUi: Clay_Initialize failed. Increase ui.clayArenaCapacityBytes.");
		}
	}

	void UiContext::initStringArenas(const FlowUi::AppConfig& cfg)
	{
		arenasCount_ = (cfg.vk.framesInFlight == 0) ? 1 : cfg.vk.framesInFlight;
		arenas_.resize(arenasCount_);
		
		for (auto& arena : arenas_)
		{
			arena.capacity = cfg.ui.stringArenaSize;
			arena.mem = std::unique_ptr<char[]>(new char[arena.capacity]);
			arena.offset = 0;
		}
		curArena_ = 0;
	}
	
	void UiContext::beginFrame(uint32_t frameIndex, const FrameInput& frameInput, float screenWidth, float screenHeight)
	{
		if (arenas_.empty()) {
			throw std::runtime_error("FlowUi: UiContext string arenas are not initialized.");
		}
		if (!clayContext_) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		curArena_ = (arenasCount_ == 0) ? 0 : (frameIndex % arenasCount_);
		arenas_[curArena_].offset = 0;

		advanceFrameInteractionSnapshots();
		frameInputForCurrentLayout_ = frameInput;

		Clay_SetCurrentContext(clayContext_);

		const float clampedScreenWidth = std::max(1.0f, screenWidth);
		const float clampedScreenHeight = std::max(1.0f, screenHeight);
		Clay_SetLayoutDimensions(Clay_Dimensions{clampedScreenWidth, clampedScreenHeight});
		Clay_SetPointerState(
			Clay_Vector2{frameInput.mouseX, frameInput.mouseY},
			frameInput.mouseDown[0]);
		Clay_UpdateScrollContainers(
			true,
			Clay_Vector2{frameInput.scrollX, frameInput.scrollY},
			static_cast<float>(frameInput.dt));
		Clay_BeginLayout();
	}

	Clay_RenderCommandArray UiContext::endFrame()
	{
		if (!clayContext_) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		Clay_SetCurrentContext(clayContext_);
		Clay_RenderCommandArray renderCommands = Clay_EndLayout();

		InteractionSnapshot interactionSnapshot;
		Clay_ElementIdArray hoveredIds = Clay_GetPointerOverIds();
		interactionSnapshot.hoveredElementIds.reserve(static_cast<size_t>(hoveredIds.length));
		for (int32_t i = 0; i < hoveredIds.length; ++i) {
			interactionSnapshot.hoveredElementIds.push_back(hoveredIds.internalArray[i]);
		}

		const bool isPrimaryPointerDown = frameInputForCurrentLayout_.mouseDown[0];
		if (isPrimaryPointerDown && !wasPrimaryPointerDownLastFrame_) {
			interactionSnapshot.pressedElementIds = interactionSnapshot.hoveredElementIds;
		} else if (isPrimaryPointerDown && wasPrimaryPointerDownLastFrame_) {
			interactionSnapshot.heldElementIds = interactionSnapshot.hoveredElementIds;
		} else if (!isPrimaryPointerDown && wasPrimaryPointerDownLastFrame_) {
			interactionSnapshot.releasedElementIds = interactionSnapshot.hoveredElementIds;
		}
		wasPrimaryPointerDownLastFrame_ = isPrimaryPointerDown;

		setCurrentInteractionSnapshot(std::move(interactionSnapshot));
		return renderCommands;
	}
	
	char* UiContext::allocBytes(size_t nBytes, size_t align)
	{
		Arena& arena = arenas_[curArena_];
		
		size_t off = arena.offset;
		size_t aligned = (off + (align - 1)) & ~(align - 1);
		
		if (aligned + nBytes > arena.capacity)
		throw std::runtime_error("FlowUi string arena overflow: increase bytesPerArena");
		
		char* ptr = arena.mem.get() + aligned;
		arena.offset = aligned + nBytes;
		return ptr;
	}
	
	Clay_String UiContext::str(std::string_view s)
	{
		const size_t len = s.size();
		char* dst = allocBytes(len + 1, alignof(char));
		std::memcpy(dst, s.data(), len);
		dst[len] = '\0';
		
		Clay_String out;
		out.isStaticallyAllocated = false;
		out.length = (int)len;
		out.chars = dst;
		return out;
	}
	
	Clay_ElementId UiContext::sid(std::string_view s) {
		return CLAY_SID(str(s));
	}
	
	Clay_ElementId UiContext::eid(std::string_view s) {
		return Clay_GetElementId(str(s));
	}

	ElementBuilder UiContext::createElement(std::string_view elementTypeName, std::string_view instanceIdPath) {
    	const ElementDefinition* definition = elementRegistry_.findElement(elementTypeName);
    	if (!definition) {
    	    throw std::runtime_error("FlowUi: createElement called with unregistered element type.");
    	}
	
    	return ElementBuilder(*this, definition, std::string(instanceIdPath));
	}

	void UiContext::setCurrentInteractionSnapshot(InteractionSnapshot snapshot) {
	    currentInteractionSnapshot_ = std::move(snapshot);
	}

	void UiContext::advanceFrameInteractionSnapshots() {
	    previousInteractionSnapshot_ = std::move(currentInteractionSnapshot_);
	    currentInteractionSnapshot_ = InteractionSnapshot{};
	}


} // namespace FlowUi
