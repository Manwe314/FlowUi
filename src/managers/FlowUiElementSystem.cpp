#include "managers/FlowUiElementSystem.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi {

Clay_ElementId ElementBuildContext::createChildElementId(std::string_view localChildId) const
{
    std::string full = std::string(instanceIdPath) + "/" + std::string(localChildId);
    return userInterface.toClayEID(full);
}

Clay_ElementId ElementInteractionContext::createChildElementId(std::string_view localChildId) const
{
    std::string full = std::string(instanceIdPath) + "/" + std::string(localChildId);
    return userInterface.toClayEID(full);
}

void ElementRegistry::registerElement(ElementDefinition definition)
{
    elementDefinitions_[definition.elementTypeName] = std::move(definition);
}

const ElementDefinition* ElementRegistry::findElement(std::string_view elementTypeName) const
{
    auto it = elementDefinitions_.find(std::string(elementTypeName));
    if (it == elementDefinitions_.end())
		return nullptr;
    return &it->second;
}


ElementBuilder::ElementBuilder(UiManager& userInterface, const ElementDefinition* definition, std::string instanceIdPath) :
	userInterface_(userInterface),
    elementDefinition_(definition),
    instanceIdPath_(std::move(instanceIdPath)) {}

ElementBuilder& ElementBuilder::set(std::string_view key, bool value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, int value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, float value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, std::string_view value) {
    userOverrides_.setValue(key, std::string(value));
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, const char* value) {
    userOverrides_.setValue(key, std::string(value ? value : ""));
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_ElementId value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_Color value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_Dimensions value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_Vector2 value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_LayoutAlignmentX value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_LayoutAlignmentY value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_SizingMinMax value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_Sizing value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_SizingAxis value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay__SizingType value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_Padding value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_LayoutDirection value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_LayoutConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_CornerRadius value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_ChildAlignment value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_TextElementConfigWrapMode value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_TextAlignment value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_TextElementConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_AspectRatioElementConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_ImageElementConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_FloatingAttachPointType value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_FloatingAttachPoints value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_PointerCaptureMode value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_FloatingAttachToElement value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_FloatingClipToElement value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_FloatingElementConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_CustomElementConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_ClipElementConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_BorderWidth value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_BorderElementConfig value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, Clay_ElementDeclaration value) {
    userOverrides_.setValue(key, value);
    return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, TextureRef value) {
	userOverrides_.setValue(key, value);
	return *this;
}

ElementBuilder& ElementBuilder::set(std::string_view key, ElementCustomCallback value) {
    userOverrides_.setValue(key, std::move(value));
    return *this;
}

void ElementBuilder::construct(ElementDrawOptions options)
{
    if (!elementDefinition_ || !elementDefinition_->constructElment)
        throw std::runtime_error("FlowUi: elementDefinition is null or missing constructElment callback.");

    ElementParameters resolvedParameters;
    if (elementDefinition_->initializeDefaultParameters)
        elementDefinition_->initializeDefaultParameters(resolvedParameters);
    resolvedParameters.mergeFrom(userOverrides_);

    Clay_ElementId rootElementId = userInterface_.toClayEID(instanceIdPath_);

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
        const InteractionSnapshot& previousInteraction = userInterface_.getPreviousFramesInteraction();

        ElementInteractionContext eventContext{
            userInterface_,
            rootElementId,
            instanceIdPath_,
            resolvedParameters,
            bindings_,
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
        const InteractionSnapshot& previousInteraction = userInterface_.getPreviousFramesInteraction();
        ElementInteractionContext logicContext{
            userInterface_,
            rootElementId,
            instanceIdPath_,
            resolvedParameters,
            bindings_,
            previousInteraction
        };
        elementDefinition_->runLogic(logicContext);
    }

    ElementBuildContext buildContext{
        userInterface_,
        rootElementId,
        instanceIdPath_,
        resolvedParameters,
        bindings_
    };

    Clay_ElementDeclaration declaration = elementDefinition_->constructElment(buildContext);
    declaration.id = rootElementId;
    Clay__OpenElement();
    Clay__ConfigureOpenElement(declaration);
    userInterface_.pushConstructedElement(rootElementId);
}

void ElementBuilder::draw(ElementDrawOptions options)
{
    if (!elementDefinition_ || !elementDefinition_->buildElement)
        throw std::runtime_error("FlowUi: elementDefinition is null or missing buildElement callback.");

    ElementParameters resolvedParameters;
    if (elementDefinition_->initializeDefaultParameters)
        elementDefinition_->initializeDefaultParameters(resolvedParameters);
    resolvedParameters.mergeFrom(userOverrides_);

    Clay_ElementId rootElementId = userInterface_.toClayEID(instanceIdPath_);

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
        const InteractionSnapshot& previousInteraction = userInterface_.getPreviousFramesInteraction();

        ElementInteractionContext eventContext{
            userInterface_,
            rootElementId,
            instanceIdPath_,
            resolvedParameters,
            bindings_,
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
        const InteractionSnapshot& previousInteraction = userInterface_.getPreviousFramesInteraction();
        ElementInteractionContext logicContext{
            userInterface_,
            rootElementId,
            instanceIdPath_,
            resolvedParameters,
            bindings_,
            previousInteraction
        };
        elementDefinition_->runLogic(logicContext);
    }

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipBuildCallback)) {
        ElementBuildContext buildContext{
            userInterface_,
            rootElementId,
            instanceIdPath_,
            resolvedParameters,
            bindings_
        };
        elementDefinition_->buildElement(buildContext);
    }
}

} // namespace FlowUi
