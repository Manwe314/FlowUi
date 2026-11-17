#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#include "devSystems/devTooling/bake/DevBakeInternals.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"

namespace {
thread_local std::ptrdiff_t allocationCountdown = -1;
thread_local const char* committedManifestPath = nullptr;
std::size_t destroyedPayloadCount = 0;

[[nodiscard]] bool rejectAllocation() noexcept {
	if (committedManifestPath) {
		if (std::FILE* manifest = std::fopen(committedManifestPath, "rb")) {
			std::fclose(manifest);
			return true;
		}
	}
	if (allocationCountdown < 0)
		return false;
	if (allocationCountdown == 0)
		return true;
	--allocationCountdown;
	return false;
}
} // namespace

// The test owns the executable allocator so library failure paths run under exhaustion.
void* operator new(std::size_t size) {
	if (rejectAllocation())
		throw std::bad_alloc{};
	if (void* allocation = std::malloc(size ? size : 1))
		return allocation;
	throw std::bad_alloc{};
}

void operator delete(void* allocation) noexcept { std::free(allocation); }
void operator delete(void* allocation, std::size_t) noexcept { std::free(allocation); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* allocation) noexcept { ::operator delete(allocation); }
void operator delete[](void* allocation, std::size_t) noexcept { ::operator delete(allocation); }

