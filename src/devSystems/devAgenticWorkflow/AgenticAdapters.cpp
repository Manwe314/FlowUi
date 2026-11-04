#include "devSystems/devAgenticWorkflow/AgenticAdapters.hpp"

#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/ThemeManager.hpp"
#include "managers/structs/InputFieldManagerStructs.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>

namespace FlowUi::devSystems::agentic {
namespace {

[[nodiscard]] Json source_path(std::string_view source) {
	if (source.empty())
		return {};
	std::filesystem::path path(source);
	if (path.is_absolute()) {
		path = path.lexically_relative(std::filesystem::current_path());
		if (path.empty())
			return {};
	}
	path = path.lexically_normal();
	for (const auto& component : path)
		if (component == "..")
			return {};
	return Json(path.generic_string());
}

[[nodiscard]] Json source_site(std::string_view file, std::string_view function, std::uint32_t line,
							   std::uint32_t column) {
	return Json::make_object({{"file", source_path(file)},
							  {"symbol", Json(function)},
							  {"line", Json::number(line)},
							  {"column", Json::number(column)},
							  {"repository_id", "application_or_library"}});
}

[[nodiscard]] Json node_index(std::uint32_t index) {
	return index == std::numeric_limits<std::uint32_t>::max() ? Json{} : Json::number(index);
}

[[nodiscard]] Json bounds(float x, float y, float width, float height) {
	return Json::make_object({{"x", Json(static_cast<double>(x))},
							  {"y", Json(static_cast<double>(y))},
							  {"width", Json(static_cast<double>(width))},
							  {"height", Json(static_cast<double>(height))}});
}

[[nodiscard]] Json resource_key(const ResourceKey& key) {
	return Json::make_object({{"name", Json(key.name)},
							  {"domain", Json::number(static_cast<std::uint8_t>(key.domain))},
							  {"window_id", decimal(key.window)}});
}

/** This documented built-in remains opaque to generic editing, but its values are portable. */
[[nodiscard]] Json overlay_style_value(const InputFieldOverlayStyle& style) {
	const std::array<double, 18> components{style.caretThicknessPx,
											style.caretBlockWidthPx,
											style.caretHeightOverflowTopPx,
											style.caretHeightOverflowBottomPx,
											style.caretBlinkPeriodSeconds,
											style.caretBlinkVisibleSeconds,
											style.caretColor.r,
											style.caretColor.g,
											style.caretColor.b,
											style.caretColor.a,
											style.selectionBoxColor.r,
											style.selectionBoxColor.g,
											style.selectionBoxColor.b,
											style.selectionBoxColor.a,
											style.selectedTextColor.r,
											style.selectedTextColor.g,
											style.selectedTextColor.b,
											style.selectedTextColor.a};
	if (std::any_of(components.begin(), components.end(),
					[](double component) { return !std::isfinite(component); }) ||
		static_cast<std::uint8_t>(style.caretShape) >
			static_cast<std::uint8_t>(InputCaretShape::Underline))
		return Json::make_object(
			{{"available", false}, {"reason", "invalid_builtin_overlay_style"}});
	const auto color = [](const Clay_Color& value) {
		return Json::make_object({{"r", Json(static_cast<double>(value.r))},
								  {"g", Json(static_cast<double>(value.g))},
								  {"b", Json(static_cast<double>(value.b))},
								  {"a", Json(static_cast<double>(value.a))}});
	};
	return Json::make_object(
		{{"caretShape", Json::number(static_cast<std::uint8_t>(style.caretShape))},
		 {"caretThicknessPx", Json(static_cast<double>(style.caretThicknessPx))},
		 {"caretBlockWidthPx", Json(static_cast<double>(style.caretBlockWidthPx))},
		 {"caretHeightOverflowTopPx", Json(static_cast<double>(style.caretHeightOverflowTopPx))},
		 {"caretHeightOverflowBottomPx",
		  Json(static_cast<double>(style.caretHeightOverflowBottomPx))},
		 {"caretColor", color(style.caretColor)},
		 {"selectionBoxColor", color(style.selectionBoxColor)},
		 {"selectedTextColor", color(style.selectedTextColor)},
		 {"caretBlinkPeriodSeconds", Json(style.caretBlinkPeriodSeconds)},
		 {"caretBlinkVisibleSeconds", Json(style.caretBlinkVisibleSeconds)}});
}

[[nodiscard]] Json reflected_value(const devMode::DevSchemaGeneration& schema,
								   devMode::DevTypeIndex type_index, const void* value,
								   std::uint32_t depth = 0) {
	if (!value)
		return {};
	if (depth >= 16u)
		return Json::make_object({{"available", false}, {"reason", "value_depth_exceeded"}});
	const auto* type = schema.type(type_index);
	if (type && type->id == ::FlowUi::detail::typeHash<InputFieldOverlayStyle>())
		return overlay_style_value(*static_cast<const InputFieldOverlayStyle*>(value));
	const auto* operations = type_index.value < schema.typeOperations.size()
								 ? schema.typeOperations[type_index.value]
								 : nullptr;
	if (type && operations) {
		if (type->kind == devMode::DevTypeKind::Text && operations->textView) {
			const auto text = operations->textView(value);
			if (text.size() > 65536u)
				return Json::make_object({{"available", false},
										  {"reason", "field_text_budget_exceeded"},
										  {"bytes", decimal(text.size())}});
			return Json(text);
		}
		if (type->kind == devMode::DevTypeKind::Pointer && operations->pointerValue) {
			const void* pointer = nullptr;
			const bool available = operations->pointerValue(value, pointer);
			return Json::make_object(
				{{"available", available}, {"pointer_present", pointer != nullptr}});
		}
		if (type->kind == devMode::DevTypeKind::Optional && operations->optionalHasValue &&
			operations->optionalValueAddress)
			return operations->optionalHasValue(value)
					   ? reflected_value(schema, type->elementType,
										 operations->optionalValueAddress(value), depth + 1u)
					   : Json{};
		if (type->kind == devMode::DevTypeKind::Sequence && operations->sequenceSize &&
			operations->sequenceElementAddress) {
			const std::size_t count = operations->sequenceSize(value);
			if (count > 4096u)
				return Json::make_object({{"available", false},
										  {"reason", "field_sequence_budget_exceeded"},
										  {"count", decimal(count)}});
			Json values = Json::make_array();
			values.array.reserve(count);
			for (std::size_t index = 0; index < count; ++index)
				values.array.emplace_back(
					reflected_value(schema, type->elementType,
									operations->sequenceElementAddress(value, index), depth + 1u));
			return values;
		}
		if (type->kind == devMode::DevTypeKind::Object) {
			Json values = Json::make_object();
			for (const auto& field : schema.fieldsOf(type_index)) {
				const auto* field_operations = field.operations < schema.fieldOperations.size()
												   ? schema.fieldOperations[field.operations]
												   : nullptr;
				values.set(std::string(schema.string(field.name)),
						   field_operations && field_operations->constAddress
							   ? reflected_value(schema, field.valueType,
												 field_operations->constAddress(value), depth + 1u)
							   : Json::make_object({{"available", false},
													{"reason", "field_access_unavailable"}}));
			}
			return values;
		}
	}
	const auto formatted = tooling::serialize_dev_value(schema, type_index, value);
	if (!formatted)
		return Json::make_object({{"available", false}, {"reason", "no_value_serializer"}});
	auto parsed = parse_json(formatted->json);
	if (!parsed)
		return Json::make_object({{"available", false}, {"reason", "invalid_native_value"}});
	if (type && type->kind == devMode::DevTypeKind::UnsignedInteger && type->size == 8u &&
		parsed->kind == Json::Kind::number)
		return Json(parsed->text);
	return std::move(*parsed);
}

[[nodiscard]] Json constraint(const devMode::DevSchemaGeneration& schema,
							  const devMode::DevFieldSchema& field) {
	Json result = Json::make_object();
	if (field.constraint < schema.constraints.size()) {
		const auto& limits = schema.constraints[field.constraint];
		if (limits.numeric.hasMinimum)
			result.set("minimum", Json(limits.numeric.minimum));
		if (limits.numeric.hasMaximum)
			result.set("maximum", Json(limits.numeric.maximum));
		if (limits.numeric.hasStep)
			result.set("step", Json(limits.numeric.step));
		if (limits.hasTextMaximum)
			result.set("maximum_text_bytes", Json::number(limits.textMaximum));
	}
	return result;
}

void append_fields(Json& fields, const devMode::DevSchemaGeneration& schema,
				   devMode::DevTypeIndex owner_type, const void* effective, const void* authored,
				   const void* startup, std::string_view parent_path, std::uint32_t depth,
				   std::uint64_t maximum_fields) {
	if (depth >= 16u)
		throw FlowUiException(
			makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
	for (const auto& field : schema.fieldsOf(owner_type)) {
		if (fields.array.size() >= maximum_fields)
			throw FlowUiException(
				makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
		const auto* type = schema.type(field.valueType);
		const auto* operations = field.operations < schema.fieldOperations.size()
									 ? schema.fieldOperations[field.operations]
									 : nullptr;
		const std::string path = parent_path.empty() ? std::string(schema.string(field.name))
													 : std::string(parent_path) + "." +
														   std::string(schema.string(field.name));
		const void* effective_value = operations && operations->constAddress && effective
										  ? operations->constAddress(effective)
										  : nullptr;
		const void* authored_value = operations && operations->constAddress && authored
										 ? operations->constAddress(authored)
										 : nullptr;
		const void* startup_value = operations && operations->constAddress && startup
										? operations->constAddress(startup)
										: nullptr;
		Json entry = Json::make_object(
			{{"path", path},
			 {"field_id", decimal(field.id)},
			 {"display_name", Json(schema.string(field.displayName))},
			 {"hint", Json(schema.string(field.hint))},
			 {"type_id", decimal(type ? type->id : 0u)},
			 {"type_name", type ? Json(schema.string(type->cppTypeName)) : Json{}},
			 {"type_kind", Json::number(type ? static_cast<std::uint8_t>(type->kind) : 0u)},
			 {"edit_capability", Json::number(static_cast<std::uint8_t>(field.effectiveEdit))},
			 {"capability_reason", Json::number(static_cast<std::uint16_t>(field.reason))},
			 {"constraints", constraint(schema, field)},
			 {"source",
			  source_site(schema.string(field.source.file), schema.string(field.source.function),
						  field.source.line, field.source.column)}});
		if (type && type->kind == devMode::DevTypeKind::Enumeration) {
			Json choices = Json::make_array();
			const std::uint64_t end = static_cast<std::uint64_t>(type->enumeration.values.first) +
									  type->enumeration.values.count;
			for (std::uint64_t index = type->enumeration.values.first;
				 index < std::min<std::uint64_t>(end, schema.enumValues.size()); ++index)
				choices.array.emplace_back(
					Json::make_object({{"name", Json(schema.string(schema.enumValues[index].name))},
									   {"bits", decimal(schema.enumValues[index].bits)}}));
			entry.set("choices", std::move(choices));
		}
		if (type && type->kind == devMode::DevTypeKind::Object) {
			entry.set("value_kind", "object_fields");
			fields.array.emplace_back(std::move(entry));
			append_fields(fields, schema, field.valueType, effective_value, authored_value,
						  startup_value, path, depth + 1u, maximum_fields);
		} else {
			entry.set("effective", reflected_value(schema, field.valueType, effective_value));
			entry.set("authored", reflected_value(schema, field.valueType, authored_value));
			entry.set("startup", reflected_value(schema, field.valueType, startup_value));
			entry.set("preview", Json{});
			entry.set("preview_observation", "effective_payload_only");
			fields.array.emplace_back(std::move(entry));
		}
	}
}

struct ThemeExportContext {
	Json themes = Json::make_array();
	const devMode::DevSchemaGeneration* schema = nullptr;
	std::uint64_t maximum_records = 0;
	std::uint64_t maximum_bytes = 0;
	FlowUiError error{};
	bool failed = false;
};

bool visit_theme(void* owner, const ThemeManager::DevThemePayloadView& view) noexcept {
	auto& context = *static_cast<ThemeExportContext*>(owner);
	try {
		if (context.themes.array.size() >= context.maximum_records)
			throw FlowUiException(
				makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
		Json entry = Json::make_object(
			{{"type_id", decimal(view.type)},
			 {"type_name", Json(view.typeName)},
			 {"variant", Json(view.variant)},
			 {"active", view.active},
			 {"revision", decimal(view.revision)},
			 {"schema_generation", decimal(context.schema ? context.schema->generation : 0u)}});
		Json fields = Json::make_array();
		const auto* theme_schema = context.schema ? context.schema->findTheme(view.type) : nullptr;
		if (theme_schema) {
			append_fields(fields, *context.schema, theme_schema->themeType, view.payload,
						  view.authored, view.startup, {}, 0u, context.maximum_records);
			entry.set("status", "reflected");
		} else {
			entry.set("status", "schema_unavailable");
		}
		entry.set("fields", std::move(fields));
		context.themes.array.emplace_back(std::move(entry));
		if (write_json(context.themes).size() > context.maximum_bytes)
			throw FlowUiException(
				makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
		return true;
	} catch (const FlowUiException& exception) {
		context.error = exception.error();
		context.failed = true;
		return false;
	} catch (...) {
		context.error = makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticSnapshot);
		context.failed = true;
		return false;
	}
}

[[nodiscard]] Json owned_theme_document(const AgenticCaptureSnapshot& snapshot,
										DevTooling& tooling_service, ThemeManager& themes) {
	const auto schema = tooling_service.schemas().view();
	ThemeExportContext context;
	context.schema = schema ? &*schema : nullptr;
	context.maximum_records = snapshot.request.limits.maximum_records;
	context.maximum_bytes = snapshot.request.limits.maximum_snapshot_bytes;
	context.themes.array.reserve(
		std::min<std::uint64_t>(themes.devThemeCount(), context.maximum_records));
	const bool visited = themes.visitDevCatalogueThemes(&context, &visit_theme);
	if (context.failed)
		throw FlowUiException(context.error);
	if (!visited && themes.devThemeCount() != 0u)
		throw FlowUiException(
			makeError(ErrorCode::AgenticIncompleteCapture, ErrorSite::AgenticSnapshot));
	Json theme_export =
		Json::make_object({{"schema", "FlowUi.Agentic.Themes"},
						   {"schema_version", "1.0"},
						   {"run_id", snapshot.run_id},
						   {"theme_revision", decimal(themes.devRevision())},
						   {"schema_generation", decimal(schema ? schema->generation : 0u)},
						   {"schema_fingerprint", decimal(schema ? schema->fingerprint : 0u)},
						   {"collection_available", visited || themes.devThemeCount() == 0u},
						   {"variants", std::move(context.themes)}});
	if (!snapshot.request.content.include_source_paths)
		for (auto& variant : theme_export.find("variants")->array)
			if (auto* fields = variant.find("fields"))
				for (auto& field : fields->array)
					field.set("source", Json{});
	return theme_export;
}

} // namespace

Result<Json> materialize_tree(const tooling::DevTreeSnapshot& tree, std::string_view run_id,
							  bool include_text, std::uint64_t maximum_bytes) noexcept {
	try {
		if (tree.strings.size() > maximum_bytes || tree.flow.nodes.size() > maximum_bytes / 128u)
			return unexpectedError(
				makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
		Json result = Json::make_object({{"schema", "FlowUi.Agentic.Tree"},
										 {"schema_version", "1.0"},
										 {"run_id", Json(run_id)},
										 {"window_id", decimal(tree.window)},
										 {"window_frame", decimal(tree.frameNumber)},
										 {"app_tick", decimal(tree.app_tick)},
										 {"ui_runtime_frame", decimal(tree.ui_runtime_frame)},
										 {"generation", decimal(tree.generation)},
										 {"complete", tree.stats.complete},
										 {"truncated", tree.stats.truncated},
										 {"text_policy", include_text ? "included" : "redacted"}});
		Json flow_nodes = Json::make_array();
		flow_nodes.array.reserve(tree.flow.nodes.size());
		for (std::size_t index = 0; index < tree.flow.nodes.size(); ++index) {
			const auto& node = tree.flow.nodes[index];
			Json entry = Json::make_object(
				{{"index", Json::number(index)},
				 {"parent", node_index(node.parent)},
				 {"first_child", node_index(node.firstChild)},
				 {"next_sibling", node_index(node.nextSibling)},
				 {"subtree_end_exclusive", Json::number(node.subtreeEnd)},
				 {"depth", Json::number(node.depth)},
				 {"definition_id", decimal(node.definition.value)},
				 {"instance_id", decimal(node.instance.value)},
				 {"debug_name", Json(tree.string(node.debugName))},
				 {"definition_name", Json(tree.string(node.definitionName))},
				 {"definition_type", Json(tree.string(node.definitionTypeToken))},
				 {"source",
				  source_site(tree.string(node.sourceFile), tree.string(node.sourceFunction),
							  node.sourceLine, node.sourceColumn)},
				 {"flags", Json::number(static_cast<std::uint32_t>(node.flags))}});
#if FLOW_UI_DEV_CAPTURE_CLAY
			entry.set("clay_root", node_index(node.clayRoot));
			Json direct_clay = Json::make_array();
			for (const auto clay_index :
				 tooling::directClayContribution(tree, static_cast<std::uint32_t>(index)))
				direct_clay.array.emplace_back(Json::number(clay_index));
			entry.set("direct_clay_nodes", std::move(direct_clay));
#endif
			flow_nodes.array.emplace_back(std::move(entry));
		}
		result.set("flow_nodes", std::move(flow_nodes));
		Json flow_roots = Json::make_array();
		for (const auto root : tree.flow.roots)
			flow_roots.array.emplace_back(Json::number(root));
		result.set("flow_roots", std::move(flow_roots));
#if FLOW_UI_DEV_CAPTURE_CLAY
		if (tree.clay.nodes.size() > maximum_bytes / 256u)
			return unexpectedError(
				makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
		Json clay_nodes = Json::make_array();
		clay_nodes.array.reserve(tree.clay.nodes.size());
		for (std::size_t index = 0; index < tree.clay.nodes.size(); ++index) {
			const auto& node = tree.clay.nodes[index];
			const auto& pointers = node.pointerPresence;
			clay_nodes.array.emplace_back(Json::make_object(
				{{"index", Json::number(index)},
				 {"parent", node_index(node.parent)},
				 {"first_child", node_index(node.firstChild)},
				 {"next_sibling", node_index(node.nextSibling)},
				 {"subtree_end_exclusive", Json::number(node.subtreeEnd)},
				 {"root_index", Json::number(node.rootIndex)},
				 {"clay_id", Json::number(node.clayId)},
				 {"name", Json(tree.string(node.idString))},
				 {"flow_owner", node_index(node.directFlowOwner)},
				 {"bounds",
				  bounds(node.bounds.x, node.bounds.y, node.bounds.width, node.bounds.height)},
				 {"dimensions", bounds(0.0f, 0.0f, node.dimensions.width, node.dimensions.height)},
				 {"minimum_dimensions",
				  bounds(0.0f, 0.0f, node.minDimensions.width, node.minDimensions.height)},
				 {"clip_clay_id", Json::number(node.clipClayId)},
				 {"clip_axes", Json::make_object({{"horizontal", node.declaration.clip.horizontal},
												  {"vertical", node.declaration.clip.vertical}})},
				 {"text_present", tooling::hasFlag(node.flags, tooling::DevClayNodeFlag::Text)},
				 {"text", include_text ? Json(tree.string(node.text)) : Json{}},
				 {"wrapped_line_count", Json::number(node.wrappedLineCount)},
				 {"flags", Json::number(static_cast<std::uint32_t>(node.flags))},
				 {"pointer_presence",
				  Json::make_object({{"image", pointers.imageData},
									 {"custom", pointers.customData},
									 {"user", pointers.userData},
									 {"text_user", pointers.textUserData},
									 {"transition_handler", pointers.transitionHandler},
									 {"transition_initial", pointers.transitionInitialState},
									 {"transition_final", pointers.transitionFinalState}})}}));
		}
		result.set("clay_nodes", std::move(clay_nodes));
		Json clay_roots = Json::make_array();
		for (const auto& root : tree.clay.roots)
			clay_roots.array.emplace_back(Json::make_object(
				{{"node", node_index(root.node)},
				 {"attachment_parent_clay_id", Json::number(root.attachmentParentClayId)},
				 {"attachment_parent", node_index(root.attachmentParent)},
				 {"clip_clay_id", Json::number(root.clipClayId)},
				 {"clip_node", node_index(root.clipNode)},
				 {"z_index", Json(static_cast<double>(root.zIndex))},
				 {"paint_order", Json::number(root.paintOrder)}}));
		result.set("clay_roots", std::move(clay_roots));
#else
		result.set("clay_status", "disabled");
#endif
		Json diagnostics = Json::make_array();
		for (const auto& diagnostic : tree.diagnostics) {
			Json entry = Json::make_object(
				{{"code", Json::number(static_cast<std::uint16_t>(diagnostic.code))},
				 {"flow_node", node_index(diagnostic.flow)},
				 {"expected", decimal(diagnostic.expected)},
				 {"observed", decimal(diagnostic.observed)}});
#if FLOW_UI_DEV_CAPTURE_CLAY
			entry.set("clay_node", node_index(diagnostic.clay));
#endif
			diagnostics.array.emplace_back(std::move(entry));
		}
		result.set("diagnostics", std::move(diagnostics));
		if (write_json(result).size() > maximum_bytes)
			return unexpectedError(
				makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
		return result;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticSnapshot));
	}
}

Status materialize_theme_revision(AgenticCaptureSnapshot& snapshot, DevTooling& tooling_service,
								  ThemeManager& themes) noexcept {
	try {
		const auto schema = tooling_service.schemas().view();
		const auto revision = themes.devRevision();
		const auto generation = schema ? schema->generation : 0u;
		const auto artifact_id = theme_revision_artifact_id(revision, generation);
		if (std::any_of(snapshot.artifacts.begin(), snapshot.artifacts.end(),
						[&](const auto& artifact) { return artifact.artifact_id == artifact_id; }))
			return {};
		auto document = owned_theme_document(snapshot, tooling_service, themes);
		const auto current_schema = tooling_service.schemas().view();
		if (themes.devRevision() != revision ||
			(current_schema ? current_schema->generation : 0u) != generation)
			return unexpectedError(
				makeError(ErrorCode::AgenticStaleGeneration, ErrorSite::AgenticSnapshot));
		const auto filename = std::to_string(revision) + "_" + std::to_string(generation) + ".json";
		return add_text_artifact(snapshot, artifact_id, "visual/themes/" + filename,
								 "application/json", "FlowUi.Agentic.Themes", write_json(document));
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticSnapshot));
	}
}

Status materialize_tooling(AgenticCaptureSnapshot& snapshot, DevTooling& tooling_service,
						   ThemeManager& themes) noexcept {
	try {
		Json theme_export = owned_theme_document(snapshot, tooling_service, themes);
		Status added =
			add_text_artifact(snapshot, "visual_themes", "visual/themes.json", "application/json",
							  "FlowUi.Agentic.Themes", write_json(theme_export));
		if (!added)
			return added;

		auto& catalogues = tooling_service.catalogues();
		std::uint64_t catalogue_records = 0;
		const auto admit_records = [&](std::size_t count) {
			if (count > snapshot.request.limits.maximum_records - catalogue_records)
				throw FlowUiException(
					makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
			catalogue_records += count;
		};
		Json catalogue = Json::make_object({{"schema", "FlowUi.Agentic.Catalogue"},
											{"schema_version", "1.0"},
											{"run_id", snapshot.run_id}});
		Json images = Json::make_array();
		const auto image_entries = catalogues.queryImages();
		admit_records(image_entries.size());
		for (const auto& entry : image_entries)
			images.array.emplace_back(Json::make_object(
				{{"key", resource_key(entry.key)},
				 {"source", source_path(entry.sourcePathOrDebugName)},
				 {"width", Json::number(entry.width)},
				 {"height", Json::number(entry.height)},
				 {"source_vk_format", Json::number(entry.formatVulkan)},
				 {"gpu_memory_bytes", decimal(entry.gpuMemoryBytes)},
				 {"texture", decimal(entry.textureHandle.packed())},
				 {"status", Json::number(static_cast<std::uint8_t>(entry.status))},
				 {"anonymous", entry.isAnonymous}}));
		catalogue.set("images", std::move(images));
		Json icons = Json::make_array();
		const auto icon_entries = catalogues.queryIcons();
		admit_records(icon_entries.size());
		for (const auto& entry : icon_entries)
			icons.array.emplace_back(
				Json::make_object({{"key", resource_key(entry.iconKey)},
								   {"name", Json(entry.iconName)},
								   {"width", Json::number(entry.targetWidth)},
								   {"height", Json::number(entry.targetHeight)},
								   {"texture", decimal(entry.atlasTexture.packed())},
								   {"vector_svg", entry.isVectorSvg},
								   {"atlas_layer", Json::number(entry.atlasLayer)},
								   {"uv", bounds(entry.atlasUv.x, entry.atlasUv.y,
												 entry.atlasUv.width, entry.atlasUv.height)}}));
		catalogue.set("icons", std::move(icons));
		Json fonts = Json::make_array();
		const auto font_entries = catalogues.queryFonts();
		admit_records(font_entries.size());
		for (const auto& entry : font_entries)
			fonts.array.emplace_back(Json::make_object(
				{{"font_id", decimal(entry.fontHandle)},
				 {"family_id", decimal(entry.familyHandle)},
				 {"family_name", Json(entry.familyName)},
				 {"face_name", Json(entry.faceName)},
				 {"source", source_path(entry.sourcePath)},
				 {"weight", Json::number(entry.weight)},
				 {"italic", entry.isItalic},
				 {"em_size_points", Json(static_cast<double>(entry.emSizePoints))},
				 {"ascent", Json(static_cast<double>(entry.ascent))},
				 {"descent", Json(static_cast<double>(entry.descent))},
				 {"line_gap", Json(static_cast<double>(entry.lineGap))},
				 {"glyphs", Json::number(entry.loadedGlyphCount)},
				 {"atlas_layer", Json::number(entry.atlasLayer)}}));
		catalogue.set("fonts", std::move(fonts));
		Json atlases = Json::make_array();
		const auto atlas_entries = catalogues.queryAtlases();
		admit_records(atlas_entries.size());
		for (const auto& entry : atlas_entries)
			atlases.array.emplace_back(Json::make_object(
				{{"key", resource_key(entry.atlasKey)},
				 {"name", Json(entry.debugName)},
				 {"kind", Json::number(static_cast<std::uint8_t>(entry.kind))},
				 {"width", Json::number(entry.width)},
				 {"height", Json::number(entry.height)},
				 {"layers", Json::number(entry.layerCount)},
				 {"texture", decimal(entry.atlasTexture.packed())},
				 {"gpu_memory_bytes", decimal(entry.gpuMemoryBytes)},
				 {"occupancy_ratio", Json(static_cast<double>(entry.occupancyRatio))},
				 {"msdf_pixel_range", Json(static_cast<double>(entry.msdfPixelRange))},
				 {"allocated_regions", Json::number(entry.allocatedRegions)}}));
		catalogue.set("atlases", std::move(atlases));
		Json actions = Json::make_array();
		const auto action_entries = catalogues.queryActions();
		admit_records(action_entries.size());
		for (const auto& entry : action_entries)
			actions.array.emplace_back(
				Json::make_object({{"stable_id", decimal(entry.stableId)},
								   {"name", Json(entry.debugName)},
								   {"kind", Json::number(static_cast<std::uint8_t>(entry.kind))},
								   {"bound", entry.isBound},
								   {"reconstructable", entry.isReconstructable},
								   {"availability_flags", Json::number(entry.availabilityFlags)},
								   {"callable_type_id", decimal(entry.callableTypeHash)},
								   {"result_type_id", decimal(entry.resultTypeHash)},
								   {"lifetime_invocations", decimal(entry.lifetimeInvocations)},
								   {"definition_source", source_path(entry.definitionSource)}}));
		catalogue.set("actions", std::move(actions));
		Json elements = Json::make_array();
		const auto element_entries = catalogues.queryElements(nullptr);
		admit_records(element_entries.size());
		for (const auto& entry : element_entries)
			elements.array.emplace_back(
				Json::make_object({{"definition_id", decimal(entry.definition.value)},
								   {"name", Json(entry.definitionName)},
								   {"parameters_type_id", decimal(entry.parametersTypeId)},
								   {"state_type_id", decimal(entry.stateTypeId)},
								   {"reflected_fields", Json::number(entry.reflectedFieldCount)},
								   {"bakeable", entry.isBakeable},
								   {"source", source_path(entry.headerSourceFile)}}));
		catalogue.set("elements", std::move(elements));
		const auto revisions = catalogues.revisions();
		catalogue.set("revisions",
					  Json::make_object({{"storage", decimal(revisions.storageRevision)},
										 {"font", decimal(revisions.fontRevision)},
										 {"schema", decimal(revisions.schemaRevision)},
										 {"tree", decimal(revisions.treeRevision)}}));
		if (!snapshot.request.content.include_source_paths)
			for (auto& section : catalogue.object)
				if (section.second.kind == Json::Kind::array)
					for (auto& entry : section.second.array) {
						if (entry.find("source"))
							entry.set("source", Json{});
						if (entry.find("definition_source"))
							entry.set("definition_source", Json{});
					}
		const auto text = write_json(catalogue);
		if (text.size() > snapshot.request.limits.maximum_snapshot_bytes)
			return unexpectedError(
				makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticSnapshot));
		return add_text_artifact(snapshot, "visual_catalogue", "visual/catalogue.json",
								 "application/json", "FlowUi.Agentic.Catalogue", text);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticSnapshot));
	}
}

} // namespace FlowUi::devSystems::agentic
