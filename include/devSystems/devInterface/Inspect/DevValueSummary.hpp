#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/Public/TextureRef.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "clay.h"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#include <optional>
#include <string>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems::interface_elements {

enum class DevQuickStatus : std::uint8_t { Normal, Missing, Warning, Unavailable };

struct DevQuickView {
	enum class Kind : std::uint8_t { Text, Color, Sizing, Spacing, Status };
	std::optional<TextureRef> thumbnail{};
	std::string primary{};
	std::string secondary{};
	/* Complete bounded value text, retained for tooltips and copy actions. */
	std::string detail{};
	std::optional<Clay_Color> swatch{};
	Kind kind = Kind::Text;
	DevQuickStatus status = DevQuickStatus::Normal;
};

/* Availability of the snapshot, independently of its schema's capture capability. */
enum class DevSummaryAvailability : std::uint8_t {
	NotCaptured,
	CopyFailed,
	CollectionDisabled,
	SchemaChanged,
};

/* Build a bounded semantic value summary without dereferencing unsupported metadata. */
[[nodiscard]] DevQuickView
format_value_summary(const tooling::DevSchemaGeneration& schema, const tooling::DevTypeSchema* type,
					 const void* value, App* app = nullptr,
					 const tooling::DevFieldSchema* field = nullptr,
					 DevSummaryAvailability availability = DevSummaryAvailability::NotCaptured);

} // namespace FlowUi::devSystems::interface_elements
#endif