namespace {
using namespace FlowUi::devSystems::tooling;

void verifyStableValueTransfer() {
	DevTypeOps operations{
		.type = 42,
		.copyConstruct = [](const void* source, void* destination) noexcept {
			std::construct_at(static_cast<int*>(destination), *static_cast<const int*>(source));
			return DevValueOperationStatus::Success;
		},
		.moveConstruct = [](void* source, void* destination) noexcept {
			return source && destination ? DevValueOperationStatus::Failed
				: DevValueOperationStatus::NullSource;
		},
		.destroy = [](void* value) noexcept {
			std::destroy_at(static_cast<int*>(value));
			++destroyedPayloadCount;
		},
	};
	DevSchemaGeneration schema;
	schema.types.resize(2);
	schema.types[1] = {.id = operations.type, .size = sizeof(int), .alignment = alignof(int)};
	schema.typeOperations = {nullptr, &operations};
	const int source = 37;
	DevOwnedValue destination;
	destroyedPayloadCount = 0;
	assert(DevOwnedValue::copyFrom(schema, DevTypeIndex{1}, &source, destination) ==
		DevValueOperationStatus::Success);
	const void* stableAddress = destination.data();
	assert(destination.heapBytes() >= sizeof(source));
	DevOwnedValue moved(std::move(destination));
	assert(!destination && moved.data() == stableAddress);
	assert(*static_cast<const int*>(moved.data()) == source);
	moved.reset();
	assert(destroyedPayloadCount == 1);
}

struct PayloadCopyFailure { int identity = 73; };
struct ThrowingMovePayload {
	int value = 0;
	static inline bool failCopy = false;
	static inline std::size_t moveCount = 0;
	ThrowingMovePayload() = default;
	explicit ThrowingMovePayload(int initialValue) : value(initialValue) {}
	ThrowingMovePayload(const ThrowingMovePayload& other) : value(other.value) {
		if (failCopy) throw PayloadCopyFailure{};
	}
	ThrowingMovePayload(ThrowingMovePayload&&) {
		++moveCount;
		throw std::runtime_error("payload move must not be invoked");
	}
	ThrowingMovePayload& operator=(const ThrowingMovePayload&) = default;
};

struct ThrowingDestructorPayload {
	static inline bool failDestruction = false;
	~ThrowingDestructorPayload() noexcept(false) {
		if (failDestruction) throw PayloadCopyFailure{};
	}
};

void verifyThrowingPayloadOwnership() {
	DevSchemaRegistry schemas;
	schemas.ensureStruct<ThrowingMovePayload>();
	schemas.ensureStruct<ThrowingDestructorPayload>();
	assert(schemas.publishPendingAtSafePoint());
	DevOverrideEngine engine(schemas);
	ThrowingMovePayload source(17);
	DevOwnedValue owned;
	assert(engine.copyValue(source, owned) == DevValueOperationStatus::Success);
	const void* stableAddress = owned.data();
	std::vector<DevOwnedValue> values;
	values.emplace_back(std::move(owned));
	values.reserve(16);
	assert(!owned && values.front().data() == stableAddress);
	assert(ThrowingMovePayload::moveCount == 0);
	DevOwnedValue retained(values.front());
	ThrowingMovePayload::failCopy = true;
	bool copyRejected = false;
	try {
		retained = values.front();
	} catch (const PayloadCopyFailure& failure) {
		copyRejected = failure.identity == 73;
	}
	assert(copyRejected && retained &&
		static_cast<const ThrowingMovePayload*>(retained.data())->value == 17);
	assert(engine.copyValue(source, retained) == DevValueOperationStatus::Failed);
	assert(retained && static_cast<const ThrowingMovePayload*>(retained.data())->value == 17);
	ThrowingMovePayload::failCopy = false;
	ThrowingDestructorPayload unsupported;
	assert(engine.copyValue(unsupported, retained) == DevValueOperationStatus::Unsupported);
	assert(retained && static_cast<const ThrowingMovePayload*>(retained.data())->value == 17);
}

void verifyDroppedOverlayUpdate() {
	FlowUi::devSystems::DevTooling tooling;
	allocationCountdown = 0;
	tooling.setOverlaySelection(2, {});
	allocationCountdown = -1;
	assert(tooling.droppedOverlaySelectionUpdates() == 1);
	DevOverlaySelectionSpec selection;
	assert(!tooling.overlaySelection(2, selection));
	tooling.setOverlaySelection(2, {});
	assert(tooling.overlaySelection(2, selection));
}

struct FailureParameters { float amount = 1; };
struct FailureElement {
	using Parameters = FailureParameters;
	static constexpr FlowUi::FlowDefinitionID definitionId{0x7072656661696cull};
	// Reflection fixture: execution never enters the element's build hook.
	static void buildElement(FlowUi::ElementBuildContext<FailureElement>&) {}
};
FLOWUI_DEV_SCHEMA(FailureParameters, FLOWUI_DEV_FIELD(FailureParameters, amount))

void verifyPartialBatchAndCaptureFailure() {
	bool observedCommittedPrefix = false;
	for (std::ptrdiff_t failurePoint = 0; failurePoint < 64 && !observedCommittedPrefix; ++failurePoint) {
		DevSchemaRegistry schemas;
		schemas.ensureElement<FailureElement>();
		assert(schemas.publishPendingAtSafePoint());
		const auto schema = schemas.view();
		const auto* element = schema->findElement(FailureElement::definitionId);
		assert(element);
		const auto fields = schema->fieldsOf(element->parametersType);
		assert(fields.size() == 1);
		DevOverrideEngine engine(schemas);
		FlowUi::ThemeManager themes;
		DevOwnedValue replacement;
		assert(engine.copyValue(5.0f, replacement) == DevValueOperationStatus::Success);
		DevChangeSet change{.transaction = 81};
		change.commands.emplace_back(DevOverrideCommand{
			.kind = DevOverrideCommandKind::SetElementField,
			.element = {.definition = FailureElement::definitionId},
			.field = {.ownerType = schema->type(element->parametersType)->id, .field = fields.front().id},
			.value = std::move(replacement)});
		change.commands.emplace_back(DevOverrideCommand{.kind = DevOverrideCommandKind::ClearAllElements});
		assert(engine.submit(std::move(change)));
		allocationCountdown = failurePoint;
		engine.commitAtSafePoint(themes);
		allocationCountdown = -1;
		for (const auto& result : engine.commandResults()) {
			if (result.transaction != 81 || result.completedCommandCount != 1)
				continue;
			assert(!result.applied && result.command == 1 && result.attemptedCommandCount == 2);
			assert(result.mayHavePartialEffects && result.status == DevCommandStatus::InternalFailure);
			FailureParameters parameters;
			engine.applyElement(FailureElement::definitionId, 2, {.value = 3}, &parameters);
			assert(parameters.amount == 5);
			observedCommittedPrefix = true;
			DevBakePipeline pipeline(schemas, engine);
			for (std::size_t queryIndex = 0; queryIndex < 2; ++queryIndex) {
				bool queryRejected = false;
				allocationCountdown = 0;
				try {
					if (queryIndex == 0) {
						const auto status = pipeline.queryStatus();
						assert(status.activeLiveOverrideCount == 1);
					} else {
						const auto differences = pipeline.queryDiff();
						assert(!differences.empty());
					}
				} catch (const std::bad_alloc&) {
					queryRejected = true;
				}
				allocationCountdown = -1;
				assert(queryRejected);
			}
			engine.beginWindowFrame(2, 1);
			allocationCountdown = 0;
			engine.captureElement(FailureElement::definitionId, 2, {.value = 3}, 1, &parameters);
			allocationCountdown = -1;
			engine.endWindowFrame(2);
			assert(engine.elementSnapshot(2).elements.empty());
			assert(engine.elementSnapshot(2).droppedElementCount == 1);
			assert(engine.stats().droppedElementCaptures == 1);
			break;
		}
	}
	assert(observedCommittedPrefix);
}

void verifyQueueAccountingAfterAllocationFailure() {
	DevSchemaRegistry schemas;
	DevOverrideEngine engine(schemas, {.maximumPendingTransactions = 128,
		.maximumCommandsPerTransaction = 1, .maximumPendingCommands = 64});
	std::size_t acceptedCount = 0;
	bool failedAllocation = false;
	while (acceptedCount < 64 && !failedAllocation) {
		DevChangeSet change;
		change.commands.emplace_back(DevOverrideCommand{.kind = DevOverrideCommandKind::ClearAllElements});
		allocationCountdown = 0;
		try {
			assert(engine.submit(std::move(change)));
			++acceptedCount;
		} catch (const std::bad_alloc&) {
			failedAllocation = true;
		}
		allocationCountdown = -1;
	}
	assert(failedAllocation);
	while (acceptedCount < 64) {
		DevChangeSet change;
		change.commands.emplace_back(DevOverrideCommand{.kind = DevOverrideCommandKind::ClearAllElements});
		assert(engine.submit(std::move(change)));
		++acceptedCount;
	}
	DevChangeSet rejected;
	rejected.commands.emplace_back(DevOverrideCommand{.kind = DevOverrideCommandKind::ClearAllElements});
	assert(!engine.submit(std::move(rejected)));
}

void verifyFailureAcknowledgementUnderExhaustion() {
	DevSchemaRegistry schemas;
	DevOverrideEngine engine(schemas);
	FlowUi::ThemeManager themes;
	allocationCountdown = 0;
	engine.commitAtSafePoint(themes);
	allocationCountdown = -1;
	assert(!engine.commandResults().empty());
	assert(engine.commandResults().back().status == DevCommandStatus::InternalFailure);
}

void verifyAllocatingConstructionPropagates() {
	const FlowUi::devSystems::DevToolingConfig configuration;
	DevSchemaRegistry schemas;
	DevOverrideEngine engine(schemas);
	for (std::size_t constructorIndex = 0; constructorIndex < 3; ++constructorIndex) {
		bool rejected = false;
		allocationCountdown = 0;
		try {
			if (constructorIndex == 0) {
				FlowUi::devSystems::DevTooling tooling(configuration);
			} else if (constructorIndex == 1) {
				DevOverrideEngine attempted(schemas);
			} else {
				DevBakePipeline pipeline(schemas, engine);
			}
		} catch (const std::bad_alloc&) {
			rejected = true;
		}
		allocationCountdown = -1;
		assert(rejected);
	}
}

void verifyTombstonePreparationFailure() {
	for (std::size_t operationIndex = 0; operationIndex < 4; ++operationIndex) {
		DevOverrideApply assignments;
		DevElementOverrideTarget target{.definition = FlowUi::FlowDefinitionID{73},
			.window = 2, .instance = {.value = 11}, .scope = DevOverrideScope::ExactInstance};
		if (operationIndex == 1)
			target.scope = DevOverrideScope::Definition;
		assignments.set(target, {.ownerType = 1, .field = 2}, DevFieldIndex{1}, {},
			DevOverrideLayer::LiveInstance, {}, 1);
		assignments.rebuildCompiled();
		if (operationIndex == 0) {
			allocationCountdown = 0;
			bool rebuildRejected = false;
			try {
				assignments.rebuildCompiled();
			} catch (const std::bad_alloc&) {
				rebuildRejected = true;
			}
			allocationCountdown = -1;
			DevOverrideLayer retainedLayer{};
			assert(rebuildRejected && assignments.winningLayer(target.definition, target.window,
				target.instance, 2, retainedLayer));
		}
		bool rejected = false;
		allocationCountdown = 0;
		try {
			if (operationIndex == 0)
				assignments.clear(target, {.ownerType = 1, .field = 2}, DevOverrideLayer::LiveInstance);
			else if (operationIndex == 1)
				assignments.resetDefinition(target.definition);
			else if (operationIndex == 2)
				assignments.resetInstance(target);
			else
				assignments.clearAll();
		} catch (const std::bad_alloc&) {
			rejected = true;
		}
		allocationCountdown = -1;
		assert(rejected && assignments.records().size() == 1 && assignments.bakeTombstones().empty());
		assignments.clearAll();
		assert(assignments.records().empty() && assignments.bakeTombstones().size() == 1);
	}
}

void verifyManifestPublicationAfterDiskCommit(const std::filesystem::path& directory) {
	std::filesystem::create_directories(directory);
#if defined(__linux__)
	// /dev/full reliably rejects stream output after the temporary file has been opened.
	const auto failedDestination = directory / "failed-write.flowchanges";
	auto temporaryPath = failedDestination;
	temporaryPath += ".tmp";
	std::filesystem::remove(temporaryPath);
	std::filesystem::create_symlink("/dev/full", temporaryPath);
	assert(!bake_detail::write_manifest_atomically(failedDestination, "unwritten manifest"));
	assert(!std::filesystem::is_symlink(temporaryPath));
#endif
	const auto path = directory / "prepared-publication.flowchanges";
	std::filesystem::remove(path);
	DevSchemaRegistry schemas;
	DevOverrideEngine overrides(schemas);
	DevBakePipeline pipeline(schemas, overrides);
	pipeline.setManifestPath(path);
	allocationCountdown = 0;
	const auto unavailableReview = pipeline.query_review();
	allocationCountdown = -1;
	assert(!unavailableReview.can_commit && !unavailableReview.valid_disk &&
		   unavailableReview.counts.errors != 0);
	DevChangesReviewRequest request;
	request.prepared_memory.emplace();
	request.prepared_memory->targets.emplace_back(FlowUi::detail::MemoryCapacitySelection{
		.evidence = {.kind = "recording", .source_name = "postcommit failure regression", .dataset_id = 7},
		.key = "flowui.tuning.input_fields.registry_entries", .policy = "manual", .value = 128});
	const auto plan = pipeline.query_review(request);
	assert(plan.valid_disk && plan.can_commit);
	const std::string nativePath = path.string();
	committedManifestPath = nativePath.c_str();
	const auto result = pipeline.commit_review(plan);
	committedManifestPath = nullptr;
	assert(result.applied && result.status == DevCommandStatus::Applied);
	assert(std::filesystem::exists(path));
	assert(pipeline.manifest().memoryCapacity &&
		   pipeline.manifest().memoryCapacity->targets.front().value == 128);
}
} // namespace

int main(int argumentCount, char** arguments) {
	assert(argumentCount == 2);
	verifyStableValueTransfer();
	verifyThrowingPayloadOwnership();
	verifyDroppedOverlayUpdate();
	verifyPartialBatchAndCaptureFailure();
	verifyQueueAccountingAfterAllocationFailure();
	verifyFailureAcknowledgementUnderExhaustion();
	verifyAllocatingConstructionPropagates();
	verifyTombstonePreparationFailure();
	verifyManifestPublicationAfterDiskCommit(arguments[1]);
}
