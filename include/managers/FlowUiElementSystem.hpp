#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <stdexcept>
#include <utility>

#include "clay.h"
#include "FlowUi/PublicStructs.hpp"

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

// ----------------------------
// Element parameter values
// ----------------------------
using ElementParameterValue = std::variant<
    bool,
    int,
    float,
    std::string,
    Clay_Color,
    Clay_Sizing,
    Clay_Padding,
	Clay_LayoutDirection,
	Clay_LayoutConfig,
	Clay_ElementDeclaration,
	TextureRef
>;

class ElementParameters {
public:
    ElementParameters() = default;

    template <typename T>
    void setValue(std::string_view key, T&& value)
	{
        parameterMap_[std::string(key)] = ElementParameterValue(std::forward<T>(value));
    }

    bool hasValue(std::string_view key) const
	{
        return parameterMap_.find(std::string(key)) != parameterMap_.end();
    }

    template <typename T>
    T getValue(std::string_view key, const T& defaultValue) const
	{
        auto it = parameterMap_.find(std::string(key));
        if (it == parameterMap_.end()) return defaultValue;
        if (const auto* typed = std::get_if<T>(&it->second)) return *typed;
        return defaultValue;
    }

	template <typename T>
	T getValue(std::string_view key) const 
		requires std::is_default_constructible_v<T>
	{
		return getValue<T>(key, T{});
	}

    std::string_view getString(std::string_view key, std::string_view defaultValue = {}) const
	{
        auto it = parameterMap_.find(std::string(key));
        if (it == parameterMap_.end()) return defaultValue;
        if (const auto* typed = std::get_if<std::string>(&it->second)) return *typed;
        return defaultValue;
    }

    void mergeFrom(const ElementParameters& other)
	{
        for (const auto& kv : other.parameterMap_)
		{
            parameterMap_[kv.first] = kv.second;
        }
    }

private:
    std::unordered_map<std::string, ElementParameterValue> parameterMap_;
};


struct ElementBindingEntry {
    void* pointer = nullptr;
    const std::type_info* typeInfo = nullptr;
};

class ElementBindings {
public:
    ElementBindings() = default;

    template <typename T>
    void bind(std::string_view key, T& reference)
	{
        ElementBindingEntry entry;
        entry.pointer = static_cast<void*>(&reference);
        entry.typeInfo = &typeid(T);
        bindingMap_[std::string(key)] = entry;
    }

    template <typename T>
    T* getPointer(std::string_view key) const
	{
        auto it = bindingMap_.find(std::string(key));
        if (it == bindingMap_.end()) return nullptr;
        if (*(it->second.typeInfo) != typeid(T)) return nullptr;
        return static_cast<T*>(it->second.pointer);
    }

private:
    std::unordered_map<std::string, ElementBindingEntry> bindingMap_;
};

class UiManager;


struct ElementBuildContext
{
    UiManager& userInterface;
    Clay_ElementId elementId;
    std::string_view instanceIdPath;
    ElementParameters& parameters;
    ElementBindings& bindings;

    Clay_ElementId createChildElementId(std::string_view localChildId) const;
};

struct ElementEventContext
{
    UiManager& userInterface;
    Clay_ElementId elementId;
    std::string_view instanceIdPath;
    ElementParameters& parameters;
    ElementBindings& bindings;
    const InteractionSnapshot& previousInteraction;

    Clay_ElementId createChildElementId(std::string_view localChildId) const;
};

struct ElementLogicContext
{
    UiManager& userInterface;
    Clay_ElementId elementId;
    std::string_view instanceIdPath;
    ElementParameters& parameters;
    ElementBindings& bindings;
    const InteractionSnapshot& previousInteraction;

    Clay_ElementId createChildElementId(std::string_view localChildId) const;
};


struct ElementDefinition
{
    std::string elementTypeName;

    std::function<void(ElementParameters& defaultParameters)> initializeDefaultParameters;

    std::function<void(ElementEventContext&)> onHovered;
    std::function<void(ElementEventContext&)> onPressed;
    std::function<void(ElementEventContext&)> onHeld;
    std::function<void(ElementEventContext&)> onReleased;

    std::function<void(ElementLogicContext&)> runLogic;

    std::function<void(ElementBuildContext&)> buildElement;
};


class ElementRegistry
{
public:
    void registerElement(ElementDefinition definition);
    const ElementDefinition* findElement(std::string_view elementTypeName) const;

private:
    std::unordered_map<std::string, ElementDefinition> elementDefinitions_;
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


class ElementBuilder {
public:
    ElementBuilder(UiManager& userInterface, const ElementDefinition* definition, std::string instanceIdPath);

    ElementBuilder& set(std::string_view key, bool value);
    ElementBuilder& set(std::string_view key, int value);
    ElementBuilder& set(std::string_view key, float value);
    ElementBuilder& set(std::string_view key, std::string_view value);
    ElementBuilder& set(std::string_view key, const char* value);
    ElementBuilder& set(std::string_view key, Clay_Color value);
    ElementBuilder& set(std::string_view key, Clay_Sizing value);
    ElementBuilder& set(std::string_view key, Clay_Padding value);
	ElementBuilder& set(std::string_view key, Clay_LayoutDirection value);
	ElementBuilder& set(std::string_view key, Clay_LayoutConfig value);
	ElementBuilder& set(std::string_view key, Clay_ElementDeclaration value);
	ElementBuilder& set(std::string_view key, TextureRef value);

    template <typename T>
    ElementBuilder& bind(std::string_view key, T& reference)
	{
        bindings_.bind<T>(key, reference);
        return *this;
    }

    void draw(ElementDrawOptions options = ElementDrawOptions::Default);

private:
    UiManager& userInterface_;
    const ElementDefinition* elementDefinition_;
    std::string instanceIdPath_;
    ElementParameters userOverrides_;
    ElementBindings bindings_;
};

} // namespace FlowUi
