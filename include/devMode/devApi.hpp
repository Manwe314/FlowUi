#pragma once

#include <type_traits>

#include "FlowUi/BuildConfig.hpp"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

template <typename Owner, typename Member>
struct FieldInfo {
	const char* name = "";
	Member Owner::* member = nullptr;
};

template <typename Owner, typename Member>
constexpr FieldInfo<Owner, Member> makeFieldInfo(const char* name, Member Owner::* member) {
	return FieldInfo<Owner, Member>{ name, member };
}

} // namespace FlowUi::devMode

// User-facing convenience macro for field reflection metadata.
#define FLOWUI_DEV_REFLECT_FIELD(TYPE, MEMBER) ::FlowUi::devMode::makeFieldInfo<TYPE>(#MEMBER, &TYPE::MEMBER)

#define FLOWUI_DEV_DETAIL_CONCAT_INNER(a, b) a##b
#define FLOWUI_DEV_DETAIL_CONCAT(a, b) FLOWUI_DEV_DETAIL_CONCAT_INNER(a, b)
#define FLOWUI_DEV_DETAIL_GET_MACRO(_1, _2, NAME, ...) NAME

#if FLOW_UI_DEV_MODE
#define FLOWUI_DEV_REGISTER_STRUCT(TYPE, ...) \
	[[maybe_unused]] static const ::FlowUi::devMode::DevRegistrar FLOWUI_DEV_DETAIL_CONCAT(_flowui_dev_struct_reg_, __COUNTER__) = \
		::FlowUi::devMode::DevRegistrar([]() { \
			::FlowUi::devMode::registerStructSchema<TYPE>(#TYPE __VA_OPT__(,) __VA_ARGS__); \
		})

#define FLOWUI_DEV_REGISTER_ELEMENT_1(DEFINITION_TYPE) \
	[[maybe_unused]] static const ::FlowUi::devMode::DevRegistrar FLOWUI_DEV_DETAIL_CONCAT(_flowui_dev_element_reg_, __COUNTER__) = \
		::FlowUi::devMode::DevRegistrar([]() { \
			::FlowUi::devMode::registerElementSchema<DEFINITION_TYPE>(#DEFINITION_TYPE); \
		})

#define FLOWUI_DEV_REGISTER_ELEMENT_2(DEFINITION_TYPE, DISPLAY_NAME) \
	[[maybe_unused]] static const ::FlowUi::devMode::DevRegistrar FLOWUI_DEV_DETAIL_CONCAT(_flowui_dev_element_reg_, __COUNTER__) = \
		::FlowUi::devMode::DevRegistrar([]() { \
			::FlowUi::devMode::registerElementSchema<DEFINITION_TYPE>(DISPLAY_NAME); \
		})

#define FLOWUI_DEV_REGISTER_ELEMENT(...) \
	FLOWUI_DEV_DETAIL_GET_MACRO(__VA_ARGS__, FLOWUI_DEV_REGISTER_ELEMENT_2, FLOWUI_DEV_REGISTER_ELEMENT_1)(__VA_ARGS__)
#else
#define FLOWUI_DEV_REGISTER_STRUCT(...)
#define FLOWUI_DEV_REGISTER_ELEMENT(...)
#endif
