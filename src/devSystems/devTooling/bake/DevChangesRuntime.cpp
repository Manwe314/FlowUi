#include "devSystems/devTooling/bake/DevChangesRuntime.hpp"
#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace FlowUi::devSystems::tooling {
namespace {
struct ResolvedRuntimeField {
	std::vector<const tooling::DevFieldSchema*> path{};
	DevOverrideFieldKey key{};
	tooling::DevTypeIndex type{};
};
[[nodiscard]] ResolvedRuntimeField resolve_runtime_field(const tooling::DevSchemaGeneration& schema,
														 const DevBakeEntry& entry) {
	ResolvedRuntimeField result;
	tooling::DevTypeIndex owner{};
	if (entry.targetKind == DevBakeTargetKind::Element) {
		const auto* element = schema.findElement(entry.definition);
		if (!element)
			return result;
		owner = element->parametersType;
	} else {
		const auto* theme = schema.findTheme(entry.themeType);
		if (!theme)
			return result;
		owner = theme->themeType;
	}
	const auto* root = schema.type(owner);
	if (!root)
		return result;
	result.key.ownerType = root->id;
	std::string_view remaining = entry.fieldPath;
	result.path.reserve(8);
	result.key.nestedPath.reserve(8);
	while (!remaining.empty()) {
		const auto separator = remaining.find('.');
		const auto name = remaining.substr(0, separator);
		const auto fields = schema.fieldsOf(owner);
		const auto field = std::ranges::find_if(
			fields, [&](const auto& candidate) { return schema.string(candidate.name) == name; });
		if (field == fields.end() || field->operations >= schema.fieldOperations.size() ||
			!schema.fieldOperations[field->operations])
			return {};
		result.path.emplace_back(std::addressof(*field));
		if (separator == std::string_view::npos) {
			if (field->id != entry.fieldId || !schema.type(field->valueType) ||
				schema.type(field->valueType)->id != entry.valueType)
				return {};
			result.key.field = field->id;
			result.type = field->valueType;
			return result;
		}
		result.key.nestedPath.emplace_back(field->id);
		owner = field->valueType;
		remaining.remove_prefix(separator + 1);
	}
	return {};
}
[[nodiscard]] const void* field_address(const tooling::DevSchemaGeneration& schema,
										const ResolvedRuntimeField& field, const void* owner,
										std::size_t begin = 0) noexcept {
	for (std::size_t index = begin; owner && index < field.path.size(); ++index) {
		const auto* operations = schema.fieldOperations[field.path[index]->operations];
		owner = operations && operations->constAddress ? operations->constAddress(owner) : nullptr;
	}
	return owner;
}

/* The manifest codec emits numeric scalars and complete reflected objects. Parse into an owned
 * typed clone so padding, constructors and assignment semantics remain the type's responsibility.
 */
class ReflectedJsonReader {
public:
	ReflectedJsonReader(const tooling::DevSchemaGeneration& schema, std::string_view input,
						DevOwnedValue& owner)
		: schema_(schema), input_(input), owner_(owner) {}
	[[nodiscard]] bool decode(tooling::DevTypeIndex type, void* destination) {
		if (!value(type, destination, 0))
			return false;
		space();
		return position_ == input_.size();
	}
	[[nodiscard]] std::string_view diagnostic() const noexcept {
		return incomplete_texture_
				   ? "Baked texture metadata must contain domain, key, fitMode and tintEnabled."
				   : "Already baked value cannot be materialized by the reflected "
					 "numeric/object/action/resource codec; retained live values remain usable.";
	}

private:
	[[nodiscard]] bool string_value(std::string& output) {
		if (!consume('"'))
			return false;
		while (position_ < input_.size()) {
			const unsigned char character = input_[position_++];
			if (character == '"')
				return true;
			if (character < 32)
				return false;
			if (character != '\\') {
				output += static_cast<char>(character);
				continue;
			}
			if (position_ == input_.size())
				return false;
			switch (input_[position_++]) {
			case '"':
				output += '"';
				break;
			case '\\':
				output += '\\';
				break;
			case '/':
				output += '/';
				break;
			case 'b':
				output += '\b';
				break;
			case 'f':
				output += '\f';
				break;
			case 'n':
				output += '\n';
				break;
			case 'r':
				output += '\r';
				break;
			case 't':
				output += '\t';
				break;
			default:
				return false;
			}
		}
		return false;
	}
	[[nodiscard]] bool texture_value(void* destination) {
		if (!consume('{'))
			return false;
		std::string key;
		std::uint64_t domain = 0, fit = 0;
		std::uint32_t members = 0;
		bool tint = false;
		for (;;) {
			std::string_view member;
			if (!name(member) || !consume(':'))
				return false;
			std::uint32_t flag = 0;
			if (member == "domain")
				flag = 1;
			else if (member == "key")
				flag = 2;
			else if (member == "fitMode")
				flag = 4;
			else if (member == "tintEnabled")
				flag = 8;
			else
				return false;
			if (members & flag)
				return false;
			members |= flag;
			if (flag == 1 && !unsigned_value(domain))
				return false;
			if (flag == 2 && !string_value(key))
				return false;
			if (flag == 4 && !unsigned_value(fit))
				return false;
			if (flag == 8) {
				space();
				if (input_.substr(position_, 4) == "true") {
					tint = true;
					position_ += 4;
				} else if (input_.substr(position_, 5) == "false")
					position_ += 5;
				else
					return false;
			}
			if (consume('}'))
				break;
			if (!consume(','))
				return false;
		}
		if (members != 15u) {
			incomplete_texture_ = true;
			return false;
		}
		if (key.empty() ||
			(domain != static_cast<std::uint64_t>(ResourceDomain::Image) &&
			 domain != static_cast<std::uint64_t>(ResourceDomain::Icon)) ||
			fit > static_cast<std::uint64_t>(TextureFitMode::None))
			return false;
		auto& texture = *static_cast<TextureRef*>(destination);
		texture = TextureRef::fromStable(static_cast<ResourceDomain>(domain), {},
										 static_cast<TextureFitMode>(fit), tint);
		return owner_.retain_text_reference(texture.sourceKey, std::move(key));
	}
	[[nodiscard]] bool unsigned_value(std::uint64_t& output) noexcept {
		space();
		const auto* begin = input_.data() + position_;
		const auto parsed = std::from_chars(begin, input_.data() + input_.size(), output);
		if (parsed.ec != std::errc{} || parsed.ptr == begin ||
			(*begin == '0' && parsed.ptr - begin > 1))
			return false;
		position_ = static_cast<std::size_t>(parsed.ptr - input_.data());
		return true;
	}
	[[nodiscard]] bool action_value(void* destination) noexcept {
		if (!consume('{'))
			return false;
		std::uint64_t kind = 0, identifier = 0;
		bool has_kind = false, has_identifier = false;
		for (;;) {
			std::string_view member;
			if (!name(member) || !consume(':'))
				return false;
			if (member == "kind" && !has_kind) {
				if (!unsigned_value(kind))
					return false;
				has_kind = true;
			} else if (member == "id" && !has_identifier) {
				if (!unsigned_value(identifier))
					return false;
				has_identifier = true;
			} else {
				return false;
			}
			if (consume('}'))
				break;
			if (!consume(','))
				return false;
		}
		if (!has_kind || !has_identifier ||
			!((kind == 0 && identifier == 0) ||
			  ((kind == static_cast<std::uint64_t>(ActionCallKind::App) ||
				kind == static_cast<std::uint64_t>(ActionCallKind::Ui)) &&
			   identifier != 0)))
			return false;
		*static_cast<ActionCall*>(destination) =
			ActionCall::fromStable(static_cast<ActionCallKind>(kind), identifier);
		return true;
	}
	void space() noexcept {
		while (position_ < input_.size() &&
			   (input_[position_] == ' ' || input_[position_] == '\n' ||
				input_[position_] == '\r' || input_[position_] == '\t'))
			++position_;
	}
	[[nodiscard]] bool consume(char character) noexcept {
		space();
		if (position_ == input_.size() || input_[position_] != character)
			return false;
		++position_;
		return true;
	}
	[[nodiscard]] bool name(std::string_view& output) noexcept {
		if (!consume('"'))
			return false;
		const auto begin = position_;
		while (position_ < input_.size() && input_[position_] != '"') {
			if (static_cast<unsigned char>(input_[position_]) < 32 || input_[position_] == '\\')
				return false;
			++position_;
		}
		if (position_ == input_.size())
			return false;
		output = input_.substr(begin, position_ - begin);
		++position_;
		return true;
	}
	[[nodiscard]] bool value(tooling::DevTypeIndex index, void* destination, uint32_t depth) {
		const auto* type = schema_.type(index);
		if (!type || !destination || depth > 12 || index.value >= schema_.typeOperations.size())
			return false;
		const auto* operations = schema_.typeOperations[index.value];
		if (!operations)
			return false;
		space();
		if (type->id == ::FlowUi::detail::typeHash<ActionCall>())
			return action_value(destination);
		if (type->id == ::FlowUi::detail::typeHash<TextureRef>()) {
			return texture_value(destination);
		}
		if (type->kind == tooling::DevTypeKind::Object) {
			if (!consume('{'))
				return false;
			const auto fields = schema_.fieldsOf(index);
			std::vector<tooling::DevFieldId> decoded;
			decoded.reserve(fields.size());
			if (!consume('}')) {
				for (;;) {
					std::string_view member;
					if (!name(member) || !consume(':'))
						return false;
					const auto found = std::ranges::find_if(fields, [&](const auto& field) {
						return schema_.string(field.name) == member;
					});
					if (found == fields.end() ||
						std::ranges::find(decoded, found->id) != decoded.end() ||
						found->operations >= schema_.fieldOperations.size())
						return false;
					const auto* access = schema_.fieldOperations[found->operations];
					if (!access || !access->mutableAddress ||
						!value(found->valueType, access->mutableAddress(destination), depth + 1))
						return false;
					decoded.emplace_back(found->id);
					if (consume('}'))
						break;
					if (!consume(','))
						return false;
				}
			}
			return decoded.size() == fields.size();
		}
		if (!operations->assignNumericValue)
			return false;
		long double numeric = 0;
		if (type->kind == tooling::DevTypeKind::Boolean) {
			if (input_.substr(position_, 4) == "true") {
				numeric = 1;
				position_ += 4;
			} else if (input_.substr(position_, 5) == "false")
				position_ += 5;
			else
				return false;
		} else {
			if (type->kind != tooling::DevTypeKind::SignedInteger &&
				type->kind != tooling::DevTypeKind::UnsignedInteger &&
				type->kind != tooling::DevTypeKind::FloatingPoint &&
				type->kind != tooling::DevTypeKind::Enumeration)
				return false;
			const auto parsed =
				std::from_chars(input_.data() + position_, input_.data() + input_.size(), numeric);
			if (parsed.ec != std::errc{} || !std::isfinite(numeric))
				return false;
			position_ = static_cast<std::size_t>(parsed.ptr - input_.data());
		}
		return operations->assignNumericValue(destination, numeric) ==
			   tooling::DevValueOperationStatus::Success;
	}
	const tooling::DevSchemaGeneration& schema_;
	std::string_view input_;
	DevOwnedValue& owner_;
	std::size_t position_ = 0;
	bool incomplete_texture_ = false;
};
} // namespace

