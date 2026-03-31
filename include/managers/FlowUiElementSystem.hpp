#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "clay.h"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/App.hpp"

namespace FlowUi {


struct InteractionSnapshot {
	std::vector<Clay_ElementId> hoveredElementIds;
    std::vector<Clay_ElementId> pressedElementIds;
    std::vector<Clay_ElementId> heldElementIds;
    std::vector<Clay_ElementId> releasedElementIds;

    static bool contains(const std::vector<Clay_ElementId>& list, Clay_ElementId id)
	{
        for (const auto& item : list)
            if (item.id == id.id)
				return true;
        return false;
    }

    bool isHovered(Clay_ElementId id)  const { return contains(hoveredElementIds, id);  }
    bool isPressed(Clay_ElementId id)  const { return contains(pressedElementIds, id);  }
    bool isHeld(Clay_ElementId id)     const { return contains(heldElementIds, id);     }
    bool isReleased(Clay_ElementId id) const { return contains(releasedElementIds, id); }
};

struct NoElementParameters {};
struct NoElementState {};
struct NoElementResources {};

class UiManager;
Clay_ElementId flowUiToClayElementId(UiManager& uiManager, std::string_view elementID);
const InteractionSnapshot& flowUiPreviousInteraction(const UiManager& uiManager);
void flowUiPushConstructedElement(UiManager& uiManager, Clay_ElementId elementId);

template <typename Parameters = NoElementParameters>
struct ElementBuildContext;

template <typename Parameters = NoElementParameters>
struct ElementInteractionContext;

template <typename Parameters>
struct ElementBuildContext
{
    using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

    UiManager& uiManager;
    std::string_view elementID;
    ParametersType& params;

    std::string createChildElementId(std::string_view localChildId) const
    {
        std::string full = std::string(elementID) + "/" + std::string(localChildId);
        return full;
    }
};

template <typename Parameters>
struct ElementInteractionContext
{
    using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

    UiManager& uiManager;
    std::string_view elementID;
    ParametersType& params;
    const InteractionSnapshot& previousInteraction;

    std::string createChildElementId(std::string_view localChildId) const
    {
        std::string full = std::string(elementID) + "/" + std::string(localChildId);
        return full;
    }
};


template <typename Parameters = NoElementParameters, typename State = void, typename Resources = void, uint64_t DefinitionId = 0>
struct ElementDefinition
{
    using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;
    using StateType = std::conditional_t<std::is_void_v<State>, NoElementState, State>;
    using ResourcesType = std::conditional_t<std::is_void_v<Resources>, NoElementResources, Resources>;
    using BuildContext = ElementBuildContext<Parameters>;
    using InteractionContext = ElementInteractionContext<Parameters>;
    using StatePoolEntry = std::pair<uint64_t, StateType>;

    static constexpr uint64_t definitionId = DefinitionId;
    static constexpr bool hasState = !std::is_void_v<State>;
    static constexpr bool hasResources = !std::is_void_v<Resources>;
    static inline std::optional<ResourcesType> resources{};
    static inline std::vector<StatePoolEntry> statePool{};

    static ResourcesType& getResources(App& app)
    {
        static_assert(hasResources, "FlowUi: getResources is only available when ElementDefinition Resources template argument is not void.");
        if (!resources.has_value()) {
            if constexpr (std::is_constructible_v<ResourcesType, App&>) {
                resources.emplace(app);
            } else if constexpr (std::is_constructible_v<ResourcesType, UiManager&>) {
                resources.emplace(app.ui());
            } else {
                resources.emplace();
            }
        }
        return *resources;
    }

    static StateType& getOrCreateState(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: getOrCreateState is only available when ElementDefinition State template argument is not void.");
        for (StatePoolEntry& entry : statePool) {
            if (entry.first == elementFlowId) {
                return entry.second;
            }
        }
        statePool.emplace_back(elementFlowId, StateType{});
        return statePool.back().second;
    }

    static StateType* tryGetState(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: tryGetState is only available when ElementDefinition State template argument is not void.");
        for (StatePoolEntry& entry : statePool) {
            if (entry.first == elementFlowId) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    static const StateType* tryGetStateConst(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: tryGetStateConst is only available when ElementDefinition State template argument is not void.");
        for (const StatePoolEntry& entry : statePool) {
            if (entry.first == elementFlowId) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    static bool eraseState(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: eraseState is only available when ElementDefinition State template argument is not void.");
        for (std::size_t i = 0; i < statePool.size(); ++i) {
            if (statePool[i].first == elementFlowId) {
                statePool[i] = std::move(statePool.back());
                statePool.pop_back();
                return true;
            }
        }
        return false;
    }

    void (*onHovered)(InteractionContext&) = nullptr;
    void (*onPressed)(InteractionContext&) = nullptr;
    void (*onHeld)(InteractionContext&) = nullptr;
    void (*onReleased)(InteractionContext&) = nullptr;

    void (*runLogic)(InteractionContext&) = nullptr;

    Clay_ElementDeclaration (*constructElment)(BuildContext&) = nullptr;
    void (*buildElement)(BuildContext&) = nullptr;
};


enum class ElementDrawOptions : uint32_t
{
    Default = 0,
    SkipEventCallbacks = 1u << 0,
    SkipLogicCallback  = 1u << 1,
    SkipBuildCallback  = 1u << 2,
};

inline ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)
{
    return static_cast<ElementDrawOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)
{
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}


template <typename Parameters = NoElementParameters, typename State = void, typename Resources = void, uint64_t DefinitionId = 0>
class ElementBuilder {
public:
    using DefinitionType = ElementDefinition<Parameters, State, Resources, DefinitionId>;
    using ParametersType = typename DefinitionType::ParametersType;
    using BuildContext = typename DefinitionType::BuildContext;
    using InteractionContext = typename DefinitionType::InteractionContext;

    ElementBuilder(UiManager& uiManager, const DefinitionType* definition, std::string elementID) :
        uiManager_(uiManager),
        elementDefinition_(definition),
        elementID_(std::move(elementID)) {}

    ElementBuilder& setParameters(const ParametersType& parameters)
    {
        params_ = parameters;
        return *this;
    }

    ElementBuilder& setParameters(ParametersType&& parameters)
    {
        params_ = std::move(parameters);
        return *this;
    }

	ElementBuilder& withElementID(std::string_view elementID)
    {
        elementID_.assign(elementID.data(), elementID.size());
        return *this;
    }

    void construct(ElementDrawOptions options = ElementDrawOptions::Default);
    void draw(ElementDrawOptions options = ElementDrawOptions::Default);

private:
    UiManager& uiManager_;
    const DefinitionType* elementDefinition_;
    std::string elementID_;
    ParametersType params_{};
};

template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId>
void ElementBuilder<Parameters, State, Resources, DefinitionId>::construct(ElementDrawOptions options)
{
    if (!elementDefinition_ || !elementDefinition_->constructElment) {
        throw std::runtime_error("FlowUi: elementDefinition is null or missing constructElment callback.");
    }

    const Clay_ElementId rootElementId = flowUiToClayElementId(uiManager_, elementID_);

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);

        InteractionContext eventContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };

        if (elementDefinition_->onHovered && previousInteraction.isHovered(rootElementId)) {
            elementDefinition_->onHovered(eventContext);
        }
        if (elementDefinition_->onPressed && previousInteraction.isPressed(rootElementId)) {
            elementDefinition_->onPressed(eventContext);
        }
        if (elementDefinition_->onHeld && previousInteraction.isHeld(rootElementId)) {
            elementDefinition_->onHeld(eventContext);
        }
        if (elementDefinition_->onReleased && previousInteraction.isReleased(rootElementId)) {
            elementDefinition_->onReleased(eventContext);
        }
    }

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipLogicCallback) && elementDefinition_->runLogic) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);
        InteractionContext logicContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };
        elementDefinition_->runLogic(logicContext);
    }

    BuildContext buildContext{
        uiManager_,
        elementID_,
        params_
    };

    Clay_ElementDeclaration declaration = elementDefinition_->constructElment(buildContext);
    declaration.id = rootElementId;
    Clay__OpenElement();
    Clay__ConfigureOpenElement(declaration);
    flowUiPushConstructedElement(uiManager_, rootElementId);
}

template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId>
void ElementBuilder<Parameters, State, Resources, DefinitionId>::draw(ElementDrawOptions options)
{
    if (!elementDefinition_ || !elementDefinition_->buildElement) {
        throw std::runtime_error("FlowUi: elementDefinition is null or missing buildElement callback.");
    }

    const Clay_ElementId rootElementId = flowUiToClayElementId(uiManager_, elementID_);

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);

        InteractionContext eventContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };

        if (elementDefinition_->onHovered && previousInteraction.isHovered(rootElementId)) {
            elementDefinition_->onHovered(eventContext);
        }
        if (elementDefinition_->onPressed && previousInteraction.isPressed(rootElementId)) {
            elementDefinition_->onPressed(eventContext);
        }
        if (elementDefinition_->onHeld && previousInteraction.isHeld(rootElementId)) {
            elementDefinition_->onHeld(eventContext);
        }
        if (elementDefinition_->onReleased && previousInteraction.isReleased(rootElementId)) {
            elementDefinition_->onReleased(eventContext);
        }
    }

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipLogicCallback) && elementDefinition_->runLogic) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);
        InteractionContext logicContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };
        elementDefinition_->runLogic(logicContext);
    }

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipBuildCallback)) {
        BuildContext buildContext{
            uiManager_,
            elementID_,
            params_
        };
        elementDefinition_->buildElement(buildContext);
    }
}

} // namespace FlowUi