DevChangesRuntimeEdit prepare_changes_runtime_edit(const tooling::DevSchemaGeneration& schema,
												   const DevOverrideEngine& engine,
												   const ThemeManager& themes,
												   const DevChangesReviewRow& row, WindowId window,
												   DevChangesRuntimeAction action) noexcept {
	DevChangesRuntimeEdit result;
	try {
		const auto* entry = action == DevChangesRuntimeAction::ApplySaved
								? (row.disk_entry ? &*row.disk_entry : nullptr)
							: action == DevChangesRuntimeAction::ApplyProposal
								? (row.proposed_entry ? &*row.proposed_entry : nullptr)
							: row.proposed_entry ? &*row.proposed_entry
							: row.disk_entry	 ? &*row.disk_entry
												 : nullptr;
		if (row.domain == DevChangesDomain::Memory) {
			result.error = "Initial memory settings apply after rebuild; no live allocation action "
						   "is available.";
			return result;
		}
		if (!entry) {
			result.error = "This row has no value for the requested baseline.";
			return result;
		}
		const auto field = resolve_runtime_field(schema, *entry);
		if (!field.type) {
			result.error =
				"The already baked field identity or type does not resolve in this executable.";
			return result;
		}
		DevOverrideCommand command;
		command.field = field.key;
		command.preserve_baked_assignment = true;
		DevOwnedValue previous, observed, authored;
		bool retained = false;
		if (entry->targetKind == DevBakeTargetKind::Element) {
			command.kind = DevOverrideCommandKind::SetElementField;
			command.element = {.definition = entry->definition,
							   .window = window,
							   .instance = entry->instanceKey,
							   .instanceDebugLabel = entry->instanceDebugLabel,
							   .scope = entry->targetScope};
			command.layer = entry->targetScope == DevOverrideScope::Definition
								? DevOverrideLayer::LiveDefinition
								: DevOverrideLayer::LiveInstance;
			for (const auto& record : engine.appliedOverrides().records()) {
				if (record.target.definition != entry->definition ||
					record.target.scope != entry->targetScope || record.field != field.key ||
					record.layer != command.layer ||
					(entry->targetScope == DevOverrideScope::ExactInstance &&
					 (record.target.window != window ||
					  record.target.instance != entry->instanceKey)))
					continue;
				command.element = record.target;
				previous = record.value;
				retained = true;
				break;
			}
			auto authored_target = command.element;
			if (authored_target.scope == DevOverrideScope::Definition)
				authored_target.window = InvalidWindowId;
			const auto authored_status = query_authored_element_value(
				engine, schema, authored_target, field.key, authored);
			if (action == DevChangesRuntimeAction::ResetAuthored &&
				authored_status == DevValueCaptureStatus::Ambiguous) {
				result.error = "Authored inputs differ between observed instances; select an exact "
					"instance before resetting to its application-supplied value.";
				return result;
			}
			const auto& capture = engine.elementSnapshot(window);
			if (capture.schema && capture.schema->generation == schema.generation)
				for (const auto& element : capture.elements) {
					if (element.definition != entry->definition ||
						(entry->targetScope == DevOverrideScope::ExactInstance &&
						 element.instance != entry->instanceKey))
						continue;
					for (uint32_t offset = 0; offset < element.fieldCount; ++offset) {
						const auto& captured = capture.fields[element.firstField + offset];
						if (!captured.field || captured.field.value > schema.fields.size() ||
							schema.fields[captured.field.value - 1].id != field.path.front()->id)
							continue;
						const auto* source = field_address(schema, field, captured.value.data(), 1);
						if (source)
							(void)DevOwnedValue::copyFrom(schema, field.type, source, observed);
						break;
					}
					if (observed)
						break;
				}
		} else {
			command.kind = DevOverrideCommandKind::SetThemeField;
			command.theme = {.themeType = entry->themeType, .variant = entry->themeVariant};
			for (const auto& record : engine.themeBakeRecords())
				if (record.target == command.theme && record.field == field.key) {
					previous = record.value;
					retained = true;
					break;
				}
			struct ThemeContext {
				const tooling::DevSchemaGeneration* schema;
				const ResolvedRuntimeField* field;
				const DevThemeOverrideTarget* target;
				DevOwnedValue* observed;
				DevOwnedValue* authored;
			} context{&schema, &field, &command.theme, &observed, &authored};
			(void)themes.visitDevCatalogueThemes(
				&context,
				[](void* pointer, const ThemeManager::DevThemePayloadView& view) noexcept {
					auto& context = *static_cast<ThemeContext*>(pointer);
					if (view.type != context.target->themeType ||
						(context.target->variant == "*" ? !view.active
														: view.variant != context.target->variant))
						return true;
					const auto* live = field_address(*context.schema, *context.field, view.payload);
					const auto* original =
						field_address(*context.schema, *context.field, view.authored);
					if (live)
						(void)DevOwnedValue::copyFrom(*context.schema, context.field->type, live,
													  *context.observed);
					if (original)
						(void)DevOwnedValue::copyFrom(*context.schema, context.field->type,
													  original, *context.authored);
					return false;
				});
		}
		DevOverrideCommand inverse = command;
		if (retained)
			inverse.value = previous;
		else
			inverse.kind = entry->targetKind == DevBakeTargetKind::Element
							   ? DevOverrideCommandKind::ClearElementField
							   : DevOverrideCommandKind::ClearThemeField;
		if (action == DevChangesRuntimeAction::DiscardLive) {
			if (!retained) {
				result.error =
					"No retained live edit exists for this target in the selected window.";
				return result;
			}
			command.kind = entry->targetKind == DevBakeTargetKind::Element
							   ? DevOverrideCommandKind::ClearElementField
							   : DevOverrideCommandKind::ClearThemeField;
		} else if (action == DevChangesRuntimeAction::ResetAuthored) {
			if (!authored || entry->themeVariant == "*") {
				result.error = "Authored baseline unavailable for this scope; open the authoring "
							   "surface for per-variant reset.";
				return result;
			}
			command.value = authored;
		} else if (action == DevChangesRuntimeAction::ApplyProposal && retained)
			command.value = previous;
		else {
			command.value = observed ? observed : previous;
			if (!command.value) {
				result.error = "Typed value unavailable: observe this element in the selected "
							   "window before applying an already baked value.";
				return result;
			}
			ReflectedJsonReader reader(schema, entry->valueJson, command.value);
			if (!reader.decode(field.type, command.value.data())) {
				result.error = reader.diagnostic();
				return result;
			}
		}
		if (command.kind != DevOverrideCommandKind::ClearElementField &&
			command.kind != DevOverrideCommandKind::ClearThemeField && !command.value) {
			result.error = "Typed value allocation failed.";
			return result;
		}
		result.forward.reserve(1);
		result.inverse.reserve(1);
		result.forward.emplace_back(std::move(command));
		result.inverse.emplace_back(std::move(inverse));
		result.available = true;
	} catch (...) {
		result.error = "Unable to allocate the typed runtime command draft.";
	}
	return result;
}
} // namespace FlowUi::devSystems::tooling
#endif
